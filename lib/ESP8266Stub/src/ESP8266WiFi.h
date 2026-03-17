// Minimal ESP8266WiFi stub for native tests
#pragma once
#include <cstdint>
#include <string>

enum wl_status_t { WL_CONNECTED, WL_DISCONNECTED };

class IPAddress {
public:
  std::string toString() const { return "127.0.0.1"; }
};

class WiFiClass {
public:
  void mode(int) {}
  void begin(const char*, const char*) {}
  wl_status_t status() { return WL_CONNECTED; }
  IPAddress localIP() { return IPAddress(); }
} ;

extern WiFiClass WiFi;
