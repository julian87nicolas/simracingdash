#include <TFT_eSPI.h>
#include "telemetry.h"
#include <Arduino.h>

void drawERSDashboard(TFT_eSPI* tft, const TelemetryFrame &frame) {
  tft->setTextSize(2);
  tft->setTextColor(TFT_WHITE, TFT_BLACK);
  // ERS energy bar
  int x = 20, y = 40, w = 440, h = 30;
  tft->drawRect(x, y, w, h, TFT_WHITE);
  int ers = min(1000, (int)frame.status.ersEnergy);
  int fill = map(ers, 0, 1000, 0, w);
  tft->fillRect(x+1, y+1, fill-2, h-2, TFT_BLUE);
  tft->setCursor(30, y+40);
  tft->printf("ERS: %u/1000  Mode:%u", ers, frame.status.ersDeployMode);
}
