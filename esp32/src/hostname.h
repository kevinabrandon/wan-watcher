// hostname.h
#pragma once
#include <Arduino.h>

// Returns a hostname like "wan-watcher-F024F90D4DE8"
String build_hostname();

// Network interface helpers (implemented in main.cpp)
bool is_eth_connected();
String get_network_ip();
String get_network_hostname();
