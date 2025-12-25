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
static Adafruit_MCP23X17 mcp;

// 7-segment display (legacy single display mode)
static Adafruit_7segment display;
static bool g_display_ok = false;

// Display manager and button handlers
DisplayManager display_manager;
ButtonHandler button_handler_packet;
ButtonHandler button_handler_bandwidth;
static bool g_use_display_manager = false;

// MCP-based LEDs (pins 0-2)
Led led_wan1_up(0, LedPinType::MCP, &mcp);
Led led_wan1_degraded(1, LedPinType::MCP, &mcp);
Led led_wan1_down(2, LedPinType::MCP, &mcp);

// GPIO-based LEDs
Led led_status1(4, LedPinType::GPIO);
Led led_heartbeat(5, LedPinType::GPIO);

// Heartbeat tracking
static bool g_wan1_timed_out = false;
static const unsigned long WAN1_TIMEOUT_MS = 3UL * 60UL * 1000UL; // 3 minutes

// Heartbeat LED blink bookkeeping
static unsigned long g_hb_last_toggle_ms = 0;
static bool g_hb_led_on = false;

// Button callback functions for packet display
static void on_packet_short_press() {
    display_manager.advancePacketMetric();
}

static void on_packet_long_press() {
    display_manager.togglePacketAutoCycle();
}

// Button callback functions for bandwidth display
static void on_bandwidth_short_press() {
    display_manager.advanceBandwidthMetric();
}

static void on_bandwidth_long_press() {
    display_manager.toggleBandwidthAutoCycle();
}

void wan1_set_leds(WanState state) {
    switch (state) {
        case WanState::UP:
            led_wan1_up.set(true);
            led_wan1_degraded.set(false);
            led_wan1_down.set(false);
            Serial.println("WAN1 LEDs -> UP");
            break;

        case WanState::DEGRADED:
            led_wan1_up.set(false);
            led_wan1_degraded.set(true);
            led_wan1_down.set(false);
            Serial.println("WAN1 LEDs -> DEGRADED");
            break;

        case WanState::DOWN:
        default:
            led_wan1_up.set(false);
            led_wan1_degraded.set(false);
            led_wan1_down.set(true);
            Serial.println("WAN1 LEDs -> DOWN");
            break;
    }
}

// internal: set heartbeat LED state (no logging spam)
static void heartbeat_set(bool on) {
    g_hb_led_on = on;
    led_heartbeat.set(on);
}

static void heartbeat_update_pattern(unsigned long now, unsigned long elapsed_ms) {
    // Very recent (< 45s since last update): LED OFF
    if (elapsed_ms < 45UL * 1000UL) {
        heartbeat_set(false);
        return;
    }

    // >= timeout (3 min): solid ON
    if (elapsed_ms >= WAN1_TIMEOUT_MS) {
        heartbeat_set(true);
        return;
    }

    // 45–90s: slow blink, 90–180s: fast blink
    unsigned long interval_ms;
    if (elapsed_ms < 90UL * 1000UL) {
        interval_ms = 500UL;
    } else {
        interval_ms = 200UL;
    }

    if (now - g_hb_last_toggle_ms >= interval_ms) {
        g_hb_last_toggle_ms = now;
        heartbeat_set(!g_hb_led_on);
    }
}

void wan1_heartbeat_check() {
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

    // Timeout → force WAN1 DOWN
    if (elapsed > WAN1_TIMEOUT_MS) {
        if (!g_wan1_timed_out) {
            g_wan1_timed_out = true;
            Serial.println("WAN1 heartbeat timeout -> forcing DOWN");
            wan1_set_leds(WanState::DOWN);
        }
    } else {
        // Reset timeout flag when we get updates
        g_wan1_timed_out = false;
    }
}

