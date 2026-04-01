#include "state.h"
#include <Arduino.h>
#include <cstring>

StateManager::StateManager() {
  memset(&curr, 0, sizeof(curr));
  memset(&prev, 0, sizeof(prev));
}

void StateManager::updateFrame(const TelemetryFrame &frame) {
  // move curr to prev, copy new into curr
  prev = curr;
  curr = frame;
}

bool StateManager::hasMFDChanged() const {
  return curr.telemetry.mfdPanelIndex != prev.telemetry.mfdPanelIndex;
}

bool StateManager::isInPit() const {
  return curr.lap.pitStatus != 0;
}

bool StateManager::hasERSChanged() const {
  return curr.status.ersEnergy != prev.status.ersEnergy;
}
