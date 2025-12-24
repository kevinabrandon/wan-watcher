// leds.h
#pragma once

#include <Arduino.h>
#include "led.h"
#include "wan_metrics.h"

// LED objects (defined in leds.cpp)
extern Led led_wan1_up;
extern Led led_wan1_degraded;
extern Led led_wan1_down;
extern Led led_heartbeat;
extern Led led_status1;

void leds_init();

// Update WAN1 LEDs based on state
void wan1_set_leds(WanState state);

// Heartbeat check - call regularly from loop()
// Uses wan_metrics[0].last_update_ms for timing
void wan1_heartbeat_check();

// 7-segment display
void display_update();
