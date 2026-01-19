// http_routes.cpp
#include <Arduino.h>
#include <WebServer.h>
#include <ETH.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "http_routes.h"
#include "hostname.h"
#include "leds.h"
#include "wan_metrics.h"
#include "local_pinger.h"
#include "freshness_bar.h"

// ---- Favicon SVGs ----
static const char* FAVICON_GREEN = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32">
  <circle cx="16" cy="16" r="14" fill="#2ecc71"/>
  <text x="16" y="16" text-anchor="middle" font-size="12" font-weight="700" fill="#ffffff" font-family="system-ui, sans-serif">W</text>
  <text x="16" y="24" text-anchor="middle" font-size="12" font-weight="700" fill="#ffffff" font-family="system-ui, sans-serif">W</text>
</svg>)";
static const char* FAVICON_YELLOW = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32">
  <circle cx="16" cy="16" r="14" fill="#f1c40f"/>
  <text x="16" y="16" text-anchor="middle" font-size="12" font-weight="700" fill="#ffffff" font-family="system-ui, sans-serif">W</text>
  <text x="16" y="24" text-anchor="middle" font-size="12" font-weight="700" fill="#ffffff" font-family="system-ui, sans-serif">W</text>
</svg>)";
static const char* FAVICON_RED = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32">
  <circle cx="16" cy="16" r="14" fill="#e74c3c"/>
  <text x="16" y="16" text-anchor="middle" font-size="12" font-weight="700" fill="#ffffff" font-family="system-ui, sans-serif">W</text>
  <text x="16" y="24" text-anchor="middle" font-size="12" font-weight="700" fill="#ffffff" font-family="system-ui, sans-serif">W</text>
</svg>)";

// ---- Helper: Get content type from filename ----
static String get_content_type(const String& filename) {
    if (filename.endsWith(".html")) return "text/html";
    if (filename.endsWith(".css")) return "text/css";
    if (filename.endsWith(".js")) return "application/javascript";
    if (filename.endsWith(".svg")) return "image/svg+xml";
    return "text/plain";
}

// ---- Generic file handler ----
static void handle_file_read(WebServer& server, String path) {
    if (path.endsWith("/")) {
        path += "index.html";
    }
    String contentType = get_content_type(path);
    if (LittleFS.exists(path)) {
        File file = LittleFS.open(path, "r");
        server.streamFile(file, contentType);
        file.close();
    } else {
        server.send(404, "text/plain", "Not found");
    }
}

// ---- 404 handler ----
static void handle_not_found(WebServer& server) {
    // Try to serve a file instead of a simple 404
    handle_file_read(server, server.uri());
}

// ---- Helper: Parse JSON and update WAN metrics ----
static bool parse_wan_json(JsonObject& obj, int wan_id) {
    const char* state_str = obj["state"] | "down";
    WanState state = wan_state_from_string(state_str);
    uint8_t loss_pct = obj["loss_pct"] | 100;
    uint16_t latency_ms = obj["latency_ms"] | 0;
    uint16_t jitter_ms = obj["jitter_ms"] | 0;
    float down_mbps = obj["down_mbps"] | 0.0f;
    float up_mbps = obj["up_mbps"] | 0.0f;
    const char* local_ip = obj["local_ip"] | "";
    const char* gateway_ip = obj["gateway_ip"] | "";
    const char* monitor_ip = obj["monitor_ip"] | "";

    wan_metrics_update(wan_id, state, loss_pct, latency_ms, jitter_ms,
                       down_mbps, up_mbps, local_ip, gateway_ip, monitor_ip);

    // Update LEDs based on WAN
    if (wan_id == 1) {
        wan1_set_leds(state);
    } else if (wan_id == 2) {
        wan2_set_leds(state);
    }

    Serial.printf("WAN%d updated: state=%s loss=%d%% lat=%dms local=%s gw=%s\n",
                  wan_id, state_str, loss_pct, latency_ms, local_ip, gateway_ip);

    return true;
}

