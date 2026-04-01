#include <TFT_eSPI.h>
#include "telemetry.h"
#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════
// DAMAGE / STATUS DASHBOARD
// Shows engine, fuel consumption, brake bias — placeholder data
// until detailed damage fields (wings, floor, etc.) are parsed.
// ═══════════════════════════════════════════════════════════════════

static bool bgDrawn = false;
static int8_t prevGear = 127;
static uint16_t prevSpeed = 0xFFFF;
static int prevFuel10 = -1;
static uint8_t prevBias = 0xFF;
static int prevErs = -1;

void resetDamageDashboardCache() {
  bgDrawn = false;
  prevGear = 127; prevSpeed = 0xFFFF;
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
    tft->drawString("DAMAGE / STATUS", 20, 4);
    tft->drawFastHLine(20, 28, 440, 0x4208);

    // Gear / Speed labels
    tft->setTextFont(2); tft->setTextSize(1);
    tft->setTextColor(0x6B4D, TFT_BLACK);
    tft->drawString("GEAR", 40, 32);
    tft->drawString("SPEED", 340, 32);

    tft->drawFastHLine(20, 82, 440, 0x4208);

    // Section labels
    tft->drawString("FUEL LEVEL", 20, 88);
    tft->drawString("ERS ENERGY", 20, 138);
    tft->drawString("BRAKE BIAS", 20, 188);

    tft->drawFastHLine(20, 232, 440, 0x4208);

    tft->setTextColor(0x4208, TFT_BLACK);
    tft->setTextDatum(MC_DATUM);
    tft->drawString("Detailed wing/floor damage requires", 240, 248);
    tft->drawString("CarDamageData packet (not yet parsed)", 240, 264);
    bgDrawn = true;
  }

  // ── Gear + Speed (prominent) ──
  int8_t gear = frame.telemetry.gear;
  if (gear != prevGear) {
    char gtxt[4];
    if (gear == 0) strcpy(gtxt, "N");
    else if (gear == -1) strcpy(gtxt, "R");
    else snprintf(gtxt, sizeof(gtxt), "%d", gear);
    uint16_t gc = (gear == 0) ? TFT_GREEN : (gear == -1) ? TFT_RED : TFT_WHITE;
    tft->setTextColor(gc, TFT_BLACK);
    tft->setTextFont(7); tft->setTextSize(1);
    tft->setTextDatum(MC_DATUM);
    tft->fillRect(20, 44, 110, 36, TFT_BLACK);
    tft->drawString(gtxt, 75, 62);
  }

  uint16_t speed = frame.telemetry.speedKmh;
  if (speed != prevSpeed) {
    char sbuf[8];
    snprintf(sbuf, sizeof(sbuf), "%3u", (unsigned)speed);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->setTextFont(7); tft->setTextSize(1);
    tft->setTextDatum(MR_DATUM);
    tft->fillRect(270, 44, 150, 36, TFT_BLACK);
    tft->drawString(sbuf, 415, 62);
    tft->setTextFont(2); tft->setTextSize(1);
    tft->setTextColor(0x6B4D, TFT_BLACK);
    tft->setTextDatum(ML_DATUM);
    tft->drawString("km/h", 420, 58);
  }

  // ── Fuel ──
  int fuel10 = (int)(frame.status.fuel * 10);
  if (fuel10 != prevFuel10) {
    drawStatBar(tft, 20, 104, 380, 20, min(fuel10, 1100), 1100, TFT_GREEN);
    char buf[12];
    snprintf(buf, sizeof(buf), "%.1fL", (double)frame.status.fuel);
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->fillRect(410, 101, 65, 26, TFT_BLACK);
    tft->drawString(buf, 410, 102);
  }

  // ── ERS ──
  int ers = min(1000, (int)frame.status.ersEnergy);
  if (ers != prevErs) {
    drawStatBar(tft, 20, 154, 380, 20, ers, 1000, TFT_BLUE);
    char buf[8];
    snprintf(buf, sizeof(buf), "%u%%", (unsigned)(ers / 10));
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->fillRect(410, 151, 65, 26, TFT_BLACK);
    tft->drawString(buf, 410, 152);
  }

  // ── Brake bias ──
  uint8_t bias = frame.status.brakeBias;
  if (bias != prevBias) {
    int barW = 380;
    tft->fillRect(20, 204, barW, 20, 0x2104);
    int marker = map(bias, 50, 70, 0, barW);
    if (marker < 0) marker = 0;
    if (marker > barW - 6) marker = barW - 6;
    tft->fillRect(20 + marker, 202, 6, 24, TFT_YELLOW);
    char buf[8];
    snprintf(buf, sizeof(buf), "%u%%", (unsigned)bias);
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->fillRect(410, 201, 65, 26, TFT_BLACK);
    tft->drawString(buf, 410, 202);
  }

  prevGear = gear; prevSpeed = speed;
  prevFuel10 = fuel10; prevErs = ers; prevBias = bias;
}
