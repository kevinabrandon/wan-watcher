// ota.h
//
// Over-the-air updates (firmware and LittleFS image) via ArduinoOTA.
// The OTA password is compiled in from the WW_OTA_PASSWORD environment
// variable (see platformio.ini); OTA stays disabled if it is empty.
#pragma once

// Start the OTA listener. Call once the network is up and mDNS has started.
void ota_init(const char* hostname);

// Service OTA requests. Call from loop().
void ota_handle();

// True while an update is being received.
bool ota_in_progress();
