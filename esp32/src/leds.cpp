// leds.cpp
#include <Arduino.h>
#include "leds.h"

// Pin assignments
const int LED_WAN1_UP_PIN        = 5;   // green
const int LED_WAN1_DEGRADED_PIN  = 18;  // yellow
const int LED_WAN1_DOWN_PIN      = 19;  // red

// Track current WAN1 state
static Wan1State g_wan1_state = WAN1_DOWN;

void set_led(int pin, bool on) {
    digitalWrite(pin, on ? HIGH : LOW);
    Serial.printf("GPIO %d -> %s\n", pin, on ? "ON" : "OFF");
}

bool led_state(int pin) {
    return (digitalRead(pin) == HIGH);
}

void wan1_set_state(Wan1State state) {
    g_wan1_state = state;

    // Ensure exactly one LED is ON at a time
    switch (state) {
        case WAN1_UP:
            set_led(LED_WAN1_UP_PIN, true);
            set_led(LED_WAN1_DEGRADED_PIN, false);
            set_led(LED_WAN1_DOWN_PIN, false);
            break;

        case WAN1_DEGRADED:
            set_led(LED_WAN1_UP_PIN, false);
            set_led(LED_WAN1_DEGRADED_PIN, true);
            set_led(LED_WAN1_DOWN_PIN, false);
            break;

        case WAN1_DOWN:
        default:
            set_led(LED_WAN1_UP_PIN, false);
            set_led(LED_WAN1_DEGRADED_PIN, false);
            set_led(LED_WAN1_DOWN_PIN, true);
            break;
    }
}

Wan1State wan1_get_state() {
    return g_wan1_state;
}

void leds_init() {
    pinMode(LED_WAN1_UP_PIN, OUTPUT);
    pinMode(LED_WAN1_DEGRADED_PIN, OUTPUT);
    pinMode(LED_WAN1_DOWN_PIN, OUTPUT);

    // Default to DOWN on boot
    wan1_set_state(WAN1_DOWN);
}
