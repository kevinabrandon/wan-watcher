// top_talkers.h
//
// Top LAN hosts by bandwidth, as reported by the pfSense daemon in the
// optional "top_talkers" object of POST /api/wans: one ranked list per
// bandwidth window (15s, 1m, 5m, 15m). Stored as-is and passed through to
// GET /api/status for the Web UI.
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// One list per BandwidthSource value, in enum order (15s, 1m, 5m, 15m)
static const int TT_WINDOWS = 4;
static const int TT_MAX_HOSTS = 5;

struct TopTalker {
    char ip[16];
    char name[33];      // DHCP hostname, "" when unknown (daemon caps at 32 chars)
    float down_mbps;
    float up_mbps;
    char wan[5];        // "wan1", "wan2" or ""
};

void top_talkers_init();

// Replace the stored lists from a "top_talkers" object. Windows missing from
// the object become empty.
void top_talkers_update(JsonObjectConst obj);

// Number of hosts stored for a window (0..TT_MAX_HOSTS)
int top_talkers_count(int window);

// Add a "top_talkers" object with the same shape as the daemon's to `dst`
// (normally the /api/status document).
void top_talkers_to_json(JsonVariant dst);
