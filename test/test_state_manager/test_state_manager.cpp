#include <unity.h>
#include "state.h"

void test_constructor_zeroed() {
  StateManager sm;
  TEST_ASSERT_EQUAL_UINT16(0, sm.current().telemetry.speedKmh);
  TEST_ASSERT_EQUAL_UINT16(0, sm.previous().telemetry.speedKmh);
}

void test_current_previous() {
  StateManager sm;
  TelemetryFrame f1{};
  f1.telemetry.speedKmh = 200;
  sm.updateFrame(f1);
  TEST_ASSERT_EQUAL_UINT16(200, sm.current().telemetry.speedKmh);
  TEST_ASSERT_EQUAL_UINT16(0, sm.previous().telemetry.speedKmh);

  TelemetryFrame f2 = f1;
  f2.telemetry.speedKmh = 300;
  sm.updateFrame(f2);
  TEST_ASSERT_EQUAL_UINT16(300, sm.current().telemetry.speedKmh);
  TEST_ASSERT_EQUAL_UINT16(200, sm.previous().telemetry.speedKmh);
}

void test_state_manager() {
  StateManager sm;
  TelemetryFrame f1{};
  f1.telemetry.mfdPanelIndex = 0;
  f1.status.ersEnergy = 500;
  f1.lap.pitStatus = 0;
  sm.updateFrame(f1);

  TelemetryFrame f2 = f1;
  f2.telemetry.mfdPanelIndex = 2;
  f2.status.ersEnergy = 600;
  f2.lap.pitStatus = 1;
  sm.updateFrame(f2);

  TEST_ASSERT_TRUE(sm.hasMFDChanged());
  TEST_ASSERT_TRUE(sm.isInPit());
  TEST_ASSERT_TRUE(sm.hasERSChanged());
}

void test_no_change() {
  StateManager sm;
  TelemetryFrame f1{};
  f1.telemetry.mfdPanelIndex = 3;
  f1.status.ersEnergy = 500;
  f1.lap.pitStatus = 0;
  sm.updateFrame(f1);
  sm.updateFrame(f1);  // same frame again

  TEST_ASSERT_FALSE(sm.hasMFDChanged());
  TEST_ASSERT_FALSE(sm.isInPit());
  TEST_ASSERT_FALSE(sm.hasERSChanged());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_constructor_zeroed);
  RUN_TEST(test_current_previous);
  RUN_TEST(test_state_manager);
  RUN_TEST(test_no_change);
  return UNITY_END();
}
