#pragma once
#include <cstddef>
#include <cstdint>

class WiFiUDP {
public:
  bool begin(uint16_t) { return true; }
  int parsePacket() { return 0; }
  int read(uint8_t* buf, size_t len) { (void)buf; (void)len; return 0; }
};
