// leds.h
#pragma once

#include <Arduino.h>

extern const int LED_WAN1_UP_PIN;
extern const int LED_WAN1_DEGRADED_PIN;
extern const int LED_WAN1_DOWN_PIN;
extern const int LED_HEARTBEAT_PIN;

enum Wan1State {
    WAN1_DOWN = 0,
    WAN1_DEGRADED = 1,
    WAN1_UP = 2
};

void leds_init();

void set_led(int pin, bool on);
bool led_state(int pin);

void wan1_set_state(Wan1State state);
Wan1State wan1_get_state();

// Heartbeat / last update tracking
void wan1_record_update();          // call when a valid update is received
unsigned long wan1_last_update_ms(); // 0 = never
void wan1_heartbeat_check();        // call regularly from loop()
