// local_pinger.h
// ESP32-based ICMP pinger for independent internet health monitoring
#pragma once

#include <Arduino.h>
#include "wan_metrics.h"  // For WanState enum

// Local pinger metrics (similar to WanMetrics but for local ping results)
struct LocalPingerMetrics {
    WanState state;              // UP/DEGRADED/DOWN based on thresholds
    uint16_t latency_ms;         // Average latency in ms
    uint16_t jitter_ms;          // Standard deviation (jitter) in ms
    uint8_t loss_pct;            // Packet loss percentage (0-100)
    uint16_t sample_count;       // Number of samples in current window
    uint16_t window_secs;        // Actual time span of samples (seconds)
    unsigned long last_update_ms; // Timestamp of last stats calculation
};

// Configuration constants
static const char* DEFAULT_PING_TARGET = "8.8.8.8";
static const unsigned long PING_INTERVAL_MS = 500;      // Send ping every 500ms
static const unsigned long SAMPLE_WINDOW_MS = 60000;    // 60 second window
static const unsigned long LOSS_TIMEOUT_MS = 5000;      // Mark as lost after 5s
static const unsigned long STATS_UPDATE_MS = 1000;      // Recalculate stats every 1s

// Thresholds
static const uint16_t LATENCY_DEGRADED_MS = 200;  // >200ms = degraded
static const uint8_t LOSS_DEGRADED_PCT = 5;       // >5% loss = degraded
static const uint8_t LOSS_DOWN_PCT = 50;          // >50% loss = down

// Initialize the local pinger (call once in setup())
void local_pinger_init();

// Update the pinger (call from loop())
// Handles sending pings and recalculating stats
void local_pinger_update();

// Get current metrics
const LocalPingerMetrics& local_pinger_get();

// Set ping target (IP address as string)
void local_pinger_set_target(const char* target);

// Get current ping target
const char* local_pinger_get_target();
