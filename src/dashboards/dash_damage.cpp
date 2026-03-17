#include <TFT_eSPI.h>
#include "telemetry.h"
#include <Arduino.h>

void drawDamageDashboard(TFT_eSPI* tft, const TelemetryFrame &frame) {
  tft->setTextColor(TFT_WHITE, TFT_BLACK);
  tft->setTextSize(2);
  // Use placeholders: show fuel as "engine" and brakeBias as wing damage
  int engine = min(100, (int)map((int)frame.status.fuel, 0, 100, 0, 100));
  int wings = min(100, (int)frame.status.brakeBias);
  tft->setCursor(20, 40);
  tft->printf("Engine: %3d%%", engine);
  tft->setCursor(20, 80);
  tft->printf("Front Wing: %3d%%", wings);
  tft->setCursor(20, 120);
  tft->printf("Tyres: - (no detailed tyre damage parsed)");
}
