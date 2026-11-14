// top_talkers.cpp
#include "top_talkers.h"
#include "wan_metrics.h"   // BandwidthSource, bw_source_to_string
#include <string.h>

static TopTalker g_tt[TT_WINDOWS][TT_MAX_HOSTS];
static uint8_t g_tt_count[TT_WINDOWS];

static void copy_str(char* dst, size_t size, const char* src) {
    strncpy(dst, src ? src : "", size - 1);
    dst[size - 1] = '\0';
}

static const char* window_key(int window) {
    return bw_source_to_string(static_cast<BandwidthSource>(window));
}

void top_talkers_init() {
    memset(g_tt, 0, sizeof(g_tt));
    memset(g_tt_count, 0, sizeof(g_tt_count));
}

void top_talkers_update(JsonObjectConst obj) {
    for (int w = 0; w < TT_WINDOWS; w++) {
        JsonArrayConst arr = obj[window_key(w)].as<JsonArrayConst>();
        int n = 0;
        for (JsonVariantConst v : arr) {
            if (n >= TT_MAX_HOSTS) break;
            JsonObjectConst e = v.as<JsonObjectConst>();
            if (e.isNull()) continue;
            TopTalker& t = g_tt[w][n];
            copy_str(t.ip, sizeof(t.ip), e["ip"] | "");
            copy_str(t.name, sizeof(t.name), e["name"] | "");
            t.down_mbps = e["down"] | 0.0f;
            t.up_mbps = e["up"] | 0.0f;
            copy_str(t.wan, sizeof(t.wan), e["wan"] | "");
            n++;
        }
        g_tt_count[w] = n;
    }
}

int top_talkers_count(int window) {
    if (window < 0 || window >= TT_WINDOWS) return 0;
    return g_tt_count[window];
}

void top_talkers_to_json(JsonVariant dst) {
    JsonObject tt = dst["top_talkers"].to<JsonObject>();
    for (int w = 0; w < TT_WINDOWS; w++) {
        JsonArray arr = tt[window_key(w)].to<JsonArray>();
        for (int i = 0; i < g_tt_count[w]; i++) {
            const TopTalker& t = g_tt[w][i];
            JsonObject e = arr.add<JsonObject>();
            e["ip"] = t.ip;
            e["name"] = t.name;
            e["down"] = t.down_mbps;
            e["up"] = t.up_mbps;
            e["wan"] = t.wan;
        }
    }
}
