// http_routes.cpp
#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include "http_routes.h"
#include "leds.h"

// ---- Helper: WAN1 state string (plain) ----
static String wan1_state_string() {
    switch (wan1_get_state()) {
        case WAN1_UP:       return "UP";
        case WAN1_DEGRADED: return "DEGRADED";
        case WAN1_DOWN:
        default:            return "DOWN";
    }
}

// ---- Helper: WAN1 status line with colored circles ----
static String wan1_state_line_html() {
    switch (wan1_get_state()) {
        case WAN1_UP:
            // green circles
            return "🟢 <strong>WAN1 state: UP</strong> 🟢";

        case WAN1_DEGRADED:
            // yellow circles
            return "🟡 <strong>WAN1 state: DEGRADED</strong> 🟡";

        case WAN1_DOWN:
        default:
            // red circles
            return "🔴 <strong>WAN1 state: DOWN</strong> 🔴";
    }
}

// ---- Helper: makes human readable last updated string ----
static String wan1_last_update_human() {
    unsigned long last = wan1_last_update_ms();
    if (last == 0) {
        return "never";
    }

    unsigned long now = millis();
    unsigned long elapsed = now - last;
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

// ---- Main page HTML (read-only status) ----
static String root_page_html() {
    // Get current hostname and IP from WiFi stack
    String hostname = WiFi.getHostname();
    String ip = WiFi.localIP().toString();

    String html = R"(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>wan-watcher</title>
  <style>
    body {
      font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI",
                   sans-serif;
      margin: 1.5rem;
    }
    h1 {
      margin-bottom: 0.5rem;
    }
    .status {
      margin-top: 0.5rem;
      margin-bottom: 1.5rem;
    }
    code {
      background: #f5f5f5;
      padding: 2px 4px;
      border-radius: 3px;
    }
  </style>
</head>
<body>
<h1>wan-watcher</h1>
)";

    html += "<h2 class=\"status\">";
    html += wan1_state_line_html();
    html += "</h2>";

    html += "<p><strong>Hostname:</strong> <code>" + hostname + "</code><br>";
    html += "<strong>IP:</strong> <code>" + ip + "</code><br>";
    html += "<strong>Last update:</strong> " + wan1_last_update_human() + "</p>";

    html += R"(
<hr>

<h3>REST-ish API (for pfSense / curl)</h3>
<p>WAN1 state can only be changed via HTTP <code>POST</code>:</p>

<ul>
  <li><code>POST /api/wan1/up</code></li>
  <li><code>POST /api/wan1/degraded</code></li>
  <li><code>POST /api/wan1/down</code></li>
</ul>

<p>Examples using this device's current hostname and IP:</p>
<ul>
  <li>By hostname: <code>curl -X POST http://)";

    html += hostname;
    html += R"(/api/wan1/up</code></li>
  <li>By IP: <code>curl -X POST http://)";

    html += ip;
    html += R"(/api/wan1/up</code></li>
</ul>

<hr>

<h3>LED Mapping (MCP23017)</h3>
<ul>
  <li>MCP pin )";
    html += String(led_wan1_up.pin());
    html += R"( → WAN1 UP (green)</li>
  <li>MCP pin )";
    html += String(led_wan1_degraded.pin());
    html += R"( → WAN1 DEGRADED (yellow)</li>
  <li>MCP pin )";
    html += String(led_wan1_down.pin());
    html += R"( → WAN1 DOWN (red)</li>
  <li>MCP pin )";
    html += String(led_heartbeat.pin());
    html += R"( → Heartbeat</li>
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

// ---- Helper: JSON response builder ----
static String make_state_json() {
    String json = "{";
    json += "\"status\":\"ok\",";
    json += "\"wan1_state\":\"";
    json += wan1_state_string();
    json += "\"}";
    return json;
}

// ---- Public: wire up all routes ----
void setup_routes(WebServer& server) {
    // Root: read-only status
    server.on("/", [&server]() {
        server.send(200, "text/html", root_page_html());
    });

    // --- WAN1 REST-ish API (POST only) ---

    server.on("/api/wan1/up", HTTP_POST, [&server]() {
        wan1_record_update();
        wan1_set_state(WAN1_UP);
        server.send(200, "application/json", make_state_json());
    });

    server.on("/api/wan1/degraded", HTTP_POST, [&server]() {
        wan1_record_update();
        wan1_set_state(WAN1_DEGRADED);
        server.send(200, "application/json", make_state_json());
    });

    server.on("/api/wan1/down", HTTP_POST, [&server]() {
        wan1_record_update();
        wan1_set_state(WAN1_DOWN);
        server.send(200, "application/json", make_state_json());
    });

    // Favicon: silence "handler not found" noise
    server.on("/favicon.ico", [&server]() {
        server.send(204);  // No Content
    });

    server.onNotFound([&server]() {
        handle_not_found(server);
    });
}

