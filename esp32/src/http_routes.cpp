// http_routes.cpp
#include <Arduino.h>
#include <WebServer.h>
#include "http_routes.h"
#include "leds.h"

// ---- Helper: generate redirect page ----
static String redirect_page(const String& message) {
    String html = R"(
<!DOCTYPE html>
<html>
<head>
<meta http-equiv="refresh" content="3; url=/" />
<title>Redirecting...</title>
</head>
<body>
<h3>)";
    html += message;
    html += R"(</h3>
<p>Returning to main page in 3 seconds...</p>
</body>
</html>
)";
    return html;
}

// ---- Helper: human-readable WAN1 state ----
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
            // green circles around the word
            return "🟢 <strong>WAN1 state: UP</strong> 🟢";

        case WAN1_DEGRADED:
            // yellow
            return "🟡 <strong>WAN1 state: DEGRADED</strong> 🟡";

        case WAN1_DOWN:
        default:
            // red
            return "🔴 <strong>WAN1 state: DOWN</strong> 🔴";
    }
}

// ---- Main page HTML ----
static String root_page_html() {
    String html = R"(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>wan-watcher</title>
</head>
<body>
<h1>wan-watcher (single WAN)</h1>
)";

    html += "<h2 style=\"font-family: system-ui, sans-serif;\">";
    html += wan1_state_line_html();
    html += "</h2>";

    html += R"(
<h3>Controls (manual for now)</h3>
<ul>
  <li><a href="/wan1/up">WAN1 UP (green)</a></li>
  <li><a href="/wan1/degraded">WAN1 DEGRADED (yellow)</a></li>
  <li><a href="/wan1/down">WAN1 DOWN (red)</a></li>
</ul>

<hr>
<p>LED mapping:</p>
<ul>
  <li>GPIO )";
    html += String(LED_WAN1_UP_PIN);
    html += R"( → WAN1 UP (green)</li>
  <li>GPIO )";
    html += String(LED_WAN1_DEGRADED_PIN);
    html += R"( → WAN1 DEGRADED (yellow)</li>
  <li>GPIO )";
    html += String(LED_WAN1_DOWN_PIN);
    html += R"( → WAN1 DOWN (red)</li>
</ul>

<p>Reload this page to see the current state reflected.</p>
</body></html>
)";

    return html;
}

// ---- 404 handler ----
static void handle_not_found(WebServer& server) {
    server.send(404, "text/plain", "Not found");
}

// ---- Public: wire up all routes ----
void setup_routes(WebServer& server) {
    // Root
    server.on("/", [&server]() {
        server.send(200, "text/html", root_page_html());
    });

    // WAN1 state endpoints
    server.on("/wan1/up", [&server]() {
        wan1_set_state(WAN1_UP);
        server.send(200, "text/html", redirect_page("WAN1 set to UP"));
    });

    server.on("/wan1/degraded", [&server]() {
        wan1_set_state(WAN1_DEGRADED);
        server.send(200, "text/html", redirect_page("WAN1 set to DEGRADED"));
    });

    server.on("/wan1/down", [&server]() {
        wan1_set_state(WAN1_DOWN);
        server.send(200, "text/html", redirect_page("WAN1 set to DOWN"));
    });

    // Favicon: silence the "handler not found" spam
    server.on("/favicon.ico", [&server]() {
        server.send(204);  // No Content
    });

    server.onNotFound([&server]() {
        handle_not_found(server);
    });
}
