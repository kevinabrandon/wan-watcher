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

// MCP-based bicolor LEDs - WAN1 (pins 0-1)
Led g_led_wan1_green(0, LedPinType::MCP, &g_mcp);
Led g_led_wan1_red(1, LedPinType::MCP, &g_mcp);

// MCP-based bicolor LEDs - WAN2 (pins 2-3)
Led g_led_wan2_green(2, LedPinType::MCP, &g_mcp);
Led g_led_wan2_red(3, LedPinType::MCP, &g_mcp);

// MCP-based bicolor LEDs - Local pinger (pins 4-5)
Led g_led_local_green(4, LedPinType::MCP, &g_mcp);
Led g_led_local_red(5, LedPinType::MCP, &g_mcp);

// GPIO-based LEDs
Led g_led_status1(4, LedPinType::GPIO);

// Router timeout tracking (monitors pfSense daemon connection)
static bool g_router_timed_out = false;

// Global brightness level (0-15, where 0 = off)
static uint8_t g_brightness = 8;
static bool g_displays_on = true;

// Physical power switch (optional)
static const uint8_t POWER_SWITCH_PIN = 13;  // MCP pin for toggle switch
static bool g_power_switch_enabled = false;
static bool g_power_switch_last_state = true;
static unsigned long g_power_switch_last_change_ms = 0;
static const unsigned long POWER_SWITCH_DEBOUNCE_MS = 50;

// Brightness potentiometer
BrightnessPotentiometer g_brightness_pot;

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
    if (!g_displays_on) return;  // All LEDs off when displays disabled
    switch (state) {
        case WanState::UP:
            g_led_wan1_green.set(true);
            g_led_wan1_red.set(false);
            Serial.println("WAN1 LED -> GREEN (UP)");
            break;

        case WanState::DEGRADED:
            g_led_wan1_green.set(true);
            g_led_wan1_red.set(true);  // Both on = yellow/amber
            Serial.println("WAN1 LED -> YELLOW (DEGRADED)");
            break;

        case WanState::DOWN:
        default:
            g_led_wan1_green.set(false);
            g_led_wan1_red.set(true);
            Serial.println("WAN1 LED -> RED (DOWN)");
            break;
    }
}

void wan2_set_leds(WanState state) {
    if (!g_displays_on) return;  // All LEDs off when displays disabled
    switch (state) {
        case WanState::UP:
            g_led_wan2_green.set(true);
            g_led_wan2_red.set(false);
            Serial.println("WAN2 LED -> GREEN (UP)");
            break;

        case WanState::DEGRADED:
            g_led_wan2_green.set(true);
            g_led_wan2_red.set(true);  // Both on = yellow/amber
            Serial.println("WAN2 LED -> YELLOW (DEGRADED)");
            break;

        case WanState::DOWN:
        default:
            g_led_wan2_green.set(false);
            g_led_wan2_red.set(true);
            Serial.println("WAN2 LED -> RED (DOWN)");
            break;
    }
}

void local_pinger_set_leds(WanState state) {
    if (!g_displays_on) return;  // All LEDs off when displays disabled
    switch (state) {
        case WanState::UP:
            g_led_local_green.set(true);
            g_led_local_red.set(false);
            break;

        case WanState::DEGRADED:
            g_led_local_green.set(true);
            g_led_local_red.set(true);  // Both on = yellow/amber
            break;

        case WanState::DOWN:
        default:
            g_led_local_green.set(false);
            g_led_local_red.set(true);
            break;
    }
}

// Helper to turn off all WAN LEDs (used during blink-off phase)
static void wan_leds_all_off() {
    g_led_wan1_green.set(false);
    g_led_wan1_red.set(false);
    g_led_wan2_green.set(false);
    g_led_wan2_red.set(false);
}

// Helper to turn off ALL indicator LEDs (used when displays are disabled)
// Note: Status LED is managed separately by loop() based on Ethernet state
static void all_leds_off() {
    g_led_wan1_green.set(false);
    g_led_wan1_red.set(false);
    g_led_wan2_green.set(false);
    g_led_wan2_red.set(false);
    g_led_local_green.set(false);
    g_led_local_red.set(false);
}

