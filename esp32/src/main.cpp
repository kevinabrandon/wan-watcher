// main.cpp
#include <Arduino.h>
#include <ETH.h>
#include <WebServer.h>
#include <ESPmDNS.h>

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

// Network interface helpers (declared in hostname.h)
bool is_eth_connected() { return g_eth_connected; }

String get_network_ip() {
    return ETH.localIP().toString();
}

String get_network_hostname() {
    return String(ETH.getHostname());
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

// Connect to Ethernet, blocking until connected
static void connect_ethernet_blocking() {
    g_led_status1.set(false);

    String hostname = build_hostname();
    Serial.printf("Hostname: %s\n", hostname.c_str());
    Serial.println("Connecting via Ethernet...");

    WiFi.onEvent(eth_event);
    ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK_MODE);

    // Wait for connection, blinking status LED
    while (!g_eth_connected) {
        delay(100);
        g_led_status1.set(!g_led_status1.state());
    }

    Serial.println("Ethernet connected");
    g_led_status1.set(true);
    start_mdns(hostname.c_str());
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

    // Initialize physical power switch (MCP pin 13)
    power_switch_init();

    // Initialize brightness potentiometer (GPIO 36 / VP)
    g_brightness_pot.begin(36);

    // Block here until Ethernet is up; g_led_status1 shows progress
    connect_ethernet_blocking();

    // Start HTTP server and routes
    setup_routes(server);
    server.begin();
    Serial.println("HTTP server started");

    // Initialize local pinger (needs network to be up)
    local_pinger_init();
}

void loop() {
    // Handle Ethernet status LED
    // Blinks when disconnected (overrides display power switch)
    // Solid when connected (respects display power switch)
    static unsigned long last_eth_blink_ms = 0;
    if (!g_eth_connected) {
        if (millis() - last_eth_blink_ms >= 100) {
            last_eth_blink_ms = millis();
            g_led_status1.set(!g_led_status1.state());
        }
    } else {
        g_led_status1.set(get_displays_on());
    }

    server.handleClient();
    power_switch_update();
    g_brightness_pot.update();
    router_heartbeat_check();
    freshness_bar_update();
    display_update();

    // Update local pinger and its LEDs
    local_pinger_update();
    const LocalPingerMetrics& lp = local_pinger_get();
    local_pinger_set_leds(lp.state);
}
