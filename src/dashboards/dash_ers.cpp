#include <TFT_eSPI.h>
#include "telemetry.h"
#include <Arduino.h>

void drawERSDashboard(TFT_eSPI* tft, const TelemetryFrame &frame) {
  static bool bgDrawn = false;
  tft->setTextSize(2);
  tft->setTextColor(TFT_WHITE, TFT_BLACK);

  if (!bgDrawn) {
    tft->fillScreen(TFT_BLACK);
    tft->setTextColor(TFT_CYAN, TFT_BLACK);
    tft->setCursor(20, 10);
    tft->print("ERS");
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    bgDrawn = true;
  }

  // ERS energy bar
  int x = 20, y = 40, w = 440, h = 30;
  tft->drawRect(x, y, w, h, TFT_WHITE);
  int ers = min(1000, (int)frame.status.ersEnergy);
  int fill = map(ers, 0, 1000, 0, w);
  tft->fillRect(x+1, y+1, w-2, h-2, TFT_BLACK);
  if (fill > 2) {
    tft->fillRect(x+1, y+1, fill-2, h-2, TFT_BLUE);
  }

  tft->fillRect(20, y + 78, 440, 28, TFT_BLACK);
  tft->setCursor(30, y+40);
  tft->printf("ERS: %u/1000  Mode:%u", ers, frame.status.ersDeployMode);
}
