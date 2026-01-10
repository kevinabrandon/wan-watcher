// freshness_bar.cpp
#include "freshness_bar.h"

FreshnessBar::FreshnessBar()
    : _ready(false)
    , _brightness(8)
    , _blink_on(false)
    , _last_blink_ms(0)
    , _last_green_count(-1)
    , _last_yellow_count(-1)
    , _last_red_count(-1)
    , _last_was_blinking(false)
    , _last_blink_state(false)
{}

bool FreshnessBar::begin(uint8_t i2c_addr, TwoWire* wire) {
    _ready = _bar.begin(i2c_addr, wire);
    if (_ready) {
        _bar.clear();
        _bar.writeDisplay();
        _bar.setBrightness(_brightness);
        Serial.printf("FreshnessBar initialized at 0x%02X\n", i2c_addr);
    } else {
        Serial.printf("FreshnessBar at 0x%02X: not found\n", i2c_addr);
    }
    return _ready;
}

bool FreshnessBar::isReady() const {
    return _ready;
}

void FreshnessBar::setBrightness(uint8_t brightness) {
    _brightness = (brightness > 15) ? 15 : brightness;
    if (_ready) {
        _bar.setBrightness(_brightness);
    }
}

void FreshnessBar::clear() {
    if (!_ready) return;
    _bar.clear();
    _bar.writeDisplay();
    _last_green_count = 0;
    _last_yellow_count = 0;
    _last_red_count = 0;
    _last_was_blinking = false;
}

void FreshnessBar::update(unsigned long elapsed_ms, bool never_updated) {
    if (!_ready) return;

    // If never updated, show empty bar
    if (never_updated) {
        if (stateChanged(0, 0, 0, false, false)) {
            clear();
            cacheState(0, 0, 0, false, false);
        }
        return;
    }

    unsigned long now = millis();

    // >60s: Full bar blinking red
    if (elapsed_ms >= FRESHNESS_RED_BUFFER_END_MS) {
        // Check if we should toggle blink state
        if (now - _last_blink_ms >= FRESHNESS_BLINK_INTERVAL_MS) {
            _last_blink_ms = now;
            _blink_on = !_blink_on;
        }

        if (stateChanged(TOTAL_LEDS, 0, 0, true, _blink_on)) {
            renderBlinkingRed(_blink_on);
            cacheState(TOTAL_LEDS, 0, 0, true, _blink_on);
        }
        return;
    }

    // Calculate fill level within current phase (0 to TOTAL_LEDS)
    // Each phase: 15s fill + 5s buffer
    int green_count = 0;
    int yellow_count = 0;
    int red_count = 0;

    if (elapsed_ms < FRESHNESS_GREEN_FILL_END_MS) {
        // 0-15s: Green fills entire bar
        green_count = (elapsed_ms * TOTAL_LEDS) / FRESHNESS_FILL_DURATION_MS;
    } else if (elapsed_ms < FRESHNESS_GREEN_BUFFER_END_MS) {
        // 15-20s: Buffer - all green
        green_count = TOTAL_LEDS;
    } else if (elapsed_ms < FRESHNESS_YELLOW_FILL_END_MS) {
        // 20-35s: Yellow overwrites green from left
        unsigned long yellow_elapsed = elapsed_ms - FRESHNESS_GREEN_BUFFER_END_MS;
        yellow_count = (yellow_elapsed * TOTAL_LEDS) / FRESHNESS_FILL_DURATION_MS;
        green_count = TOTAL_LEDS - yellow_count;  // Remaining LEDs stay green
    } else if (elapsed_ms < FRESHNESS_YELLOW_BUFFER_END_MS) {
        // 35-40s: Buffer - all yellow
        yellow_count = TOTAL_LEDS;
    } else if (elapsed_ms < FRESHNESS_RED_FILL_END_MS) {
        // 40-55s: Red overwrites yellow from left
        unsigned long red_elapsed = elapsed_ms - FRESHNESS_YELLOW_BUFFER_END_MS;
        red_count = (red_elapsed * TOTAL_LEDS) / FRESHNESS_FILL_DURATION_MS;
        yellow_count = TOTAL_LEDS - red_count;  // Remaining LEDs stay yellow
    } else {
        // 55-60s: Buffer - all red
        red_count = TOTAL_LEDS;
    }

    // Only update display if state changed
    if (stateChanged(green_count, yellow_count, red_count, false, false)) {
        renderBarOverwrite(green_count, yellow_count, red_count);
        cacheState(green_count, yellow_count, red_count, false, false);
    }
}

void FreshnessBar::renderBarOverwrite(int green_count, int yellow_count, int red_count) {
    // Render bar with overwrite behavior:
    // - During green phase: green_count LEDs are green, rest are off
    // - During yellow phase: yellow_count LEDs are yellow (left), green_count LEDs are green (right)
    // - During red phase: red_count LEDs are red (left), yellow_count LEDs are yellow (right)

    for (int i = 0; i < TOTAL_LEDS; i++) {
        uint8_t color;

        if (red_count > 0) {
            // Red phase: red on left, yellow on right
            if (i < red_count) {
                color = LED_RED;
            } else if (i < red_count + yellow_count) {
                color = LED_YELLOW;
            } else {
                color = LED_OFF;
            }
        } else if (yellow_count > 0) {
            // Yellow phase: yellow on left, green on right
            if (i < yellow_count) {
                color = LED_YELLOW;
            } else if (i < yellow_count + green_count) {
                color = LED_GREEN;
            } else {
                color = LED_OFF;
            }
        } else {
            // Green phase: green filling from left
            if (i < green_count) {
                color = LED_GREEN;
            } else {
                color = LED_OFF;
            }
        }

        _bar.setBar(i, color);
    }

    _bar.writeDisplay();
}

void FreshnessBar::renderBlinkingRed(bool on) {
    uint8_t color = on ? LED_RED : LED_OFF;

    for (int i = 0; i < TOTAL_LEDS; i++) {
        _bar.setBar(i, color);
    }

    _bar.writeDisplay();
}

bool FreshnessBar::stateChanged(int green, int yellow, int red, bool blinking, bool blink_state) {
    if (blinking != _last_was_blinking) return true;
    if (blinking && blink_state != _last_blink_state) return true;
    if (!blinking) {
        return (green != _last_green_count ||
                yellow != _last_yellow_count ||
                red != _last_red_count);
    }
    return false;
}

void FreshnessBar::cacheState(int green, int yellow, int red, bool blinking, bool blink_state) {
    _last_green_count = green;
    _last_yellow_count = yellow;
    _last_red_count = red;
    _last_was_blinking = blinking;
    _last_blink_state = blink_state;
}
