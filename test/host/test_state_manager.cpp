#include <cassert>
#include <iostream>
#include <cstring>
#include "../../include/state.h"

int main() {
  // Test constructor: both frames should be zeroed
  StateManager sm;
  assert(sm.current().telemetry.speedKmh == 0);
  assert(sm.current().telemetry.rpm == 0);
  assert(sm.previous().telemetry.speedKmh == 0);
  std::cout << "PASS: constructor zeroes frames\n";

  // Test current()/previous() after first update
  TelemetryFrame f1{};
  memset(&f1, 0, sizeof(f1));
  f1.telemetry.speedKmh = 200;
  f1.telemetry.mfdPanelIndex = 0;
  f1.status.ersEnergy = 500;
  f1.lap.pitStatus = 0;
  sm.updateFrame(f1);

  assert(sm.current().telemetry.speedKmh == 200);
  assert(sm.previous().telemetry.speedKmh == 0);  // prev was zero
  std::cout << "PASS: current/previous after first update\n";

  // Test MFD change, pit status, ERS change
  TelemetryFrame f2 = f1;
  f2.telemetry.speedKmh = 250;
  f2.telemetry.mfdPanelIndex = 2;
  f2.status.ersEnergy = 600;
  f2.lap.pitStatus = 1;
  sm.updateFrame(f2);

  assert(sm.current().telemetry.speedKmh == 250);
  assert(sm.previous().telemetry.speedKmh == 200);
  assert(sm.hasMFDChanged() == true);
  assert(sm.isInPit() == true);
  assert(sm.hasERSChanged() == true);
  std::cout << "PASS: hasMFDChanged/isInPit/hasERSChanged\n";

  // Test no-change scenario
  TelemetryFrame f3 = f2;
  sm.updateFrame(f3);

  assert(sm.hasMFDChanged() == false);
  assert(sm.isInPit() == true);  // still in pit
  assert(sm.hasERSChanged() == false);
  std::cout << "PASS: no-change scenario\n";

  // Test leaving pit
  TelemetryFrame f4 = f3;
  f4.lap.pitStatus = 0;
  sm.updateFrame(f4);
  assert(sm.isInPit() == false);
  std::cout << "PASS: leaving pit\n";

  std::cout << "All state_manager tests passed\n";
  return 0;
}
