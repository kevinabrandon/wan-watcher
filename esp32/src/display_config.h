// display_config.h
// Configuration for multi-display 7-segment metrics system
#pragma once

#include <Arduino.h>

// Display mode: how to show metric indicator
enum class DisplayMode {
    PREFIX_LETTER,   // First digit shows L/J/P or d/U, 3 digits for value
    INDICATOR_LED    // Full 4 digits for value, separate LEDs indicate metric
};

// Packet metrics (displayed on packet display)
enum class PacketMetric : uint8_t {
    LATENCY = 0,  // L
    JITTER  = 1,  // J
    LOSS    = 2   // P (packet loss %)
};

// Bandwidth metrics (displayed on bandwidth display)
enum class BandwidthMetric : uint8_t {
    DOWNLOAD = 0,  // d
    UPLOAD   = 1   // U
};

// Display type
enum class DisplayType {
    PACKET,      // Shows latency/jitter/loss
    BANDWIDTH    // Shows download/upload
};

// Button pin type
enum class ButtonPinSource {
    NONE,   // Button disabled
    GPIO,   // ESP32 GPIO pin
    MCP     // MCP23017 pin
};

// Number of metrics per display type
static const uint8_t PACKET_METRIC_COUNT = 3;
static const uint8_t BANDWIDTH_METRIC_COUNT = 2;

// Maximum displays supported (2 WANs x 2 displays each)
static const uint8_t MAX_DISPLAYS = 4;

// Configuration structure
struct DisplaySystemConfig {
    // Display mode
    DisplayMode mode = DisplayMode::PREFIX_LETTER;

    // Cycle timing
    unsigned long cycle_interval_ms = 3000;  // 3 seconds
    bool auto_cycle_enabled = true;

    // I2C base address for displays
    // Layout: base+0=wan1_packet, base+1=wan1_bw, base+2=wan2_packet, base+3=wan2_bw
    uint8_t base_address = 0x71;

    // Button configuration (two buttons for independent control)
    // Button 1: controls packet display (L/J/P)
    ButtonPinSource button1_type = ButtonPinSource::NONE;
    uint8_t button1_pin = 0;
    // Button 2: controls bandwidth display (d/U)
    ButtonPinSource button2_type = ButtonPinSource::NONE;
    uint8_t button2_pin = 0;

    // Long press threshold
    unsigned long long_press_ms = 1000;

    // Indicator LED MCP pins (only used in INDICATOR_LED mode)
    uint8_t led_latency_pin = 8;
    uint8_t led_jitter_pin = 9;
    uint8_t led_loss_pin = 10;
    uint8_t led_download_pin = 11;
    uint8_t led_upload_pin = 12;
};
