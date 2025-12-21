// leds.h
#pragma once

#include <Arduino.h>
#include "led.h"

// LED objects (defined in leds.cpp)
extern Led led_wan1_up;
extern Led led_wan1_degraded;
extern Led led_wan1_down;
extern Led led_heartbeat;
extern Led led_status1;

enum Wan1State {
    WAN1_DOWN = 0,
    WAN1_DEGRADED = 1,
    WAN1_UP = 2
};

void leds_init();

void wan1_set_state(Wan1State state);
Wan1State wan1_get_state();

// Heartbeat / last update tracking
void wan1_record_update();          // call when a valid update is received
unsigned long wan1_last_update_ms(); // 0 = never
void wan1_heartbeat_check();        // call regularly from loop()

// 7-segment display
void display_update();              // update display with seconds since last update
