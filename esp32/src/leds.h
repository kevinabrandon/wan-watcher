// leds.h
#pragma once

#include <Arduino.h>

// Pin assignments (wire your LEDs to match these, or adjust)
extern const int LED_WAN1_UP_PIN;        // e.g. GPIO 5  = green
extern const int LED_WAN1_DEGRADED_PIN;  // e.g. GPIO 18 = yellow
extern const int LED_WAN1_DOWN_PIN;      // e.g. GPIO 19 = red

// WAN1 state enum
enum Wan1State {
    WAN1_DOWN = 0,
    WAN1_DEGRADED = 1,
    WAN1_UP = 2
};

// Initialize LED pins and put WAN1 into a default state
void leds_init();

// Low-level helpers (still available if you want them)
void set_led(int pin, bool on);
bool led_state(int pin);

// High-level WAN1 helpers
void wan1_set_state(Wan1State state);
Wan1State wan1_get_state();
