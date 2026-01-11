// brightness_pot.cpp
#include "brightness_pot.h"
#include "leds.h"

BrightnessPotentiometer::BrightnessPotentiometer()
    : _pin(0)
    , _enabled(false)
    , _current_level(8)
    , _last_raw(2048)
    , _last_change_ms(0)
{}

void BrightnessPotentiometer::begin(uint8_t gpio_pin) {
    _pin = gpio_pin;

    // Read initial value
    _last_raw = analogRead(_pin);
    _current_level = rawToLevel(_last_raw);
    _last_change_ms = millis();
    _enabled = true;

    // Set initial brightness to match pot position
    set_display_brightness(_current_level);

    Serial.printf("Brightness pot initialized on GPIO %d, level: %d (raw: %d)\n",
                  _pin, _current_level, _last_raw);
}

void BrightnessPotentiometer::update() {
    if (!_enabled) return;

    unsigned long now = millis();

    // Debounce: skip reads too soon after last change
    if (now - _last_change_ms < DEBOUNCE_MS) {
        return;
    }

    uint16_t raw = analogRead(_pin);

    // Apply hysteresis: only update if change exceeds threshold
    int16_t diff = (int16_t)raw - (int16_t)_last_raw;
    if (abs(diff) < HYSTERESIS) {
        return;
    }

    // Update raw value
    _last_raw = raw;

    // Convert to level
    uint8_t new_level = rawToLevel(raw);

    // Only act if level actually changed
    if (new_level != _current_level) {
        _current_level = new_level;
        _last_change_ms = now;

        // Pot movement overrides current brightness
        set_display_brightness(_current_level);
        Serial.printf("Brightness pot: %d (raw: %d)\n", _current_level, raw);
    }
}

uint8_t BrightnessPotentiometer::getPotLevel() const {
    return _current_level;
}

bool BrightnessPotentiometer::isEnabled() const {
    return _enabled;
}

uint8_t BrightnessPotentiometer::rawToLevel(uint16_t raw) const {
    // Map 0-4095 to 0-15 (low = dim, high = bright)
    uint8_t level = raw / 256;
    if (level > 15) level = 15;
    return level;
}
