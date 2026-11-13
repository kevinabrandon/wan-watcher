// ota.cpp
#include "ota.h"

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <LittleFS.h>

#ifndef OTA_PASSWORD
#define OTA_PASSWORD ""
#endif

static const uint16_t OTA_PORT = 3232;

static bool g_ota_enabled = false;
static bool g_ota_in_progress = false;

void ota_init(const char* hostname) {
    if (strlen(OTA_PASSWORD) == 0) {
        Serial.println("OTA disabled: no password compiled in (set WW_OTA_PASSWORD before building)");
        return;
    }

    ArduinoOTA.setHostname(hostname);
    ArduinoOTA.setPort(OTA_PORT);
    ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA.setMdnsEnabled(false);   // mDNS is already running; advertise below

    ArduinoOTA.onStart([]() {
        g_ota_in_progress = true;
        bool fs = (ArduinoOTA.getCommand() == U_SPIFFS);
        if (fs) {
            LittleFS.end();             // filesystem must be unmounted while its partition is rewritten
        }
        Serial.printf("OTA start: %s\n", fs ? "filesystem" : "firmware");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        static unsigned int last_pct = 100;
        unsigned int pct = total ? (progress * 100U) / total : 0;
        if (pct / 10 != last_pct / 10) {
            last_pct = pct;
            Serial.printf("OTA %u%%\n", pct);
        }
    });

    ArduinoOTA.onEnd([]() {
        g_ota_in_progress = false;
        Serial.println("OTA complete, rebooting");
    });

    ArduinoOTA.onError([](ota_error_t error) {
        g_ota_in_progress = false;
        const char* msg = "unknown";
        switch (error) {
            case OTA_AUTH_ERROR:    msg = "auth failed"; break;
            case OTA_BEGIN_ERROR:   msg = "begin failed"; break;
            case OTA_CONNECT_ERROR: msg = "connect failed"; break;
            case OTA_RECEIVE_ERROR: msg = "receive failed"; break;
            case OTA_END_ERROR:     msg = "end failed"; break;
        }
        Serial.printf("OTA error %d: %s\n", (int)error, msg);
        // A failed filesystem update left LittleFS unmounted; the device keeps
        // running but the web UI cannot serve files until the next reboot.
    });

    ArduinoOTA.begin();
    MDNS.enableArduino(OTA_PORT, true);
    g_ota_enabled = true;
    Serial.printf("OTA ready on port %u\n", OTA_PORT);
}

void ota_handle() {
    if (g_ota_enabled) {
        ArduinoOTA.handle();
    }
}

bool ota_in_progress() {
    return g_ota_in_progress;
}
