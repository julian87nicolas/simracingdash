#include <cassert>
#include <iostream>
#include "../../include/telemetry.h"
#include "include/TFT_eSPI.h"

// drawMainDashboard is defined in src/dashboards/dash_main.cpp; declare it here
extern void drawMainDashboard(TFT_eSPI* tft, const TelemetryFrame &frame);

int main() {
  // Test that rendering calls TFT methods
  TFT_eSPI tft;
  TelemetryFrame f{};
  f.telemetry.speedKmh = 200;
  f.telemetry.rpm = 12000;
  f.telemetry.gear = 7;
  f.telemetry.throttle = 250;
  f.telemetry.brake = 0;

  int before = tft.call_count;
  drawMainDashboard(&tft, f);
  assert(tft.call_count > before);
  std::cout << "PASS: drawMainDashboard renders (" << (tft.call_count - before) << " TFT calls)\n";

  // Test with different data to ensure it doesn't crash
  TFT_eSPI tft2;
  TelemetryFrame f2{};
  f2.telemetry.speedKmh = 0;
  f2.telemetry.rpm = 0;
  f2.telemetry.gear = -1;   // reverse
  f2.telemetry.throttle = 0;
  f2.telemetry.brake = 255;
  f2.telemetry.drsActive = true;
  drawMainDashboard(&tft2, f2);
  assert(tft2.call_count > 0);
  std::cout << "PASS: drawMainDashboard edge values (reverse, full brake)\n";

  std::cout << "All dashboard tests passed\n";
  return 0;
}
