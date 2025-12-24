// main.cpp
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

#include "wifi_config.h"
#include "hostname.h"
#include "leds.h"
#include "http_routes.h"
#include "wan_metrics.h"

WebServer server(80);

// start mDNS
void startMDNS(const char* hostname) {
    if (MDNS.begin(hostname)) {
        Serial.println("mDNS started");

        // Advertise HTTP service over mDNS
        MDNS.addService("http", "tcp", 80);

    } else {
        Serial.println("mDNS failed to start");
    }
}

// Blocking WiFi connect with infinite retry + status LED blink
static void connectToWiFiBlocking() {
    led_status1.set(false);  // start off

    String hostname = build_hostname();
    Serial.printf("Hostname: %s\n", hostname.c_str());
    Serial.printf("Target SSID: %s\n", WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.setHostname(hostname.c_str());

    for (;;) {  // forever loop until connected
        Serial.println("Starting WiFi connection attempt...");
        WiFi.disconnect(true);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

        int attempts = 0;

        // Try for ~30s (60 * 500ms) on this attempt
        while (WiFi.status() != WL_CONNECTED && attempts < 60) {
            delay(500);
            attempts++;

            // Blink status LED while attempting
            led_status1.set(!led_status1.state());

            Serial.print(".");
        }

        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("WiFi connected");
            Serial.print("IP address: ");
            Serial.println(WiFi.localIP());

            // Connected: LED ON
            led_status1.set(true);
            Serial.printf("Starting mDNS at %s.local\n", hostname.c_str());
            startMDNS(hostname.c_str());
            return;
        }

        // This attempt failed: indicate error (fast blink a few times)
        Serial.println("WiFi connect FAILED, retrying...");
        for (int i = 0; i < 6; ++i) {  // ~1.5s of fast blink
            led_status1.set(!led_status1.state());
            delay(250);
        }

        // Loop back and try again
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println();
    Serial.println("ESP32 LED webserver starting...");

    // Initialize WAN metrics storage
    wan_metrics_init();

    // Initialize I2C, MCP23017, and all LEDs
    leds_init();

    // Block here until WiFi is actually up; led_status1 shows progress
    connectToWiFiBlocking();

    // Only now that WiFi is up, start HTTP server and routes
    setup_routes(server);
    server.begin();
    Serial.println("HTTP server started");
}

void loop() {
    server.handleClient();
    wan1_heartbeat_check();
    display_update();
}
