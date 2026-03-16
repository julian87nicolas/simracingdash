#pragma once
#include <Arduino.h>
#include "telemetry.h"

// State manager stores current and previous telemetry frames and provides helpers
class StateManager {
public:
  StateManager();

  // Update current frame (called after parsing a new UDP packet)
  void updateFrame(const TelemetryFrame &frame);

  // Accessors
  const TelemetryFrame& current() const { return curr; }
  const TelemetryFrame& previous() const { return prev; }

  // Helpers
  bool hasMFDChanged() const;
  bool isInPit() const;
  bool hasERSChanged() const;

private:
  TelemetryFrame curr;
  TelemetryFrame prev;
};
