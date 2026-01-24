// display_config.h
// Configuration for multi-display 7-segment metrics system
#pragma once

#include <Arduino.h>

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

// Maximum displays supported (2 WANs x 2 displays each + 2 local: packet + bandwidth)
static const uint8_t MAX_DISPLAYS = 6;

// Local pinger display I2C addresses
static const uint8_t LOCAL_PINGER_DISPLAY_ADDR = 0x75;  // Local packet (L/J/P)
static const uint8_t LOCAL_BW_DISPLAY_ADDR = 0x76;      // Local bandwidth (sum of WANs)

// Metrics data source (which device provides the data)
enum class MetricsSource : uint8_t {
    WAN1 = 1,
    WAN2 = 2,
    LOCAL_PINGER = 0  // Uses local_pinger_get() instead of wan_metrics_get()
};

// Configuration structure
struct DisplaySystemConfig {
    // Cycle timing
    unsigned long cycle_interval_ms = 5000;  // 5 seconds
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
};