// ---- Handler: POST /api/wans (batch) ----
static void handle_wans_post(WebServer& server) {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"no body\"}");
        return;
    }

    String body = server.arg("plain");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
        Serial.printf("JSON parse error: %s\n", error.c_str());
        server.send(400, "application/json", "{\"error\":\"invalid JSON\"}");
        return;
    }

    // Extract top-level router info
    const char* router_ip = doc["router_ip"] | "";
    const char* timestamp = doc["timestamp"] | "";
    wan_metrics_set_router_info(router_ip, timestamp);

    // Process wan1 if present
    if (doc["wan1"].is<JsonObject>()) {
        JsonObject wan1 = doc["wan1"];
        parse_wan_json(wan1, 1);
    }

    // Process wan2 if present
    if (doc["wan2"].is<JsonObject>()) {
        JsonObject wan2 = doc["wan2"];
        parse_wan_json(wan2, 2);
    }

    // Build response with all WANs
    JsonDocument resp;
    resp["status"] = "ok";

    for (int i = 1; i <= MAX_WANS; i++) {
        const WanMetrics& m = wan_metrics_get(i);
        JsonObject wan = resp["wan" + String(i)].to<JsonObject>();
        wan["state"] = wan_state_to_string(m.state);
        wan["loss_pct"] = m.loss_pct;
        wan["latency_ms"] = m.latency_ms;
        wan["jitter_ms"] = m.jitter_ms;
        wan["down_mbps"] = m.down_mbps;
        wan["up_mbps"] = m.up_mbps;
    }

    String output;
    serializeJson(resp, output);
    server.send(200, "application/json", output);
}

// ---- Handler: GET /api/status ----
static void handle_status_get(WebServer& server) {
    const WanMetrics& w1 = wan_metrics_get(1);
    const WanMetrics& w2 = wan_metrics_get(2);
    const LocalPingerMetrics& lp = local_pinger_get();
    const char* timestamp = wan_metrics_get_timestamp();

    JsonDocument doc;
    doc["timestamp"] = timestamp;
    doc["router_ip"] = wan_metrics_get_router_ip();

    JsonObject wan1 = doc["wan1"].to<JsonObject>();
    wan1["state"] = wan_state_to_string(w1.state);
    wan1["latency_ms"] = w1.latency_ms;
    wan1["jitter_ms"] = w1.jitter_ms;
    wan1["loss_pct"] = w1.loss_pct;
    wan1["down_mbps"] = w1.down_mbps;
    wan1["up_mbps"] = w1.up_mbps;
    wan1["monitor_ip"] = w1.monitor_ip;
    wan1["gateway_ip"] = w1.gateway_ip;
    wan1["local_ip"] = w1.local_ip;

    JsonObject wan2 = doc["wan2"].to<JsonObject>();
    wan2["state"] = wan_state_to_string(w2.state);
    wan2["latency_ms"] = w2.latency_ms;
    wan2["jitter_ms"] = w2.jitter_ms;
    wan2["loss_pct"] = w2.loss_pct;
    wan2["down_mbps"] = w2.down_mbps;
    wan2["up_mbps"] = w2.up_mbps;
    wan2["monitor_ip"] = w2.monitor_ip;
    wan2["gateway_ip"] = w2.gateway_ip;
    wan2["local_ip"] = w2.local_ip;

    JsonObject local = doc["local"].to<JsonObject>();
    local["state"] = wan_state_to_string(lp.state);
    local["latency_ms"] = lp.latency_ms;
    local["jitter_ms"] = lp.jitter_ms;
    local["loss_pct"] = lp.loss_pct;

    // Freshness bar timing constants (in seconds, matching hardware)
    JsonObject freshness = doc["freshness"].to<JsonObject>();
    freshness["green_fill_end"] = FRESHNESS_GREEN_FILL_END_MS / 1000;
    freshness["green_buffer_end"] = FRESHNESS_GREEN_BUFFER_END_MS / 1000;
    freshness["yellow_fill_end"] = FRESHNESS_YELLOW_FILL_END_MS / 1000;
    freshness["yellow_buffer_end"] = FRESHNESS_YELLOW_BUFFER_END_MS / 1000;
    freshness["red_fill_end"] = FRESHNESS_RED_FILL_END_MS / 1000;
    freshness["red_buffer_end"] = FRESHNESS_RED_BUFFER_END_MS / 1000;
    freshness["fill_duration"] = FRESHNESS_FILL_DURATION_MS / 1000;
    freshness["led_count"] = TOTAL_LEDS;

    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", output);
}

