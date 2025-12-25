// leds.h
#pragma once

#include <Arduino.h>
#include "led.h"
#include "wan_metrics.h"
#include "display_config.h"
#include "display_manager.h"
#include "button_handler.h"

// LED objects (defined in leds.cpp)
extern Led g_led_wan1_up;
extern Led g_led_wan1_degraded;
extern Led g_led_wan1_down;
extern Led g_led_heartbeat;
extern Led g_led_status1;

// Display manager and button handlers (defined in leds.cpp)
extern DisplayManager g_display_manager;
extern ButtonHandler g_button_handler_packet;   // Controls packet display (L/J/P)
extern ButtonHandler g_button_handler_bandwidth; // Controls bandwidth display (d/U)

// Legacy init (single display showing seconds since update)
void leds_init();

// New init with multi-display support
void leds_init_with_displays(const DisplaySystemConfig& config);

// Update WAN1 LEDs based on state
void wan1_set_leds(WanState state);

// Heartbeat check - call regularly from loop()
// Uses wan_metrics[0].last_update_ms for timing
void wan1_heartbeat_check();

// 7-segment display update
// Uses DisplayManager if active, otherwise legacy single display
void display_update();
