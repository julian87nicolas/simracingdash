#include <unity.h>
#include "telemetry.h"
#include "TFT_eSPI.h"

extern void drawMainDashboard(TFT_eSPI* tft, const TelemetryFrame &frame);

void test_dash_main_render() {
  TFT_eSPI tft;
  TelemetryFrame f{};
  f.telemetry.speedKmh = 200;
  f.telemetry.rpm = 12000;
  f.telemetry.gear = 7;
  f.telemetry.throttle = 250;
  f.telemetry.brake = 0;

  int before = tft.call_count;
  drawMainDashboard(&tft, f);
  TEST_ASSERT_GREATER_THAN(before, tft.call_count);
}

void test_dash_main_edge_values() {
  TFT_eSPI tft;
  TelemetryFrame f{};
  f.telemetry.speedKmh = 0;
  f.telemetry.rpm = 0;
  f.telemetry.gear = -1;
  f.telemetry.throttle = 0;
  f.telemetry.brake = 255;
  f.telemetry.drsActive = true;

  drawMainDashboard(&tft, f);
  TEST_ASSERT_GREATER_THAN(0, tft.call_count);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_dash_main_render);
  RUN_TEST(test_dash_main_edge_values);
  return UNITY_END();
}
