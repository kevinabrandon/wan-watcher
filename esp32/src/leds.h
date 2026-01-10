// leds.h
#pragma once

#include <Arduino.h>
#include "led.h"
#include "wan_metrics.h"
#include "display_config.h"
#include "display_manager.h"
#include "button_handler.h"
#include "freshness_bar.h"

// LED objects (defined in leds.cpp)
extern Led g_led_wan1_up;
extern Led g_led_wan1_degraded;
extern Led g_led_wan1_down;
extern Led g_led_wan2_up;
extern Led g_led_wan2_degraded;
extern Led g_led_wan2_down;
extern Led g_led_local_up;
extern Led g_led_local_degraded;
extern Led g_led_local_down;
extern Led g_led_heartbeat;
extern Led g_led_status1;

// Display manager and button handlers (defined in leds.cpp)
extern DisplayManager g_display_manager;
extern ButtonHandler g_button_handler_packet;   // Controls packet display (L/J/P)
extern ButtonHandler g_button_handler_bandwidth; // Controls bandwidth display (d/U)

// Freshness bar indicator (defined in leds.cpp)
extern FreshnessBar g_freshness_bar;

// Legacy init (single display showing seconds since update)
void leds_init();

// New init with multi-display support
void leds_init_with_displays(const DisplaySystemConfig& config);

// Update WAN1 LEDs based on state
void wan1_set_leds(WanState state);

// Update WAN2 LEDs based on state
void wan2_set_leds(WanState state);

// Update local pinger LEDs based on state
void local_pinger_set_leds(WanState state);

// Router heartbeat check - call regularly from loop()
// Monitors pfSense daemon connection, forces all WANs DOWN on timeout
void router_heartbeat_check();

// Freshness bar update - call regularly from loop()
// Updates the bicolor LED bargraph based on data freshness
void freshness_bar_update();

// 7-segment display update
// Uses DisplayManager if active, otherwise legacy single display
void display_update();
