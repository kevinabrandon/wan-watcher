// http_routes.cpp
#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "http_routes.h"
#include "leds.h"
#include "wan_metrics.h"

// ---- Helper: WAN status line with colored circles ----
static String wan_state_line_html(int wan_id) {
    const WanMetrics& m = wan_metrics_get(wan_id);

    String prefix = "WAN" + String(wan_id);

    switch (m.state) {
        case WanState::UP:
            return "&#x1F7E2; <strong>" + prefix + ": UP</strong> &#x1F7E2;";
        case WanState::DEGRADED:
            return "&#x1F7E1; <strong>" + prefix + ": DEGRADED</strong> &#x1F7E1;";
        case WanState::DOWN:
        default:
            return "&#x1F534; <strong>" + prefix + ": DOWN</strong> &#x1F534;";
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

// ---- Helper: WAN metrics table row ----
static String wan_metrics_row_html(int wan_id) {
    const WanMetrics& m = wan_metrics_get(wan_id);

    String html = "<tr>";
    html += "<td>WAN" + String(wan_id) + "</td>";
    html += "<td>" + String(wan_state_to_string(m.state)) + "</td>";
    html += "<td>" + String(m.loss_pct) + "%</td>";
    html += "<td>" + String(m.latency_ms) + " ms</td>";
    html += "<td>" + String(m.jitter_ms) + " ms</td>";
    html += "<td>" + String(m.down_mbps, 1) + " Mbps</td>";
    html += "<td>" + String(m.up_mbps, 1) + " Mbps</td>";
    html += "<td>" + last_update_human(wan_id) + "</td>";
    html += "</tr>";

    return html;
}

// ---- Main page HTML ----
static String root_page_html() {
    String hostname = WiFi.getHostname();
    String ip = WiFi.localIP().toString();

    String html = R"(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta http-equiv="refresh" content="10">
  <title>wan-watcher</title>
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

    html += "<h2 class=\"status\">";
    html += wan_state_line_html(1);
    html += "</h2>";

    html += "<p><strong>Hostname:</strong> <code>" + hostname + "</code><br>";
    html += "<strong>IP:</strong> <code>" + ip + "</code></p>";

    // Metrics table
    html += R"(
<h3>WAN Metrics</h3>
<table>
  <tr>
    <th>WAN</th>
    <th>State</th>
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
    html += "</table>";

    html += R"(
<hr>

<h3>JSON API</h3>
<p>POST metrics as JSON to update WAN state:</p>

<pre><code>curl -X POST -H "Content-Type: application/json" \
  -d '{"state":"up","loss_pct":0,"latency_ms":6,"jitter_ms":0,"down_mbps":2.0,"up_mbps":3.3}' \
  http://)";
    html += hostname;
    html += R"(/api/wan1</code></pre>

<p>Endpoints:</p>
<ul>
  <li><code>POST /api/wan1</code> - Update WAN1 metrics</li>
  <li><code>POST /api/wan2</code> - Update WAN2 metrics</li>
  <li><code>POST /api/wans</code> - Update all WANs (batch)</li>
</ul>

<hr>

<h3>LED Mapping (MCP23017)</h3>
<ul>
  <li>MCP pin )";
    html += String(led_wan1_up.pin());
    html += R"( - WAN1 UP (green)</li>
  <li>MCP pin )";
    html += String(led_wan1_degraded.pin());
    html += R"( - WAN1 DEGRADED (yellow)</li>
  <li>MCP pin )";
    html += String(led_wan1_down.pin());
    html += R"( - WAN1 DOWN (red)</li>
</ul>

<hr>
<p>Reload this page to see the current state reflected.</p>

</body></html>
)";

    return html;
}

// ---- 404 handler ----
static void handle_not_found(WebServer& server) {
    server.send(404, "text/plain", "Not found");
}

// ---- Helper: Build JSON response for a WAN ----
static String make_wan_json(int wan_id) {
    const WanMetrics& m = wan_metrics_get(wan_id);

    JsonDocument doc;
    doc["status"] = "ok";

    JsonObject wan = doc["wan" + String(wan_id)].to<JsonObject>();
    wan["state"] = wan_state_to_string(m.state);
    wan["loss_pct"] = m.loss_pct;
    wan["latency_ms"] = m.latency_ms;
    wan["jitter_ms"] = m.jitter_ms;
    wan["down_mbps"] = m.down_mbps;
    wan["up_mbps"] = m.up_mbps;

    String output;
    serializeJson(doc, output);
    return output;
}

// ---- Helper: Parse JSON and update WAN metrics ----
static bool parse_wan_json(const String& body, int wan_id) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
        Serial.printf("JSON parse error: %s\n", error.c_str());
        return false;
    }

    const char* state_str = doc["state"] | "down";
    WanState state = wan_state_from_string(state_str);
    uint8_t loss_pct = doc["loss_pct"] | 100;
    uint16_t latency_ms = doc["latency_ms"] | 0;
    uint16_t jitter_ms = doc["jitter_ms"] | 0;
    float down_mbps = doc["down_mbps"] | 0.0f;
    float up_mbps = doc["up_mbps"] | 0.0f;

    wan_metrics_update(wan_id, state, loss_pct, latency_ms, jitter_ms, down_mbps, up_mbps);

    // Update LEDs for WAN1
    if (wan_id == 1) {
        wan1_set_leds(state);
    }

    Serial.printf("WAN%d updated: state=%s loss=%d%% lat=%dms\n",
                  wan_id, state_str, loss_pct, latency_ms);

    return true;
}

// ---- Handler: POST /api/wan1 or /api/wan2 ----
static void handle_wan_post(WebServer& server, int wan_id) {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"no body\"}");
        return;
    }

    String body = server.arg("plain");

    if (parse_wan_json(body, wan_id)) {
        server.send(200, "application/json", make_wan_json(wan_id));
    } else {
        server.send(400, "application/json", "{\"error\":\"invalid JSON\"}");
    }
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

    // Process wan1 if present
    if (doc["wan1"].is<JsonObject>()) {
        JsonObject wan1 = doc["wan1"];
        String wan1_json;
        serializeJson(wan1, wan1_json);
        parse_wan_json(wan1_json, 1);
    }

    // Process wan2 if present
    if (doc["wan2"].is<JsonObject>()) {
        JsonObject wan2 = doc["wan2"];
        String wan2_json;
        serializeJson(wan2, wan2_json);
        parse_wan_json(wan2_json, 2);
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

    // JSON API endpoints
    server.on("/api/wan1", HTTP_POST, [&server]() {
        handle_wan_post(server, 1);
    });

    server.on("/api/wan2", HTTP_POST, [&server]() {
        handle_wan_post(server, 2);
    });

    server.on("/api/wans", HTTP_POST, [&server]() {
        handle_wans_post(server);
    });

    // Favicon: silence noise
    server.on("/favicon.ico", [&server]() {
        server.send(204);
    });

    server.onNotFound([&server]() {
        handle_not_found(server);
    });
}
