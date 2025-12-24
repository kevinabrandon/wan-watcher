// wan_metrics.h
#pragma once

#include <Arduino.h>

enum class WanState { DOWN, DEGRADED, UP };

struct WanMetrics {
    WanState state;
    uint8_t loss_pct;
    uint16_t latency_ms;
    uint16_t jitter_ms;
    float down_mbps;
    float up_mbps;
    unsigned long last_update_ms;
};

// Maximum number of WANs supported
static const int MAX_WANS = 2;

// Global metrics storage
extern WanMetrics wan_metrics[MAX_WANS];

// Initialize metrics to defaults
void wan_metrics_init();

// Update metrics for a WAN (wan_id: 1 or 2)
void wan_metrics_update(int wan_id, WanState state, uint8_t loss_pct,
                        uint16_t latency_ms, uint16_t jitter_ms,
                        float down_mbps, float up_mbps);

// Get metrics for a WAN (wan_id: 1 or 2)
const WanMetrics& wan_metrics_get(int wan_id);

// Parse state string to enum
WanState wan_state_from_string(const char* str);

// Convert state enum to string
const char* wan_state_to_string(WanState state);
