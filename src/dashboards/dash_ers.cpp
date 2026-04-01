#include <TFT_eSPI.h>
#include "telemetry.h"
#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════
// ERS / ENGINE DASHBOARD
// Layout (480×300 usable):
//   Title bar  "ERS / ENGINE"
//   Large ERS energy bar with percentage
//   Deploy mode indicator with color
//   Fuel remaining bar + liters
//   Brake bias
// ═══════════════════════════════════════════════════════════════════

static bool bgDrawn = false;
static int prevErs = -1;
static uint8_t prevMode = 0xFF;
static int prevFuel10 = -1;
static uint8_t prevBias = 0xFF;

void resetERSDashboardCache() {
  bgDrawn = false;
  prevErs = -1; prevMode = 0xFF; prevFuel10 = -1; prevBias = 0xFF;
}

// Helper: horizontal bar with outline
static void drawHBar(TFT_eSPI* tft, int x, int y, int w, int h,
                     int val, int maxVal, uint16_t fillCol) {
  tft->drawRect(x, y, w, h, 0x6B4D);
  int fill = (int)((long)val * (w - 2) / maxVal);
  if (fill > w - 2) fill = w - 2;
  tft->fillRect(x + 1, y + 1, w - 2, h - 2, 0x2104);
  if (fill > 0) tft->fillRect(x + 1, y + 1, fill, h - 2, fillCol);
}

void drawERSDashboard(TFT_eSPI* tft, const TelemetryFrame &frame) {
  if (!bgDrawn) {
    // Title
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_CYAN, TFT_BLACK);
    tft->drawString("ERS / ENGINE", 20, 8);
    tft->drawFastHLine(20, 35, 440, 0x4208);

    // Section labels
    tft->setTextFont(2); tft->setTextSize(1);
    tft->setTextColor(0x6B4D, TFT_BLACK);
    tft->drawString("ENERGY STORE", 20, 50);
    tft->drawString("DEPLOY MODE", 20, 128);
    tft->drawString("FUEL IN TANK", 20, 188);
    tft->drawString("BRAKE BIAS", 20, 248);
    bgDrawn = true;
  }

  int ers = min(1000, (int)frame.status.ersEnergy);
  if (ers != prevErs) {
    // ERS bar (wide, prominent)
    drawHBar(tft, 20, 68, 380, 24, ers, 1000, TFT_BLUE);
    // Percentage text
    char buf[8];
    snprintf(buf, sizeof(buf), "%u%%", (unsigned)(ers / 10));
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->fillRect(410, 65, 65, 28, TFT_BLACK);
    tft->drawString(buf, 410, 67);
  }

  uint8_t mode = frame.status.ersDeployMode;
  if (mode != prevMode) {
    const char* names[] = {"NONE", "MEDIUM", "HOTLAP", "OVERTAKE"};
    const uint16_t cols[] = {TFT_DARKGREY, TFT_CYAN, TFT_YELLOW, TFT_RED};
    uint8_t mi = (mode <= 3) ? mode : 0;
    // Mode badge
    tft->fillRect(20, 146, 300, 32, TFT_BLACK);
    tft->fillRoundRect(20, 146, 16 + strlen(names[mi]) * 14, 30, 6, cols[mi]);
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(ML_DATUM);
    tft->setTextColor(TFT_BLACK, cols[mi]);
    tft->drawString(names[mi], 28, 161);
  }

  int fuel10 = (int)(frame.status.fuel * 10);
  if (fuel10 != prevFuel10) {
    drawHBar(tft, 20, 206, 380, 24, min(fuel10, 1100), 1100, TFT_GREEN);
    char buf[12];
    snprintf(buf, sizeof(buf), "%.1fL", (double)frame.status.fuel);
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->fillRect(410, 203, 65, 28, TFT_BLACK);
    tft->drawString(buf, 410, 205);
  }

  uint8_t bias = frame.status.brakeBias;
  if (bias != prevBias) {
    // Visual: a small left/right indicator
    tft->fillRect(20, 266, 200, 24, TFT_BLACK);
    int barW = 160;
    int biasX = 20;
    tft->fillRect(biasX, 268, barW, 18, 0x2104);
    int marker = map(bias, 50, 70, 0, barW); // typical range 50-70
    if (marker < 0) marker = 0;
    if (marker > barW - 4) marker = barW - 4;
    tft->fillRect(biasX + marker, 266, 4, 22, TFT_YELLOW);
    // Value
    char buf[8];
    snprintf(buf, sizeof(buf), "%u%%", (unsigned)bias);
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->fillRect(200, 263, 65, 28, TFT_BLACK);
    tft->drawString(buf, 200, 265);
  }

  prevErs = ers; prevMode = mode; prevFuel10 = fuel10; prevBias = bias;
}
