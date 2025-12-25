// button_handler.cpp
#include "button_handler.h"

ButtonHandler::ButtonHandler()
    : _pin(0)
    , _type(ButtonPinType::GPIO)
    , _mcp(nullptr)
    , _enabled(false)
    , _last_debounce_ms(0)
    , _last_raw_state(HIGH)
    , _stable_state(HIGH)
    , _was_pressed(false)
    , _press_start_ms(0)
    , _long_press_threshold_ms(1000)
    , _long_press_fired(false)
    , _short_press_cb(nullptr)
    , _long_press_cb(nullptr)
{}

void ButtonHandler::begin(uint8_t pin, ButtonPinType type, Adafruit_MCP23X17* mcp) {
    if (pin == 0) {
        _enabled = false;
        return;
    }

    _pin = pin;
    _type = type;
    _mcp = mcp;

    if (_type == ButtonPinType::MCP) {
        if (_mcp == nullptr) {
            Serial.println("ERROR: ButtonHandler MCP mode requires MCP pointer");
            _enabled = false;
            return;
        }
        _mcp->pinMode(_pin, INPUT_PULLUP);
        Serial.printf("Button handler initialized on MCP pin %d\n", _pin);
    } else {
        pinMode(_pin, INPUT_PULLUP);
        Serial.printf("Button handler initialized on GPIO %d\n", _pin);
    }

    _enabled = true;
    _stable_state = readPin();
    _last_raw_state = _stable_state;
}

bool ButtonHandler::readPin() const {
    if (_type == ButtonPinType::MCP && _mcp != nullptr) {
        return _mcp->digitalRead(_pin);
    }
    return digitalRead(_pin);
}

void ButtonHandler::onShortPress(ButtonCallback callback) {
    _short_press_cb = callback;
}

void ButtonHandler::onLongPress(ButtonCallback callback) {
    _long_press_cb = callback;
}

void ButtonHandler::setLongPressThreshold(unsigned long ms) {
    _long_press_threshold_ms = ms;
}

bool ButtonHandler::isPressed() const {
    return _enabled && (_stable_state == LOW);
}

bool ButtonHandler::isEnabled() const {
    return _enabled;
}

void ButtonHandler::update() {
    if (!_enabled) return;

    unsigned long now = millis();
    bool raw = readPin();

    // Debounce
    if (raw != _last_raw_state) {
        _last_debounce_ms = now;
    }
    _last_raw_state = raw;

    if ((now - _last_debounce_ms) > DEBOUNCE_MS) {
        // State is stable
        if (raw != _stable_state) {
            _stable_state = raw;

            if (_stable_state == LOW) {
                // Button pressed
                _was_pressed = true;
                _press_start_ms = now;
                _long_press_fired = false;
            } else {
                // Button released
                if (_was_pressed && !_long_press_fired) {
                    // Short press
                    if (_short_press_cb) {
                        _short_press_cb();
                    }
                }
                _was_pressed = false;
            }
        }
    }

    // Check for long press while held
    if (_was_pressed && !_long_press_fired && _stable_state == LOW) {
        if ((now - _press_start_ms) >= _long_press_threshold_ms) {
            _long_press_fired = true;
            if (_long_press_cb) {
                _long_press_cb();
            }
        }
    }
}
