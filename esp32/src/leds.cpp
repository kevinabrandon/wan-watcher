// leds.cpp
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include <Adafruit_LEDBackpack.h>
#include "leds.h"

// I2C pins for Olimex ESP32-POE-ISO
static const int I2C_SDA = 13;
static const int I2C_SCL = 16;
static const uint8_t MCP23017_ADDR = 0x20;
static const uint8_t DISPLAY_ADDR = 0x71;

// MCP23017 expander
static Adafruit_MCP23X17 g_mcp;

// 7-segment display (legacy single display mode)
static Adafruit_7segment g_display;
static bool g_display_ok = false;

// Display manager and button handlers
DisplayManager g_display_manager;
ButtonHandler g_button_handler_packet;
ButtonHandler g_button_handler_bandwidth;
FreshnessBar g_freshness_bar;
static bool g_use_display_manager = false;

// MCP-based LEDs - WAN1 (pins 0-2)
Led g_led_wan1_up(0, LedPinType::MCP, &g_mcp);
Led g_led_wan1_degraded(1, LedPinType::MCP, &g_mcp);
Led g_led_wan1_down(2, LedPinType::MCP, &g_mcp);

// MCP-based LEDs - WAN2 (pins 3-5)
Led g_led_wan2_up(3, LedPinType::MCP, &g_mcp);
Led g_led_wan2_degraded(4, LedPinType::MCP, &g_mcp);
Led g_led_wan2_down(5, LedPinType::MCP, &g_mcp);

// MCP-based LEDs - Local pinger (pins 6-8)
Led g_led_local_up(6, LedPinType::MCP, &g_mcp);
Led g_led_local_degraded(7, LedPinType::MCP, &g_mcp);
Led g_led_local_down(8, LedPinType::MCP, &g_mcp);

// GPIO-based LEDs
Led g_led_status1(4, LedPinType::GPIO);
Led g_led_heartbeat(5, LedPinType::GPIO);

// Heartbeat tracking (monitors pfSense daemon connection)
static bool g_router_timed_out = false;
static const unsigned long ROUTER_TIMEOUT_MS = 45UL * 1000UL; // 45 seconds

// Heartbeat LED blink bookkeeping
static unsigned long g_hb_last_toggle_ms = 0;
static bool g_hb_led_on = false;

// Button callback functions for packet display
static void on_packet_short_press() {
    g_display_manager.advancePacketMetric();
}

static void on_packet_long_press() {
    g_display_manager.togglePacketAutoCycle();
}

// Button callback functions for bandwidth display
static void on_bandwidth_short_press() {
    g_display_manager.advanceBandwidthMetric();
}

static void on_bandwidth_long_press() {
    g_display_manager.toggleBandwidthAutoCycle();
}

void wan1_set_leds(WanState state) {
    switch (state) {
        case WanState::UP:
            g_led_wan1_up.set(true);
            g_led_wan1_degraded.set(false);
            g_led_wan1_down.set(false);
            Serial.println("WAN1 LEDs -> UP");
            break;

        case WanState::DEGRADED:
            g_led_wan1_up.set(false);
            g_led_wan1_degraded.set(true);
            g_led_wan1_down.set(false);
            Serial.println("WAN1 LEDs -> DEGRADED");
            break;

        case WanState::DOWN:
        default:
            g_led_wan1_up.set(false);
            g_led_wan1_degraded.set(false);
            g_led_wan1_down.set(true);
            Serial.println("WAN1 LEDs -> DOWN");
            break;
    }
}

void wan2_set_leds(WanState state) {
    switch (state) {
        case WanState::UP:
            g_led_wan2_up.set(true);
            g_led_wan2_degraded.set(false);
            g_led_wan2_down.set(false);
            Serial.println("WAN2 LEDs -> UP");
            break;

        case WanState::DEGRADED:
            g_led_wan2_up.set(false);
            g_led_wan2_degraded.set(true);
            g_led_wan2_down.set(false);
            Serial.println("WAN2 LEDs -> DEGRADED");
            break;

        case WanState::DOWN:
        default:
            g_led_wan2_up.set(false);
            g_led_wan2_degraded.set(false);
            g_led_wan2_down.set(true);
            Serial.println("WAN2 LEDs -> DOWN");
            break;
    }
}

