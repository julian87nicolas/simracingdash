#include <unity.h>
#include "state.h"

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

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_state_manager);
  return UNITY_END();
}
