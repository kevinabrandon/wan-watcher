// display_manager.h
// Central coordinator for all displays, cycling, and synchronization
#pragma once

#include <Arduino.h>
#include <Adafruit_MCP23X17.h>
#include "display_config.h"
#include "metric_display.h"
#include "led.h"

class DisplayManager {
public:
    DisplayManager();

    // Initialize all displays and indicator LEDs
    void begin(const DisplaySystemConfig& config, Adafruit_MCP23X17* mcp, TwoWire* wire = &Wire);

    // Call from loop() - handles cycling, rendering
    void update();

    // Button actions (separate for packet and bandwidth)
    void advancePacketMetric();      // Packet button short press
    void advanceBandwidthMetric();   // Bandwidth button short press
    void togglePacketAutoCycle();    // Packet button long press
    void toggleBandwidthAutoCycle(); // Bandwidth button long press

    // Configuration access
    DisplayMode displayMode() const;
    void setDisplayMode(DisplayMode mode);

    bool isPacketAutoCycleEnabled() const;
    bool isBandwidthAutoCycleEnabled() const;
    void setPacketAutoCycleEnabled(bool enabled);
    void setBandwidthAutoCycleEnabled(bool enabled);

    // Get current metric indices
    PacketMetric currentPacketMetric() const;
    BandwidthMetric currentBandwidthMetric() const;

    // Get number of active displays
    uint8_t activeDisplayCount() const;

    // Check if specific display is available
    bool isDisplayReady(int wan_id, DisplayType type) const;

private:
    DisplaySystemConfig _config;

    // Displays array: [wan1_packet, wan1_bw, wan2_packet, wan2_bw]
    MetricDisplay _displays[MAX_DISPLAYS];
    uint8_t _active_count;

    // Indicator LEDs (created if mode is INDICATOR_LED)
    Led* _led_latency;
    Led* _led_jitter;
    Led* _led_loss;
    Led* _led_download;
    Led* _led_upload;
    Adafruit_MCP23X17* _mcp;

    // Cycling state (shared timer keeps displays in sync)
    unsigned long _last_cycle_ms;
    PacketMetric _current_packet_metric;
    BandwidthMetric _current_bw_metric;
    bool _packet_auto_cycle;
    bool _bw_auto_cycle;

    // Internal methods
    void cyclePacketMetric();
    void cycleBandwidthMetric();
    void syncAllDisplayMetrics();
    void updateIndicatorLeds();
    void renderAllDisplays();

    // Helper to get display index
    int displayIndex(int wan_id, DisplayType type) const;
};