void local_pinger_set_leds(WanState state) {
    switch (state) {
        case WanState::UP:
            g_led_local_up.set(true);
            g_led_local_degraded.set(false);
            g_led_local_down.set(false);
            break;

        case WanState::DEGRADED:
            g_led_local_up.set(false);
            g_led_local_degraded.set(true);
            g_led_local_down.set(false);
            break;

        case WanState::DOWN:
        default:
            g_led_local_up.set(false);
            g_led_local_degraded.set(false);
            g_led_local_down.set(true);
            break;
    }
}

// internal: set heartbeat LED state (no logging spam)
static void heartbeat_set(bool on) {
    g_hb_led_on = on;
    g_led_heartbeat.set(on);
}

static void heartbeat_update_pattern(unsigned long now, unsigned long elapsed_ms) {
    // Very recent (< 15s since last update): LED OFF
    if (elapsed_ms < 15UL * 1000UL) {
        heartbeat_set(false);
        return;
    }

    // Determine blink interval based on staleness
    // 15-30s: slow blink (500ms), 30-45s: medium blink (250ms), >45s: fast blink (100ms)
    unsigned long interval_ms;
    if (elapsed_ms < 30UL * 1000UL) {
        interval_ms = 500UL;
    } else if (elapsed_ms < 45UL * 1000UL) {
        interval_ms = 250UL;
    } else {
        interval_ms = 100UL;
    }

    if (now - g_hb_last_toggle_ms >= interval_ms) {
        g_hb_last_toggle_ms = now;
        heartbeat_set(!g_hb_led_on);
    }
}

void router_heartbeat_check() {
    unsigned long now = millis();
    const WanMetrics& m = wan_metrics_get(1);

    if (m.last_update_ms == 0) {
        // Never received an update: indicate "no heartbeat yet" with solid ON
        heartbeat_set(true);
        return;
    }

    unsigned long elapsed = now - m.last_update_ms;

    // Update heartbeat LED pattern first
    heartbeat_update_pattern(now, elapsed);

    // Timeout → force all WANs DOWN (daemon sends both in single batch)
    if (elapsed > ROUTER_TIMEOUT_MS) {
        if (!g_router_timed_out) {
            g_router_timed_out = true;
            Serial.println("Router heartbeat timeout -> forcing all WANs DOWN");
            wan1_set_leds(WanState::DOWN);
            wan2_set_leds(WanState::DOWN);
        }
    } else {
        // Reset timeout flag when we get updates
        g_router_timed_out = false;
    }
}

void freshness_bar_update() {
    if (!g_freshness_bar.isReady()) return;

    const WanMetrics& m = wan_metrics_get(1);

    if (m.last_update_ms == 0) {
        // Never received an update
        g_freshness_bar.update(0, true);
        return;
    }

    unsigned long elapsed = millis() - m.last_update_ms;
    g_freshness_bar.update(elapsed, false);
}

void leds_init() {
    // Initialize I2C for MCP23017
    Wire.begin(I2C_SDA, I2C_SCL);

    if (!g_mcp.begin_I2C(MCP23017_ADDR, &Wire)) {
        Serial.println("ERROR: MCP23017 not found!");
    } else {
        Serial.println("MCP23017 initialized");
        // Immediately clear all 16 MCP pins
        for (int i = 0; i < 16; i++) {
            g_mcp.pinMode(i, OUTPUT);
            g_mcp.digitalWrite(i, LOW);
        }
    }

    // Initialize 7-segment display
    if (!g_display.begin(DISPLAY_ADDR, &Wire)) {
        Serial.println("ERROR: 7-segment display not found!");
        g_display_ok = false;
    } else {
        Serial.println("7-segment display initialized");
        g_display_ok = true;
        g_display.clear();
        g_display.writeDisplay();
        g_display.setBrightness(8);
    }

    // Initialize all LEDs
    g_led_wan1_up.begin();
    g_led_wan1_degraded.begin();
    g_led_wan1_down.begin();
    g_led_wan2_up.begin();
    g_led_wan2_degraded.begin();
    g_led_wan2_down.begin();
    g_led_local_up.begin();
    g_led_local_degraded.begin();
    g_led_local_down.begin();
    g_led_status1.begin();
    g_led_heartbeat.begin();

    // Reset heartbeat state
    g_router_timed_out = false;
    g_hb_last_toggle_ms = 0;

    // Not using display manager in legacy mode
    g_use_display_manager = false;
}

