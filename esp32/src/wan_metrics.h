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
    // EWMA bandwidth averages (1/5/15 minute)
    float down_1m;
    float down_5m;
    float down_15m;
    float up_1m;
    float up_5m;
    float up_15m;
    unsigned long last_update_ms;
    char local_ip[16];
    char gateway_ip[16];
    char monitor_ip[16];
};

// Maximum number of WANs supported
static const int MAX_WANS = 2;

// Global metrics storage
extern WanMetrics g_wan_metrics[MAX_WANS];

// Initialize metrics to defaults
void wan_metrics_init();

// Update metrics for a WAN (wan_id: 1 or 2)
void wan_metrics_update(int wan_id, WanState state, uint8_t loss_pct,
                        uint16_t latency_ms, uint16_t jitter_ms,
                        float down_mbps, float up_mbps,
                        float down_1m, float down_5m, float down_15m,
                        float up_1m, float up_5m, float up_15m,
                        const char* local_ip, const char* gateway_ip,
                        const char* monitor_ip);

// Update router-level info (from top-level JSON fields)
void wan_metrics_set_router_info(const char* router_ip, const char* timestamp);

// Get router IP
const char* wan_metrics_get_router_ip();

// Get last timestamp from pfSense
const char* wan_metrics_get_timestamp();

// Get metrics for a WAN (wan_id: 1 or 2)
const WanMetrics& wan_metrics_get(int wan_id);

// Parse state string to enum
WanState wan_state_from_string(const char* str);

// Convert state enum to string
const char* wan_state_to_string(WanState state);

// Bandwidth source setting (which time window to display)
enum class BandwidthSource : uint8_t {
    INSTANT = 0,  // 15s (raw sample)
    AVG_1M  = 1,  // 1 minute EWMA (default)
    AVG_5M  = 2,  // 5 minute EWMA
    AVG_15M = 3   // 15 minute EWMA
};

// Set/get bandwidth display source
void wan_metrics_set_bw_source(BandwidthSource source);
BandwidthSource wan_metrics_get_bw_source();

// Get bandwidth values based on current source setting
float wan_metrics_get_down(int wan_id);
float wan_metrics_get_up(int wan_id);

// Convert bandwidth source to/from string
const char* bw_source_to_string(BandwidthSource source);
BandwidthSource bw_source_from_string(const char* str);
