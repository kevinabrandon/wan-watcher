// wan_metrics.cpp
#include "wan_metrics.h"
#include <string.h>

// Global metrics storage (index 0 = wan1, index 1 = wan2)
WanMetrics g_wan_metrics[MAX_WANS];

// Global router-level info
static char g_router_ip[16] = "";
static char g_last_timestamp[32] = "";

void wan_metrics_init() {
    for (int i = 0; i < MAX_WANS; i++) {
        g_wan_metrics[i].state = WanState::DOWN;
        g_wan_metrics[i].loss_pct = 100;
        g_wan_metrics[i].latency_ms = 0;
        g_wan_metrics[i].jitter_ms = 0;
        g_wan_metrics[i].down_mbps = 0.0f;
        g_wan_metrics[i].up_mbps = 0.0f;
        g_wan_metrics[i].last_update_ms = 0;
        g_wan_metrics[i].local_ip[0] = '\0';
        g_wan_metrics[i].gateway_ip[0] = '\0';
        g_wan_metrics[i].monitor_ip[0] = '\0';
    }
    g_router_ip[0] = '\0';
    g_last_timestamp[0] = '\0';
}

void wan_metrics_update(int wan_id, WanState state, uint8_t loss_pct,
                        uint16_t latency_ms, uint16_t jitter_ms,
                        float down_mbps, float up_mbps,
                        const char* local_ip, const char* gateway_ip,
                        const char* monitor_ip) {
    if (wan_id < 1 || wan_id > MAX_WANS) return;

    int idx = wan_id - 1;
    g_wan_metrics[idx].state = state;
    g_wan_metrics[idx].loss_pct = loss_pct;
    g_wan_metrics[idx].latency_ms = latency_ms;
    g_wan_metrics[idx].jitter_ms = jitter_ms;
    g_wan_metrics[idx].down_mbps = down_mbps;
    g_wan_metrics[idx].up_mbps = up_mbps;
    g_wan_metrics[idx].last_update_ms = millis();

    strncpy(g_wan_metrics[idx].local_ip, local_ip ? local_ip : "", sizeof(g_wan_metrics[idx].local_ip) - 1);
    g_wan_metrics[idx].local_ip[sizeof(g_wan_metrics[idx].local_ip) - 1] = '\0';

    strncpy(g_wan_metrics[idx].gateway_ip, gateway_ip ? gateway_ip : "", sizeof(g_wan_metrics[idx].gateway_ip) - 1);
    g_wan_metrics[idx].gateway_ip[sizeof(g_wan_metrics[idx].gateway_ip) - 1] = '\0';

    strncpy(g_wan_metrics[idx].monitor_ip, monitor_ip ? monitor_ip : "", sizeof(g_wan_metrics[idx].monitor_ip) - 1);
    g_wan_metrics[idx].monitor_ip[sizeof(g_wan_metrics[idx].monitor_ip) - 1] = '\0';
}

void wan_metrics_set_router_info(const char* router_ip, const char* timestamp) {
    strncpy(g_router_ip, router_ip ? router_ip : "", sizeof(g_router_ip) - 1);
    g_router_ip[sizeof(g_router_ip) - 1] = '\0';

    strncpy(g_last_timestamp, timestamp ? timestamp : "", sizeof(g_last_timestamp) - 1);
    g_last_timestamp[sizeof(g_last_timestamp) - 1] = '\0';
}

const char* wan_metrics_get_router_ip() {
    return g_router_ip;
}

const char* wan_metrics_get_timestamp() {
    return g_last_timestamp;
}

const WanMetrics& wan_metrics_get(int wan_id) {
    if (wan_id < 1 || wan_id > MAX_WANS) {
        return g_wan_metrics[0];  // fallback to wan1
    }
    return g_wan_metrics[wan_id - 1];
}

WanState wan_state_from_string(const char* str) {
    if (strcasecmp(str, "up") == 0) return WanState::UP;
    if (strcasecmp(str, "degraded") == 0) return WanState::DEGRADED;
    return WanState::DOWN;
}

const char* wan_state_to_string(WanState state) {
    switch (state) {
        case WanState::UP: return "up";
        case WanState::DEGRADED: return "degraded";
        case WanState::DOWN:
        default: return "down";
    }
}
