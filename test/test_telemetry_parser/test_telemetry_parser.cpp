#include <unity.h>
#include "telemetry.h"
#include "telemetry_parser.h"

void test_telemetry_parser() {
  uint8_t buf[64];
  memset(buf, 0, sizeof(buf));
  buf[5] = F1Packets::PACKET_ID_CAR_TELEMETRY;
  buf[24] = 123 & 0xFF; buf[25] = (123 >> 8) & 0xFF;
  buf[26] = 7500 & 0xFF; buf[27] = (7500 >> 8) & 0xFF;
  buf[32] = 200; buf[33] = 10;
  buf[36] = 4; buf[37] = 1; buf[38] = 1;

  TelemetryFrame out{};
  telemetry_parse(buf, sizeof(buf), out);

  TEST_ASSERT_EQUAL_UINT16(123, out.telemetry.speedKmh);
  TEST_ASSERT_EQUAL_UINT16(7500, out.telemetry.rpm);
  TEST_ASSERT_EQUAL_UINT8(200, out.telemetry.throttle);
  TEST_ASSERT_EQUAL_UINT8(10, out.telemetry.brake);
  TEST_ASSERT_EQUAL_INT8(4, out.telemetry.gear);
  TEST_ASSERT_TRUE(out.telemetry.drsActive);
  TEST_ASSERT_EQUAL_UINT8(1, out.telemetry.mfdPanelIndex);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_telemetry_parser);
  return UNITY_END();
}
