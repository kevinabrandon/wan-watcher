// brightness_pot.h
// Analog potentiometer for brightness control with hysteresis
#pragma once

#include <Arduino.h>

class BrightnessPotentiometer {
public:
    BrightnessPotentiometer();

    // Initialize with GPIO pin (must be ADC1: 32, 33, 34, 35, 36, 39)
    void begin(uint8_t gpio_pin);

    // Call from loop() to read and process pot value
    void update();

    // Get current pot position as brightness level (0-15)
    uint8_t getPotLevel() const;

    // Check if initialized
    bool isEnabled() const;

private:
    uint8_t _pin;
    bool _enabled;
    uint8_t _current_level;
    uint16_t _last_raw;
    unsigned long _last_change_ms;

    // Hysteresis to prevent jitter (~0.5 brightness levels worth)
    static const uint16_t HYSTERESIS = 128;

    // Debounce time for rapid changes
    static const unsigned long DEBOUNCE_MS = 50;

    // Convert raw ADC value to brightness level (0-15)
    uint8_t rawToLevel(uint16_t raw) const;
};
