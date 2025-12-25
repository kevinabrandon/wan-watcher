// button_handler.h
// Debounced button input with short/long press detection
#pragma once

#include <Arduino.h>
#include <Adafruit_MCP23X17.h>

// Callback type
typedef void (*ButtonCallback)();

// Pin type for button
enum class ButtonPinType {
    GPIO,
    MCP
};

class ButtonHandler {
public:
    ButtonHandler();

    // Initialize with GPIO pin (configured as INPUT_PULLUP)
    void begin(uint8_t pin, ButtonPinType type = ButtonPinType::GPIO,
               Adafruit_MCP23X17* mcp = nullptr);

    // Set callbacks for button actions
    void onShortPress(ButtonCallback callback);
    void onLongPress(ButtonCallback callback);

    // Set long press threshold (default 1000ms)
    void setLongPressThreshold(unsigned long ms);

    // Call from loop() to process button state
    void update();

    // Check if button is currently pressed
    bool isPressed() const;

    // Check if initialized
    bool isEnabled() const;

private:
    uint8_t _pin;
    ButtonPinType _type;
    Adafruit_MCP23X17* _mcp;
    bool _enabled;

    // Read pin state (routes to GPIO or MCP)
    bool readPin() const;

    // Debounce
    static const unsigned long DEBOUNCE_MS = 50;
    unsigned long _last_debounce_ms;
    bool _last_raw_state;
    bool _stable_state;

    // Press tracking
    bool _was_pressed;
    unsigned long _press_start_ms;
    unsigned long _long_press_threshold_ms;
    bool _long_press_fired;

    // Callbacks
    ButtonCallback _short_press_cb;
    ButtonCallback _long_press_cb;
};
