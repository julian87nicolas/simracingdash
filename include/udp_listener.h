#pragma once
#include <Arduino.h>

// Initialize UDP listener and read incoming packets without heap allocations.
bool udp_init(uint16_t port);
// Read a packet into provided buffer. Returns length or 0 if none.
size_t udp_read_packet(uint8_t* buffer, size_t maxlen);
