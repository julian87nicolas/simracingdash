#pragma once
#include <Arduino.h>

bool udp_init(uint16_t port);
size_t udp_read_packet(uint8_t* buffer, size_t maxlen);