// ---- Handler: GET /api/brightness ----
static void handle_brightness_get(WebServer& server) {
    JsonDocument doc;
    doc["brightness"] = get_display_brightness();
    doc["pot_level"] = get_brightness_pot_level();

    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", output);
}

// ---- Handler: POST /api/brightness ----
static void handle_brightness_post(WebServer& server) {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"no body\"}");
        return;
    }

    String body = server.arg("plain");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
        server.send(400, "application/json", "{\"error\":\"invalid JSON\"}");
        return;
    }

    if (!doc["brightness"].is<int>()) {
        server.send(400, "application/json", "{\"error\":\"brightness field required\"}");
        return;
    }

    int brightness = doc["brightness"];
    if (brightness < 0) brightness = 0;
    if (brightness > 15) brightness = 15;

    set_display_brightness((uint8_t)brightness);

    JsonDocument resp;
    resp["brightness"] = get_display_brightness();
    resp["status"] = "ok";

    String output;
    serializeJson(resp, output);
    server.send(200, "application/json", output);
}

// ---- Handler: GET /api/display-power ----
static void handle_display_power_get(WebServer& server) {
    JsonDocument doc;
    doc["on"] = get_displays_on();
    doc["switch_position"] = get_power_switch_position();

    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", output);
}

// ---- Handler: POST /api/display-power ----
static void handle_display_power_post(WebServer& server) {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"no body\"}");
        return;
    }

    String body = server.arg("plain");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
        server.send(400, "application/json", "{\"error\":\"invalid JSON\"}");
        return;
    }

    if (!doc["on"].is<bool>()) {
        server.send(400, "application/json", "{\"error\":\"on field required\"}");
        return;
    }

    set_displays_on(doc["on"].as<bool>());

    JsonDocument resp;
    resp["on"] = get_displays_on();
    resp["status"] = "ok";

    String output;
    serializeJson(resp, output);
    server.send(200, "application/json", output);
}

// ---- Public: wire up all routes ----
void setup_routes(WebServer& server) {
    // Initialize LittleFS
    if (!LittleFS.begin()) {
        Serial.println("An error occurred while mounting LittleFS");
        return;
    }

    // Root: status page
    server.on("/", HTTP_GET, [&server]() {
        handle_file_read(server, "/index.html");
    });

    // JSON API endpoints
    server.on("/api/status", HTTP_GET, [&server]() {
        handle_status_get(server);
    });
    server.on("/api/wans", HTTP_POST, [&server]() {
        handle_wans_post(server);
    });
    server.on("/api/brightness", HTTP_GET, [&server]() {
        handle_brightness_get(server);
    });
    server.on("/api/brightness", HTTP_POST, [&server]() {
        handle_brightness_post(server);
    });
    server.on("/api/display-power", HTTP_GET, [&server]() {
        handle_display_power_get(server);
    });
    server.on("/api/display-power", HTTP_POST, [&server]() {
        handle_display_power_post(server);
    });

    // Favicons (still served from memory for speed)
    server.on("/favicon-green.svg", [&server]() {
        server.send(200, "image/svg+xml", FAVICON_GREEN);
    });
    server.on("/favicon-yellow.svg", [&server]() {
        server.send(200, "image/svg+xml", FAVICON_YELLOW);
    });
    server.on("/favicon-red.svg", [&server]() {
        server.send(200, "image/svg+xml", FAVICON_RED);
    });
    server.on("/favicon.svg", [&server]() {
        server.send(200, "image/svg+xml", FAVICON_GREEN);  // Default to green
    });
    server.on("/favicon.ico", [&server]() {
        server.send(204);
    });

    // Fallback for all other requests
    server.onNotFound([&server]() {
        handle_not_found(server);
    });
}
