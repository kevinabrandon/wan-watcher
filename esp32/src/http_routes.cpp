// http_routes.cpp
#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "http_routes.h"
#include "hostname.h"
#include "leds.h"
#include "wan_metrics.h"
#include "local_pinger.h"

// ---- Favicon SVG ----
static const char* FAVICON_SVG = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32">
  <circle cx="16" cy="16" r="14" fill="#2ecc71"/>
  <text x="16" y="16" text-anchor="middle" font-size="12" font-weight="700" fill="#ffffff" font-family="system-ui, sans-serif">W</text>
  <text x="16" y="24" text-anchor="middle" font-size="12" font-weight="700" fill="#ffffff" font-family="system-ui, sans-serif">W</text>
</svg>)";

// ---- Helper: State cell with colored circle ----
static String state_cell_html(WanState state) {
    switch (state) {
        case WanState::UP:
            return "&#x1F7E2; UP";
        case WanState::DEGRADED:
            return "&#x1F7E1; DEGRADED";
        case WanState::DOWN:
        default:
            return "&#x1F534; DOWN";
    }
}

// ---- Helper: makes human readable last updated string ----
static String last_update_human(int wan_id) {
    const WanMetrics& m = wan_metrics_get(wan_id);

    if (m.last_update_ms == 0) {
        return "never";
    }

    unsigned long now = millis();
    unsigned long elapsed = now - m.last_update_ms;
    unsigned long secs = elapsed / 1000UL;

    if (secs < 60) {
        return String(secs) + "s ago";
    }

    unsigned long mins = secs / 60;
    if (mins < 60) {
        unsigned long rem_s = secs % 60;
        String s = String(mins) + "m";
        if (rem_s > 0) {
            s += " " + String(rem_s) + "s";
        }
        s += " ago";
        return s;
    }

    unsigned long hours = mins / 60;
    unsigned long rem_m = mins % 60;
    String s = String(hours) + "h";
    if (rem_m > 0) {
        s += " " + String(rem_m) + "m";
    }
    s += " ago";
    return s;
}

// ---- Helper: WAN description (TODO: make configurable) ----
static String wan_description(int wan_id) {
    switch (wan_id) {
        case 1: return "PeakWifi";
        case 2: return "Starlink";
        default: return "";
    }
}

// ---- Helper: WAN metrics table row ----
static String wan_metrics_row_html(int wan_id) {
    const WanMetrics& m = wan_metrics_get(wan_id);

    String html = "<tr>";
    html += "<td>WAN" + String(wan_id) + "</td>";
    html += "<td>" + wan_description(wan_id) + "</td>";
    html += "<td>" + state_cell_html(m.state) + "</td>";
    html += "<td>" + String(m.monitor_ip) + "</td>";
    html += "<td>" + String(m.gateway_ip) + "</td>";
    html += "<td>" + String(m.local_ip) + "</td>";
    html += "<td>" + String(m.loss_pct) + "%</td>";
    html += "<td>" + String(m.latency_ms) + " ms</td>";
    html += "<td>" + String(m.jitter_ms) + " ms</td>";
    html += "<td>" + String(m.down_mbps, 1) + " Mbps</td>";
    html += "<td>" + String(m.up_mbps, 1) + " Mbps</td>";
    html += "<td>" + last_update_human(wan_id) + "</td>";
    html += "</tr>";

    return html;
}

// ---- Helper: Local pinger last updated string ----
static String local_pinger_last_update_human() {
    const LocalPingerMetrics& m = local_pinger_get();

    if (m.last_update_ms == 0) {
        return "never";
    }

    unsigned long now = millis();
    unsigned long elapsed = now - m.last_update_ms;
    unsigned long secs = elapsed / 1000UL;

    if (secs < 60) {
        return String(secs) + "s ago";
    }

    unsigned long mins = secs / 60;
    if (mins < 60) {
        unsigned long rem_s = secs % 60;
        String s = String(mins) + "m";
        if (rem_s > 0) {
            s += " " + String(rem_s) + "s";
        }
        s += " ago";
        return s;
    }

    unsigned long hours = mins / 60;
    unsigned long rem_m = mins % 60;
    String s = String(hours) + "h";
    if (rem_m > 0) {
        s += " " + String(rem_m) + "m";
    }
    s += " ago";
    return s;
}

