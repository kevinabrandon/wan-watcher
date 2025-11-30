// hostname.cpp
#include "hostname.h"
#include <esp_system.h>   // esp_read_mac

String build_hostname() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);   // MAC for WiFi station

    // last 3 bytes of MAC: mac[3], mac[4], mac[5]
    char buf[64];

    // format to lowercase hex
    snprintf(buf, sizeof(buf),
             "wan-watcher-%02x%02x%02x",
             mac[3], mac[4], mac[5]);

    return String(buf);
}
