#include <cassert>
#include <cstring>
#include <iostream>
#include "../include/telemetry.h"
#include "../include/telemetry_parser.h"

int main() {
  // Build a fake CarTelemetry packet buffer
  uint8_t buf[64];
  memset(buf, 0, sizeof(buf));
  buf[5] = F1Packets::PACKET_ID_CAR_TELEMETRY;
  // speed = 123 km/h -> uint16 little-endian at offset 24
  buf[24] = 123 & 0xFF;
  buf[25] = (123 >> 8) & 0xFF;
  // rpm = 7500 -> uint16 at offset 26
  buf[26] = 7500 & 0xFF;
  buf[27] = (7500 >> 8) & 0xFF;
  // throttle and brake
  buf[32] = 200; // throttle
  buf[33] = 10;  // brake
  // gear
  buf[36] = 4;
  // drs
  buf[37] = 1;
  // mfd panel
  buf[38] = 1;

  TelemetryFrame out{};
  telemetry_parse(buf, sizeof(buf), out);

  assert(out.telemetry.speedKmh == 123);
  assert(out.telemetry.rpm == 7500);
  assert(out.telemetry.throttle == 200);
  assert(out.telemetry.brake == 10);
  assert(out.telemetry.gear == 4);
  assert(out.telemetry.drsActive == true);
  assert(out.telemetry.mfdPanelIndex == 1);

  std::cout << "telemetry_parser test passed\n";
  return 0;
}