// ---- Helper: Local pinger metrics table row ----
static String local_pinger_metrics_row_html() {
    const LocalPingerMetrics& m = local_pinger_get();

    String html = "<tr>";
    html += "<td>Local</td>";
    html += "<td>" + get_network_hostname() + "</td>";
    html += "<td>" + state_cell_html(m.state) + "</td>";
    html += "<td>" + String(local_pinger_get_target()) + "</td>";  // Monitor target
    html += "<td>" + String(wan_metrics_get_router_ip()) + "</td>";  // Router IP
    html += "<td>" + get_network_ip() + "</td>";  // ESP32's IP
    html += "<td>" + String(m.loss_pct) + "%</td>";
    html += "<td>" + String(m.latency_ms) + " ms</td>";
    html += "<td>" + String(m.jitter_ms) + " ms</td>";
    html += "<td>-</td>";  // No download for local pinger
    html += "<td>-</td>";  // No upload for local pinger
    html += "<td>" + local_pinger_last_update_human() + "</td>";
    html += "</tr>";

    return html;
}

// ---- Main page HTML ----
static String root_page_html() {
    String hostname = get_network_hostname();

    String html = R"(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta http-equiv="refresh" content="10">
  <title>wan-watcher</title>
  <link rel="icon" type="image/svg+xml" href="/favicon.svg">
  <style>
    body {
      font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      margin: 1.5rem;
    }
    h1 { margin-bottom: 0.5rem; }
    .status { margin-top: 0.5rem; margin-bottom: 1.5rem; }
    code { background: #f5f5f5; padding: 2px 4px; border-radius: 3px; }
    table { border-collapse: collapse; margin: 1rem 0; }
    th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
    th { background: #f5f5f5; }
  </style>
</head>
<body>
<h1>wan-watcher</h1>
)";

    html += "<p><strong>Hostname:</strong> <code>" + hostname + "</code><br>";

    const char* timestamp = wan_metrics_get_timestamp();
    if (timestamp[0] != '\0') {
        html += "<strong>Last update:</strong> <code id=\"last-update\">" + String(timestamp) + "</code>";
    }
    html += "</p>";
    html += R"(<script>
(function() {
  var el = document.getElementById('last-update');
  if (el) {
    var d = new Date(el.textContent);
    if (!isNaN(d)) el.textContent = d.toLocaleString();
  }
})();
</script>)";

    // Metrics table
    html += R"(
<h3>Network Metrics</h3>
<table>
  <tr>
    <th>Interface</th>
    <th>Description</th>
    <th>State</th>
    <th>Monitor IP</th>
    <th>Gateway IP</th>
    <th>Local IP</th>
    <th>Loss</th>
    <th>Latency</th>
    <th>Jitter</th>
    <th>Download</th>
    <th>Upload</th>
    <th>Last Update</th>
  </tr>
)";
    html += wan_metrics_row_html(1);
    html += wan_metrics_row_html(2);
    html += local_pinger_metrics_row_html();
    html += "</table>";

    html += R"(
</body></html>
)";

    return html;
}

// ---- 404 handler ----
static void handle_not_found(WebServer& server) {
    server.send(404, "text/plain", "Not found");
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

// ---- Public: wire up all routes ----
void setup_routes(WebServer& server) {
    // Root: status page
    server.on("/", [&server]() {
        server.send(200, "text/html", root_page_html());
    });

    // JSON API endpoint (batch only)
    server.on("/api/wans", HTTP_POST, [&server]() {
        handle_wans_post(server);
    });

    // Favicon
    server.on("/favicon.svg", [&server]() {
        server.send(200, "image/svg+xml", FAVICON_SVG);
    });
    server.on("/favicon.ico", [&server]() {
        server.send(204);  // Some browsers still request .ico
    });

    server.onNotFound([&server]() {
        handle_not_found(server);
    });
}