void leds_init() {
    // Initialize I2C for MCP23017
    Wire.begin(I2C_SDA, I2C_SCL);

    if (!mcp.begin_I2C(MCP23017_ADDR, &Wire)) {
        Serial.println("ERROR: MCP23017 not found!");
    } else {
        Serial.println("MCP23017 initialized");
        // Immediately clear all 16 MCP pins
        for (int i = 0; i < 16; i++) {
            mcp.pinMode(i, OUTPUT);
            mcp.digitalWrite(i, LOW);
        }
    }

    // Initialize 7-segment display
    if (!display.begin(DISPLAY_ADDR, &Wire)) {
        Serial.println("ERROR: 7-segment display not found!");
        g_display_ok = false;
    } else {
        Serial.println("7-segment display initialized");
        g_display_ok = true;
        display.clear();
        display.writeDisplay();
        display.setBrightness(8);
    }

    // Initialize GPIO-based LEDs
    led_status1.begin();
    led_heartbeat.begin();

    // Reset heartbeat state
    g_wan1_timed_out = false;
    g_hb_last_toggle_ms = 0;

    // Not using display manager in legacy mode
    g_use_display_manager = false;
}

void leds_init_with_displays(const DisplaySystemConfig& config) {
    // Initialize I2C for MCP23017
    Wire.begin(I2C_SDA, I2C_SCL);

    if (!mcp.begin_I2C(MCP23017_ADDR, &Wire)) {
        Serial.println("ERROR: MCP23017 not found!");
    } else {
        Serial.println("MCP23017 initialized");
        // Immediately clear all 16 MCP pins
        for (int i = 0; i < 16; i++) {
            mcp.pinMode(i, OUTPUT);
            mcp.digitalWrite(i, LOW);
        }
    }

    // Initialize display manager (handles all 7-segment displays)
    display_manager.begin(config, &mcp, &Wire);
    g_use_display_manager = true;

    // Initialize packet button handler if configured
    if (config.button1_type != ButtonPinSource::NONE && config.button1_pin != 0) {
        ButtonPinType btn_type = (config.button1_type == ButtonPinSource::MCP)
                                 ? ButtonPinType::MCP : ButtonPinType::GPIO;
        Adafruit_MCP23X17* btn_mcp = (config.button1_type == ButtonPinSource::MCP)
                                     ? &mcp : nullptr;
        button_handler_packet.begin(config.button1_pin, btn_type, btn_mcp);
        button_handler_packet.onShortPress(on_packet_short_press);
        button_handler_packet.onLongPress(on_packet_long_press);
        button_handler_packet.setLongPressThreshold(config.long_press_ms);
    }

    // Initialize bandwidth button handler if configured
    if (config.button2_type != ButtonPinSource::NONE && config.button2_pin != 0) {
        ButtonPinType btn_type = (config.button2_type == ButtonPinSource::MCP)
                                 ? ButtonPinType::MCP : ButtonPinType::GPIO;
        Adafruit_MCP23X17* btn_mcp = (config.button2_type == ButtonPinSource::MCP)
                                     ? &mcp : nullptr;
        button_handler_bandwidth.begin(config.button2_pin, btn_type, btn_mcp);
        button_handler_bandwidth.onShortPress(on_bandwidth_short_press);
        button_handler_bandwidth.onLongPress(on_bandwidth_long_press);
        button_handler_bandwidth.setLongPressThreshold(config.long_press_ms);
    }

    // Initialize GPIO-based LEDs
    led_status1.begin();
    led_heartbeat.begin();

    // Reset heartbeat state
    g_wan1_timed_out = false;
    g_hb_last_toggle_ms = 0;
}

void display_update() {
    // Use display manager if active
    if (g_use_display_manager) {
        button_handler_packet.update();
        button_handler_bandwidth.update();
        display_manager.update();
        return;
    }

    // Legacy single display mode
    if (!g_display_ok) return;

    const WanMetrics& m = wan_metrics_get(1);

    if (m.last_update_ms == 0) {
        // Never updated - show actual dashes (segment G = 0x40)
        display.writeDigitRaw(0, 0x40);
        display.writeDigitRaw(1, 0x40);
        display.writeDigitRaw(3, 0x40);  // position 2 is the colon
        display.writeDigitRaw(4, 0x40);
    } else {
        unsigned long elapsed_secs = (millis() - m.last_update_ms) / 1000UL;
        if (elapsed_secs > 9999) {
            elapsed_secs = 9999;  // cap at 4 digits
        }
        display.print((int)elapsed_secs, DEC);
    }
    display.writeDisplay();
}