void router_heartbeat_check() {
    if (!g_displays_on) return;  // Skip when displays disabled

    const WanMetrics& m = wan_metrics_get(1);
    bool is_stale = false;

    if (m.last_update_ms == 0) {
        // Never received an update - treat as stale
        is_stale = true;
    } else {
        unsigned long elapsed = millis() - m.last_update_ms;
        is_stale = (elapsed > FRESHNESS_RED_BUFFER_END_MS);
    }

    if (is_stale) {
        // Log once when entering stale state
        if (!g_router_timed_out) {
            g_router_timed_out = true;
            Serial.println("Router timeout -> blinking WANs DOWN");
        }

        // Sync WAN LED blinking with freshness bar
        if (g_freshness_bar.isBlinkOn()) {
            // Blink on: show DOWN state (red only)
            g_led_wan1_green.set(false);
            g_led_wan1_red.set(true);
            g_led_wan2_green.set(false);
            g_led_wan2_red.set(true);
        } else {
            // Blink off: all LEDs off
            wan_leds_all_off();
        }
    } else {
        // Reset timeout flag when we have fresh data
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

    // Initialize all LEDs (bicolor: green/red pairs)
    g_led_wan1_green.begin();
    g_led_wan1_red.begin();
    g_led_wan2_green.begin();
    g_led_wan2_red.begin();
    g_led_local_green.begin();
    g_led_local_red.begin();
    g_led_status1.begin();

    // Reset timeout state
    g_router_timed_out = false;

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

    // Initialize all LEDs (bicolor: green/red pairs)
    g_led_wan1_green.begin();
    g_led_wan1_red.begin();
    g_led_wan2_green.begin();
    g_led_wan2_red.begin();
    g_led_local_green.begin();
    g_led_local_red.begin();
    g_led_status1.begin();

    // Reset timeout state
    g_router_timed_out = false;
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

void set_display_brightness(uint8_t brightness) {
    if (brightness > 15) brightness = 15;
    g_brightness = brightness;

    // Apply brightness directly (0-15 maps to HT16K33 0-15)
    g_display_manager.setBrightness(brightness);
    g_freshness_bar.setBrightness(brightness);

    // Apply to legacy display if in use
    if (g_display_ok && !g_use_display_manager) {
        g_display.setBrightness(brightness);
    }
}

uint8_t get_display_brightness() {
    return g_brightness;
}

void set_displays_on(bool on) {
    if (on == g_displays_on) return;  // No change

    g_displays_on = on;
    g_display_manager.setDisplayOn(on);
    g_freshness_bar.setDisplayOn(on);

    if (!on) {
        // Turn off all LEDs when displays are disabled
        all_leds_off();
    } else {
        // Restore WAN LED states from current metrics
        const WanMetrics& m1 = wan_metrics_get(1);
        const WanMetrics& m2 = wan_metrics_get(2);
        wan1_set_leds(m1.state);
        wan2_set_leds(m2.state);
        // Local pinger LEDs are restored by the normal loop() update cycle
        // Status LED is managed by loop() based on Ethernet state
    }
}

bool get_displays_on() {
    return g_displays_on;
}

void power_switch_init() {
    // Configure MCP pin as input with pullup
    g_mcp.pinMode(POWER_SWITCH_PIN, INPUT_PULLUP);

    // Read initial state (switch closed = LOW = displays on)
    g_power_switch_last_state = g_mcp.digitalRead(POWER_SWITCH_PIN) == LOW;
    g_power_switch_last_change_ms = millis();
    g_power_switch_enabled = true;

    // Set initial display state to match switch
    set_displays_on(g_power_switch_last_state);

    Serial.printf("Power switch initialized on MCP pin %d, state: %s\n",
                  POWER_SWITCH_PIN, g_power_switch_last_state ? "ON" : "OFF");
}

void power_switch_update() {
    if (!g_power_switch_enabled) return;

    unsigned long now = millis();

    // Debounce: ignore reads too soon after last change
    if (now - g_power_switch_last_change_ms < POWER_SWITCH_DEBOUNCE_MS) {
        return;
    }

    // Read current state (switch closed = LOW = displays on)
    bool current_state = g_mcp.digitalRead(POWER_SWITCH_PIN) == LOW;

    // Detect state change
    if (current_state != g_power_switch_last_state) {
        g_power_switch_last_state = current_state;
        g_power_switch_last_change_ms = now;

        // Physical switch change overrides current state
        set_displays_on(current_state);
        Serial.printf("Power switch toggled: %s\n", current_state ? "ON" : "OFF");
    }
}

bool get_power_switch_position() {
    return g_power_switch_last_state;
}

uint8_t get_brightness_pot_level() {
    return g_brightness_pot.getPotLevel();
}
