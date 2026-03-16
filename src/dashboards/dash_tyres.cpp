#include <TFT_eSPI.h>
#include "telemetry.h"
#include <Arduino.h>

// Tyres dashboard: shows 4 tyre spots with temperature & wear.
// Since we don't parse tyre-specific fields yet, show placeholders from status.

void drawTyresDashboard(TFT_eSPI* tft, const TelemetryFrame &frame) {
  tft->setTextSize(2);
  tft->setTextColor(TFT_WHITE, TFT_BLACK);
  const int cx[4] = {60, 360, 60, 360};
  const int cy[4] = {40, 40, 220, 220};
  const char* labels[4] = {"FL","FR","RL","RR"};
  for (int i=0;i<4;i++) {
    int x = cx[i], y = cy[i];
    tft->drawRect(x-50, y-30, 100, 60, TFT_WHITE);
    tft->setCursor(x-20, y-10);
    tft->printf("%s", labels[i]);
    // Placeholder temperature & wear using ersEnergy & brakeBias
    int temp = map((int)frame.status.ersEnergy, 0, 1000, 60, 120);
    int wear = min(100, (int)frame.status.brakeBias);
    tft->setCursor(x-40, y+8);
    tft->printf("T:%3dC W:%3d%%", temp, wear);
  }
}
