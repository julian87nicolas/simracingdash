#include <TFT_eSPI.h>
#include "telemetry.h"
#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════
// DAMAGE / STATUS DASHBOARD
// Shows engine, fuel consumption, brake bias — placeholder data
// until detailed damage fields (wings, floor, etc.) are parsed.
// ═══════════════════════════════════════════════════════════════════

static bool bgDrawn = false;
static int prevFuel10 = -1;
static uint8_t prevBias = 0xFF;
static int prevErs = -1;

void resetDamageDashboardCache() {
  bgDrawn = false;
  prevFuel10 = -1; prevBias = 0xFF; prevErs = -1;
}

static void drawStatBar(TFT_eSPI* tft, int x, int y, int w, int h,
                        int val, int maxVal, uint16_t col) {
  tft->drawRect(x, y, w, h, 0x6B4D);
  tft->fillRect(x + 1, y + 1, w - 2, h - 2, 0x2104);
  int fill = (int)((long)val * (w - 2) / maxVal);
  if (fill > w - 2) fill = w - 2;
  if (fill > 0) tft->fillRect(x + 1, y + 1, fill, h - 2, col);
}

void drawDamageDashboard(TFT_eSPI* tft, const TelemetryFrame &frame) {
  if (!bgDrawn) {
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_ORANGE, TFT_BLACK);
    tft->drawString("DAMAGE / STATUS", 20, 8);
    tft->drawFastHLine(20, 35, 440, 0x4208);

    // Section labels
    tft->setTextFont(2); tft->setTextSize(1);
    tft->setTextColor(0x6B4D, TFT_BLACK);
    tft->drawString("FUEL LEVEL", 20, 50);
    tft->drawString("ERS ENERGY", 20, 120);
    tft->drawString("BRAKE BIAS",  20, 190);

    // Horizontal separator
    tft->drawFastHLine(20, 250, 440, 0x4208);

    // Status note
    tft->setTextColor(0x4208, TFT_BLACK);
    tft->setTextDatum(MC_DATUM);
    tft->drawString("Detailed wing/floor damage requires", 240, 262);
    tft->drawString("CarDamageData packet (not yet parsed)", 240, 278);
    bgDrawn = true;
  }

  int fuel10 = (int)(frame.status.fuel * 10);
  if (fuel10 != prevFuel10) {
    drawStatBar(tft, 20, 68, 380, 22, min(fuel10, 1100), 1100, TFT_GREEN);
    char buf[12];
    snprintf(buf, sizeof(buf), "%.1fL", (double)frame.status.fuel);
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->fillRect(410, 65, 65, 28, TFT_BLACK);
    tft->drawString(buf, 410, 67);
  }

  int ers = min(1000, (int)frame.status.ersEnergy);
  if (ers != prevErs) {
    drawStatBar(tft, 20, 138, 380, 22, ers, 1000, TFT_BLUE);
    char buf[8];
    snprintf(buf, sizeof(buf), "%u%%", (unsigned)(ers / 10));
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->fillRect(410, 135, 65, 28, TFT_BLACK);
    tft->drawString(buf, 410, 137);
  }

  uint8_t bias = frame.status.brakeBias;
  if (bias != prevBias) {
    // Bias bar 50-70 range
    int barW = 380;
    tft->fillRect(20, 208, barW, 22, 0x2104);
    int marker = map(bias, 50, 70, 0, barW);
    if (marker < 0) marker = 0;
    if (marker > barW - 6) marker = barW - 6;
    tft->fillRect(20 + marker, 206, 6, 26, TFT_YELLOW);
    char buf[8];
    snprintf(buf, sizeof(buf), "%u%%", (unsigned)bias);
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->fillRect(410, 205, 65, 28, TFT_BLACK);
    tft->drawString(buf, 410, 207);
  }

  prevFuel10 = fuel10; prevErs = ers; prevBias = bias;
}
