// led.cpp
#include "led.h"

Led::Led(uint8_t pin, LedPinType type, Adafruit_MCP23X17* mcp)
    : _pin(pin), _type(type), _mcp(mcp) {}

void Led::begin() {
    if (_type == LedPinType::MCP && _mcp) {
        _mcp->pinMode(_pin, OUTPUT);
        _mcp->digitalWrite(_pin, LOW);
    } else {
        pinMode(_pin, OUTPUT);
        digitalWrite(_pin, LOW);
    }
}

void Led::set(bool on) {
    if (_type == LedPinType::MCP && _mcp) {
        _mcp->digitalWrite(_pin, on ? HIGH : LOW);
    } else {
        digitalWrite(_pin, on ? HIGH : LOW);
    }
}

bool Led::state() const {
    if (_type == LedPinType::MCP && _mcp) {
        return _mcp->digitalRead(_pin) == HIGH;
    } else {
        return digitalRead(_pin) == HIGH;
    }
}
