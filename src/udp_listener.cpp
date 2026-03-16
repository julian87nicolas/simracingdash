#include "udp_listener.h"
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

static WiFiUDP udp;
static uint16_t listenPort = 0;

bool udp_init(uint16_t port) {
  listenPort = port;
  if (!udp.begin(listenPort)) {
    return false;
  }
  return true;
}

size_t udp_read_packet(uint8_t* buffer, size_t maxlen) {
  int packetSize = udp.parsePacket();
  if (packetSize <= 0) return 0;
  if ((size_t)packetSize > maxlen) packetSize = (int)maxlen;
  int read = udp.read(buffer, packetSize);
  if (read < 0) return 0;
  return (size_t)read;
}