void leds_init_with_displays(const DisplaySystemConfig& config) {
    // Initialize I2C for MCP23017
    Wire.begin(I2C_SDA, I2C_SCL);

    if (!g_mcp.begin_I2C(MCP23017_ADDR, &Wire)) {
        Serial.println("ERROR: MCP23017 not found!");
    } else {
        Serial.println("MCP23017 initialized");
        // Immediately clear all 16 MCP pins
        for (int i = 0; i < 16; i++) {
            g_mcp.pinMode(i, OUTPUT);
            g_mcp.digitalWrite(i, LOW);
        }
    }

    // Initialize display manager (handles all 7-segment displays)
    g_display_manager.begin(config, &g_mcp, &Wire);
    g_use_display_manager = true;

    // Initialize freshness bar (bicolor LED bargraph)
    g_freshness_bar.begin(FRESHNESS_BAR_ADDR, &Wire);

    // Initialize packet button handler if configured
    if (config.button1_type != ButtonPinSource::NONE && config.button1_pin != 0) {
        ButtonPinType btn_type = (config.button1_type == ButtonPinSource::MCP)
                                 ? ButtonPinType::MCP : ButtonPinType::GPIO;
        Adafruit_MCP23X17* btn_mcp = (config.button1_type == ButtonPinSource::MCP)
                                     ? &g_mcp : nullptr;
        g_button_handler_packet.begin(config.button1_pin, btn_type, btn_mcp);
        g_button_handler_packet.onShortPress(on_packet_short_press);
        g_button_handler_packet.onLongPress(on_packet_long_press);
        g_button_handler_packet.setLongPressThreshold(config.long_press_ms);
    }

    // Initialize bandwidth button handler if configured
    if (config.button2_type != ButtonPinSource::NONE && config.button2_pin != 0) {
        ButtonPinType btn_type = (config.button2_type == ButtonPinSource::MCP)
                                 ? ButtonPinType::MCP : ButtonPinType::GPIO;
        Adafruit_MCP23X17* btn_mcp = (config.button2_type == ButtonPinSource::MCP)
                                     ? &g_mcp : nullptr;
        g_button_handler_bandwidth.begin(config.button2_pin, btn_type, btn_mcp);
        g_button_handler_bandwidth.onShortPress(on_bandwidth_short_press);
        g_button_handler_bandwidth.onLongPress(on_bandwidth_long_press);
        g_button_handler_bandwidth.setLongPressThreshold(config.long_press_ms);
    }

    // Initialize all LEDs
    g_led_wan1_up.begin();
    g_led_wan1_degraded.begin();
    g_led_wan1_down.begin();
    g_led_wan2_up.begin();
    g_led_wan2_degraded.begin();
    g_led_wan2_down.begin();
    g_led_local_up.begin();
    g_led_local_degraded.begin();
    g_led_local_down.begin();
    g_led_status1.begin();
    g_led_heartbeat.begin();

    // Reset heartbeat state
    g_router_timed_out = false;
    g_hb_last_toggle_ms = 0;
}

void display_update() {
    // Use display manager if active
    if (g_use_display_manager) {
        g_button_handler_packet.update();
        g_button_handler_bandwidth.update();
        g_display_manager.update();
        return;
    }

    // Legacy single display mode
    if (!g_display_ok) return;

    const WanMetrics& m = wan_metrics_get(1);

    if (m.last_update_ms == 0) {
        // Never updated - show actual dashes (segment G = 0x40)
        g_display.writeDigitRaw(0, 0x40);
        g_display.writeDigitRaw(1, 0x40);
        g_display.writeDigitRaw(3, 0x40);  // position 2 is the colon
        g_display.writeDigitRaw(4, 0x40);
    } else {
        unsigned long elapsed_secs = (millis() - m.last_update_ms) / 1000UL;
        if (elapsed_secs > 9999) {
            elapsed_secs = 9999;  // cap at 4 digits
        }
        g_display.print((int)elapsed_secs, DEC);
    }
    g_display.writeDisplay();
}
