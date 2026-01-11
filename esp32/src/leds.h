// leds.h
#pragma once

#include <Arduino.h>
#include "led.h"
#include "wan_metrics.h"
#include "display_config.h"
#include "display_manager.h"
#include "button_handler.h"
#include "freshness_bar.h"
#include "brightness_pot.h"

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

// Brightness control (0-15) for all HT16K33 displays
void set_display_brightness(uint8_t brightness);
uint8_t get_display_brightness();

// Display on/off control (uses HT16K33 display enable register)
void set_displays_on(bool on);
bool get_displays_on();

// Physical power switch (toggle switch on MCP pin)
void power_switch_init();   // Call after leds_init_with_displays()
void power_switch_update(); // Call from loop()
bool get_power_switch_position(); // Get physical switch state (true=on position)

// Brightness potentiometer (analog input on GPIO)
extern BrightnessPotentiometer g_brightness_pot;
uint8_t get_brightness_pot_level(); // Get pot position as brightness level (0-15)
