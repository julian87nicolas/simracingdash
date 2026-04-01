#include <TFT_eSPI.h>
#include "telemetry.h"
#include <Arduino.h>

void drawDamageDashboard(TFT_eSPI* tft, const TelemetryFrame &frame) {
  static bool bgDrawn = false;
  tft->setTextColor(TFT_WHITE, TFT_BLACK);
  tft->setTextSize(2);

  if (!bgDrawn) {
    tft->fillScreen(TFT_BLACK);
    tft->setTextColor(TFT_ORANGE, TFT_BLACK);
    tft->setCursor(20, 10);
    tft->print("DAMAGE / STATUS");
    tft->drawFastHLine(20, 34, 440, TFT_DARKGREY);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    bgDrawn = true;
  }

  // Use placeholders: show fuel as "engine" and brakeBias as wing damage
  int engine = min(100, (int)map((int)frame.status.fuel, 0, 100, 0, 100));
  int wings = min(100, (int)frame.status.brakeBias);

  tft->fillRect(20, 45, 440, 28, TFT_BLACK);
  tft->setCursor(20, 40);
  tft->printf("Engine: %3d%%", engine);

  tft->fillRect(20, 85, 440, 28, TFT_BLACK);
  tft->setCursor(20, 80);
  tft->printf("Front Wing: %3d%%", wings);

  tft->fillRect(20, 125, 440, 28, TFT_BLACK);
  tft->setCursor(20, 120);
  tft->printf("Tyres: - (no detailed tyre damage parsed)");
}
