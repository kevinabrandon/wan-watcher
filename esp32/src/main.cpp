// main.cpp
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <ETH.h>
#include <WebServer.h>
#include <ESPmDNS.h>

#include "wifi_config.h"
#include "hostname.h"
#include "leds.h"
#include "http_routes.h"
#include "wan_metrics.h"
#include "display_config.h"
#include "local_pinger.h"

WebServer server(80);

// Ethernet configuration for Olimex ESP32-POE-ISO
#define ETH_CLK_MODE    ETH_CLOCK_GPIO17_OUT
#define ETH_POWER_PIN   12
#define ETH_TYPE        ETH_PHY_LAN8720
#define ETH_ADDR        0
#define ETH_MDC_PIN     23
#define ETH_MDIO_PIN    18

// Connection state
static bool g_eth_connected = false;
static bool g_wifi_connected = false;

// Network interface helpers (declared in hostname.h)
bool is_eth_connected() { return g_eth_connected; }
bool is_wifi_connected() { return g_wifi_connected; }

String get_network_ip() {
    if (g_eth_connected) {
        return ETH.localIP().toString();
    }
    return WiFi.localIP().toString();
}

String get_network_hostname() {
    // Both interfaces use the same hostname from build_hostname()
    // but we need to query the active interface
    if (g_eth_connected) {
        return String(ETH.getHostname());
    }
    return String(WiFi.getHostname());
}

// Display system configuration
static DisplaySystemConfig build_display_config() {
    DisplaySystemConfig config;
    config.mode = DisplayMode::PREFIX_LETTER;
    config.cycle_interval_ms = 5000;       // 5 second cycle
    config.auto_cycle_enabled = true;      // Auto-cycle by default
    config.base_address = 0x71;            // WAN1 packet=0x71, WAN1 bw=0x72, etc.

    // Button configuration (two buttons for independent control)
    // Button 1: controls packet display (L/J/P) - MCP pin 14
    config.button1_type = ButtonPinSource::MCP;
    config.button1_pin = 14;
    // Button 2: controls bandwidth display (d/U) - MCP pin 15
    config.button2_type = ButtonPinSource::MCP;
    config.button2_pin = 15;
    config.long_press_ms = 1000;

    // Indicator LED pins (only used in INDICATOR_LED mode)
    config.led_latency_pin = 8;
    config.led_jitter_pin = 9;
    config.led_loss_pin = 10;
    config.led_download_pin = 11;
    config.led_upload_pin = 12;
    return config;
}

// start mDNS
static void start_mdns(const char* hostname) {
    if (MDNS.begin(hostname)) {
        Serial.println("mDNS started");

        // Advertise HTTP service over mDNS
        MDNS.addService("http", "tcp", 80);

    } else {
        Serial.println("mDNS failed to start");
    }
}

// Ethernet event handler
static void eth_event(WiFiEvent_t event) {
    switch (event) {
        case ARDUINO_EVENT_ETH_START:
            Serial.println("ETH Started");
            ETH.setHostname(build_hostname().c_str());
            break;
        case ARDUINO_EVENT_ETH_CONNECTED:
            Serial.println("ETH Connected");
            break;
        case ARDUINO_EVENT_ETH_GOT_IP:
            Serial.print("ETH IP: ");
            Serial.println(ETH.localIP());
            Serial.printf("ETH Speed: %dMbps, %s\n",
                ETH.linkSpeed(),
                ETH.fullDuplex() ? "Full Duplex" : "Half Duplex");
            g_eth_connected = true;
            break;
        case ARDUINO_EVENT_ETH_DISCONNECTED:
            Serial.println("ETH Disconnected");
            g_eth_connected = false;
            break;
        case ARDUINO_EVENT_ETH_STOP:
            Serial.println("ETH Stopped");
            g_eth_connected = false;
            break;
        default:
            break;
    }
}

// Try to connect via Ethernet, returns true if successful
static bool try_ethernet(int timeout_ms) {
    Serial.println("Trying Ethernet connection...");

    WiFi.onEvent(eth_event);
    ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK_MODE);

    unsigned long start = millis();
    while (!g_eth_connected && (millis() - start < (unsigned long)timeout_ms)) {
        delay(100);
        g_led_status1.set(!g_led_status1.state());
    }

    return g_eth_connected;
}

// Try to connect via WiFi, returns true if successful
static bool try_wifi(int timeout_ms) {
    String hostname = build_hostname();
    Serial.printf("Trying WiFi connection to %s...\n", WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.setHostname(hostname.c_str());
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start < (unsigned long)timeout_ms)) {
        delay(100);
        g_led_status1.set(!g_led_status1.state());
    }

    if (WiFi.status() == WL_CONNECTED) {
        g_wifi_connected = true;
        // Disable WiFi power saving for lower latency
        esp_wifi_set_ps(WIFI_PS_NONE);
        return true;
    }

    return false;
}

// Connect to network: try Ethernet first, fall back to WiFi
static void connect_to_network_blocking() {
    g_led_status1.set(false);

    String hostname = build_hostname();
    Serial.printf("Hostname: %s\n", hostname.c_str());

    // Try Ethernet first (5 second timeout)
    if (try_ethernet(5000)) {
        Serial.println("Connected via Ethernet");
        g_led_status1.set(true);
        start_mdns(hostname.c_str());
        return;
    }

    Serial.println("Ethernet not connected, trying WiFi...");

    // Fall back to WiFi with retry loop
    for (;;) {
        if (try_wifi(30000)) {
            Serial.println("WiFi connected");
            Serial.print("IP address: ");
            Serial.println(WiFi.localIP());
            Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
            g_led_status1.set(true);
            start_mdns(hostname.c_str());
            return;
        }

        Serial.println("WiFi connect FAILED, retrying...");
        WiFi.disconnect(true);

        // Fast blink to indicate error
        for (int i = 0; i < 6; ++i) {
            g_led_status1.set(!g_led_status1.state());
            delay(250);
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println();
    Serial.println("ESP32 LED webserver starting...");

    // Initialize WAN metrics storage
    wan_metrics_init();

    // Initialize I2C, MCP23017, displays, and LEDs
    DisplaySystemConfig config = build_display_config();
    leds_init_with_displays(config);

    // Block here until network is up; g_led_status1 shows progress
    // Tries Ethernet first, falls back to WiFi
    connect_to_network_blocking();

    // Only now that WiFi is up, start HTTP server and routes
    setup_routes(server);
    server.begin();
    Serial.println("HTTP server started");

    // Initialize local pinger (needs WiFi to be up)
    local_pinger_init();
}

void loop() {
    server.handleClient();
    router_heartbeat_check();
    freshness_bar_update();
    display_update();

    // Update local pinger and its LEDs
    local_pinger_update();
    const LocalPingerMetrics& lp = local_pinger_get();
    local_pinger_set_leds(lp.state);
}
