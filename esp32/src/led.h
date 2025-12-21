// led.h
#pragma once

#include <Arduino.h>
#include <Adafruit_MCP23X17.h>

enum class LedPinType { GPIO, MCP };

class Led {
public:
    Led(uint8_t pin, LedPinType type, Adafruit_MCP23X17* mcp = nullptr);

    void begin();           // Set pin as OUTPUT
    void set(bool on);      // Write HIGH/LOW
    bool state() const;     // Read current state

    uint8_t pin() const { return _pin; }
    LedPinType type() const { return _type; }

private:
    uint8_t _pin;
    LedPinType _type;
    Adafruit_MCP23X17* _mcp;
};
