#include <cassert>
#include <iostream>
#include "../../include/telemetry.h"
#include "include/TFT_eSPI.h"

// drawMainDashboard is defined in src/dashboards/dash_main.cpp; declare it here
extern void drawMainDashboard(TFT_eSPI* tft, const TelemetryFrame &frame);

int main() {
  TFT_eSPI tft;
  TelemetryFrame f{};
  f.telemetry.speedKmh = 200;
  f.telemetry.rpm = 12000;
  f.telemetry.gear = 7;
  f.telemetry.throttle = 250;
  f.telemetry.brake = 0;

  drawMainDashboard(&tft, f);

  assert(tft.call_count > 0);
  std::cout << "dashboard main render test passed\n";
  return 0;
}
