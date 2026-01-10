// freshness_bar.h
// Bicolor 24-bar bargraph freshness indicator for pfSense connection
#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_LEDBackpack.h>

// Default I2C address for the freshness bar
static const uint8_t FRESHNESS_BAR_ADDR = 0x70;

// Time thresholds in milliseconds
// Green phase: 0-15s fill, 15-20s buffer
static const unsigned long FRESHNESS_GREEN_FILL_END_MS = 15UL * 1000UL;    // Green fill complete
static const unsigned long FRESHNESS_GREEN_BUFFER_END_MS = 20UL * 1000UL;  // Green buffer ends, yellow starts
// Yellow phase: 20-35s fill, 35-40s buffer
static const unsigned long FRESHNESS_YELLOW_FILL_END_MS = 35UL * 1000UL;   // Yellow fill complete
static const unsigned long FRESHNESS_YELLOW_BUFFER_END_MS = 40UL * 1000UL; // Yellow buffer ends, red starts
// Red phase: 40-55s fill, 55-60s buffer
static const unsigned long FRESHNESS_RED_FILL_END_MS = 55UL * 1000UL;      // Red fill complete
static const unsigned long FRESHNESS_RED_BUFFER_END_MS = 60UL * 1000UL;    // Red buffer ends, blink starts

// Phase duration for fill calculations (15 seconds each)
static const unsigned long FRESHNESS_FILL_DURATION_MS = 15UL * 1000UL;

// Blink interval for stale state (500ms)
static const unsigned long FRESHNESS_BLINK_INTERVAL_MS = 500UL;

// LED count per section
static const uint8_t LEDS_PER_SECTION = 8;
static const uint8_t TOTAL_LEDS = 24;

class FreshnessBar {
public:
    FreshnessBar();

    // Initialize bargraph at specified I2C address
    bool begin(uint8_t i2c_addr = FRESHNESS_BAR_ADDR, TwoWire* wire = &Wire);

    // Check if bargraph initialized successfully
    bool isReady() const;

    // Update the bargraph based on elapsed time since last pfSense update
    void update(unsigned long elapsed_ms, bool never_updated);

    // Set brightness (0-15)
    void setBrightness(uint8_t brightness);

    // Clear all LEDs
    void clear();

private:
    Adafruit_24bargraph _bar;
    bool _ready;
    uint8_t _brightness;

    // Blink state tracking
    bool _blink_on;
    unsigned long _last_blink_ms;

    // Cache previous state to avoid unnecessary I2C writes
    int _last_green_count;
    int _last_yellow_count;
    int _last_red_count;
    bool _last_was_blinking;
    bool _last_blink_state;

    // Internal helpers
    void renderBarOverwrite(int green_count, int yellow_count, int red_count);
    void renderBlinkingRed(bool on);
    bool stateChanged(int green, int yellow, int red, bool blinking, bool blink_state);
    void cacheState(int green, int yellow, int red, bool blinking, bool blink_state);
};
