// metric_display.h
// Single 7-segment display wrapper with metric formatting
#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_LEDBackpack.h>
#include "display_config.h"

class MetricDisplay {
public:
    MetricDisplay();

    // Initialize display at given I2C address
    bool begin(uint8_t i2c_addr, TwoWire* wire = &Wire);

    // Check if display initialized successfully
    bool isReady() const;

    // Configure display type and WAN association
    void configure(DisplayType type, int wan_id);

    // Set brightness (0-15)
    void setBrightness(uint8_t brightness);

    // Turn display on/off (uses HT16K33 display setup register)
    void setDisplayOn(bool on);

    // Render current metric value (uses prefix letter mode)
    void render();

    // Set which metric to display
    void setPacketMetric(PacketMetric metric);
    void setBandwidthMetric(BandwidthMetric metric);

    // Get current metric selection
    PacketMetric currentPacketMetric() const;
    BandwidthMetric currentBandwidthMetric() const;

    // Getters
    DisplayType displayType() const;
    int wanId() const;

private:
    Adafruit_7segment _display;
    TwoWire* _wire;
    uint8_t _i2c_addr;
    bool _ready;
    DisplayType _type;
    int _wan_id;
    PacketMetric _packet_metric;
    BandwidthMetric _bandwidth_metric;

    // Render helpers
    void renderPacketValue();
    void renderBandwidthValue();

    // Write a letter to first digit position
    void writeLetterDigit(char letter);

    // Show "----" for no data
    void showDashes();

    // Write a 3-digit value right-aligned in positions 1,3,4
    void write3DigitValue(int value);
};
