// wan_metrics.cpp
#include "wan_metrics.h"
#include <string.h>

// Global metrics storage (index 0 = wan1, index 1 = wan2)
WanMetrics wan_metrics[MAX_WANS];

void wan_metrics_init() {
    for (int i = 0; i < MAX_WANS; i++) {
        wan_metrics[i].state = WanState::DOWN;
        wan_metrics[i].loss_pct = 100;
        wan_metrics[i].latency_ms = 0;
        wan_metrics[i].jitter_ms = 0;
        wan_metrics[i].down_mbps = 0.0f;
        wan_metrics[i].up_mbps = 0.0f;
        wan_metrics[i].last_update_ms = 0;
    }
}

void wan_metrics_update(int wan_id, WanState state, uint8_t loss_pct,
                        uint16_t latency_ms, uint16_t jitter_ms,
                        float down_mbps, float up_mbps) {
    if (wan_id < 1 || wan_id > MAX_WANS) return;

    int idx = wan_id - 1;
    wan_metrics[idx].state = state;
    wan_metrics[idx].loss_pct = loss_pct;
    wan_metrics[idx].latency_ms = latency_ms;
    wan_metrics[idx].jitter_ms = jitter_ms;
    wan_metrics[idx].down_mbps = down_mbps;
    wan_metrics[idx].up_mbps = up_mbps;
    wan_metrics[idx].last_update_ms = millis();
}

const WanMetrics& wan_metrics_get(int wan_id) {
    if (wan_id < 1 || wan_id > MAX_WANS) {
        return wan_metrics[0];  // fallback to wan1
    }
    return wan_metrics[wan_id - 1];
}

WanState wan_state_from_string(const char* str) {
    if (strcasecmp(str, "up") == 0) return WanState::UP;
    if (strcasecmp(str, "degraded") == 0) return WanState::DEGRADED;
    return WanState::DOWN;
}

const char* wan_state_to_string(WanState state) {
    switch (state) {
        case WanState::UP: return "UP";
        case WanState::DEGRADED: return "DEGRADED";
        case WanState::DOWN:
        default: return "DOWN";
    }
}
