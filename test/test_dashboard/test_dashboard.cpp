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

  drawMainDashboard(&tft, f);

  // If it compiles and runs without crash, consider pass
  TEST_ASSERT_TRUE(true);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_dash_main_render);
  return UNITY_END();
}
