// leds.cpp
#include <Arduino.h>
#include "leds.h"

// Pin assignments
const int LED_WAN1_UP_PIN        = 5;
const int LED_WAN1_DEGRADED_PIN  = 18;
const int LED_WAN1_DOWN_PIN      = 19;
const int LED_HEARTBEAT_PIN      = 4;   // heartbeat LED

// Track current WAN1 state
static Wan1State g_wan1_state = WAN1_DOWN;

// Heartbeat tracking
static unsigned long g_wan1_last_update_ms = 0;          // 0 = never
static bool          g_wan1_timed_out      = false;
static const unsigned long WAN1_TIMEOUT_MS = 3UL * 60UL * 1000UL; // 3 minutes

// Heartbeat LED blink bookkeeping
static unsigned long g_hb_last_toggle_ms = 0;
static bool          g_hb_led_on         = false;

void set_led(int pin, bool on) {
    digitalWrite(pin, on ? HIGH : LOW);
    Serial.printf("GPIO %d -> %s\n", pin, on ? "ON" : "OFF");
}

bool led_state(int pin) {
    return (digitalRead(pin) == HIGH);
}

void wan1_set_state(Wan1State state) {
    g_wan1_state = state;

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

void wan1_record_update() {
    g_wan1_last_update_ms = millis();
    g_wan1_timed_out = false;
}

unsigned long wan1_last_update_ms() {
    return g_wan1_last_update_ms;
}

// internal: set heartbeat LED state (no logging spam)
static void heartbeat_set(bool on) {
    g_hb_led_on = on;
    digitalWrite(LED_HEARTBEAT_PIN, on ? HIGH : LOW);
}

static void heartbeat_update_pattern(unsigned long now, unsigned long elapsed_ms) {
    // Very recent (< 45s since last update): LED OFF
    if (elapsed_ms < 45UL * 1000UL) {
        heartbeat_set(false);
        return;
    }

    // >= timeout (3 min): solid ON
    if (elapsed_ms >= WAN1_TIMEOUT_MS) {
        heartbeat_set(true);
        return;
    }

    // 45–90s: slow blink, 90–180s: fast blink (unchanged)
    unsigned long interval_ms;
    if (elapsed_ms < 90UL * 1000UL) {
        interval_ms = 500UL;
    } else {
        interval_ms = 200UL;
    }

    if (now - g_hb_last_toggle_ms >= interval_ms) {
        g_hb_last_toggle_ms = now;
        heartbeat_set(!g_hb_led_on);
    }
}

void wan1_heartbeat_check() {
    unsigned long now = millis();

    if (g_wan1_last_update_ms == 0) {
        // Never received an update: indicate "no heartbeat yet" with solid ON
        heartbeat_set(true);
        return;
    }

    unsigned long elapsed = now - g_wan1_last_update_ms;

    // Update heartbeat LED pattern first
    heartbeat_update_pattern(now, elapsed);

    // Timeout → force WAN1 DOWN
    if (elapsed > WAN1_TIMEOUT_MS) {
        if (!g_wan1_timed_out) {
            g_wan1_timed_out = true;
            Serial.println("WAN1 heartbeat timeout -> forcing DOWN");
        }

        if (g_wan1_state != WAN1_DOWN) {
            wan1_set_state(WAN1_DOWN);
        }
    }
}

void leds_init() {
    pinMode(LED_WAN1_UP_PIN,       OUTPUT);
    pinMode(LED_WAN1_DEGRADED_PIN, OUTPUT);
    pinMode(LED_WAN1_DOWN_PIN,     OUTPUT);
    pinMode(LED_HEARTBEAT_PIN,     OUTPUT);

    // Default to DOWN on boot; heartbeat off
    wan1_set_state(WAN1_DOWN);
    heartbeat_set(true);

    g_wan1_last_update_ms = 0;
    g_wan1_timed_out = false;
    g_hb_last_toggle_ms = 0;
}
