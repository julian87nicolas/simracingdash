#include <cassert>
#include <iostream>
#include "../include/state.h"

int main() {
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

  assert(sm.hasMFDChanged() == true);
  assert(sm.isInPit() == true);
  assert(sm.hasERSChanged() == true);

  std::cout << "state_manager tests passed\n";
  return 0;
}
