#include <TFT_eSPI.h>
#include "telemetry.h"
#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════
// SETUP DASHBOARD — MFD 0
// Brake bias, ERS deploy mode, ERS energy store
// ═══════════════════════════════════════════════════════════════════

static bool bgDrawn = false;
static int8_t prevGear = 127;
static uint16_t prevSpeed = 0xFFFF;
static uint8_t prevBias = 0xFF;
static uint8_t prevMode = 0xFF;
static int prevErs = -1;
static uint8_t prevDiff = 0xFF;

void resetSetupDashboardCache() {
  bgDrawn = false;
  prevGear = 127; prevSpeed = 0xFFFF;
  prevBias = 0xFF; prevMode = 0xFF; prevErs = -1;
  prevDiff = 0xFF;
}

static void drawHBar(TFT_eSPI* tft, int x, int y, int w, int h,
                     int val, int maxVal, uint16_t fillCol) {
  tft->drawRect(x, y, w, h, 0x6B4D);
  int fill = (int)((long)val * (w - 2) / maxVal);
  if (fill > w - 2) fill = w - 2;
  tft->fillRect(x + 1, y + 1, w - 2, h - 2, 0x2104);
  if (fill > 0) tft->fillRect(x + 1, y + 1, fill, h - 2, fillCol);
}

void drawSetupDashboard(TFT_eSPI* tft, const TelemetryFrame &frame) {
  if (!bgDrawn) {
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_CYAN, TFT_BLACK);
    tft->drawString("SETUP", 20, 4);
    tft->drawFastHLine(20, 28, 440, 0x4208);

    tft->setTextFont(2); tft->setTextSize(1);
    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(0x6B4D, TFT_BLACK);
    tft->drawString("GEAR", 155, 55);
    tft->drawString("SPEED", 235, 55);
    tft->setTextDatum(TL_DATUM);
    tft->drawFastHLine(20, 82, 440, 0x4208);

    tft->drawString("BRAKE BIAS", 20, 88);
    tft->drawString("DIFFERENTIAL", 20, 148);
    tft->drawString("ERS DEPLOY MODE", 20, 208);
    tft->drawString("ERS ENERGY STORE", 20, 256);
    bgDrawn = true;
  }

  // ── Gear + Speed ──
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
    tft->fillRect(20, 30, 110, 50, TFT_BLACK);
    tft->drawString(gtxt, 75, 55);
  }
  uint16_t speed = frame.telemetry.speedKmh;
  if (speed != prevSpeed) {
    char sbuf[8];
    snprintf(sbuf, sizeof(sbuf), "%3u", (unsigned)speed);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->setTextFont(7); tft->setTextSize(1);
    tft->setTextDatum(MR_DATUM);
    tft->fillRect(270, 30, 150, 50, TFT_BLACK);
    tft->drawString(sbuf, 415, 55);
    tft->setTextFont(2); tft->setTextSize(1);
    tft->setTextColor(0x6B4D, TFT_BLACK);
    tft->setTextDatum(ML_DATUM);
    tft->drawString("km/h", 420, 51);
  }

  // ── Brake bias ──
  uint8_t bias = frame.status.brakeBias;
  if (bias != prevBias) {
    // Wide bar 50-70 range
    int barW = 380;
    tft->fillRect(20, 106, barW, 22, 0x2104);
    int marker = map(bias, 50, 70, 0, barW);
    if (marker < 0) marker = 0;
    if (marker > barW - 6) marker = barW - 6;
    tft->fillRect(20 + marker, 104, 6, 26, TFT_YELLOW);
    // Labels at ends
    tft->setTextFont(2); tft->setTextSize(1);
    tft->setTextColor(0x6B4D, TFT_BLACK);
    tft->setTextDatum(TL_DATUM);
    tft->drawString("FRONT", 20, 130);
    tft->setTextDatum(TR_DATUM);
    tft->drawString("REAR", 400, 130);
    // Value
    char buf[8];
    snprintf(buf, sizeof(buf), "%u%%", (unsigned)bias);
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->fillRect(410, 104, 65, 28, TFT_BLACK);
    tft->drawString(buf, 410, 106);
  }

  // ── Differential (single on-throttle value, matching MFD) ──
  uint8_t diff = frame.status.diffOnThrottle;
  if (diff != prevDiff) {
    uint8_t displayDiff = (diff > 100) ? 100 : diff;
    drawHBar(tft, 20, 166, 380, 20, displayDiff, 100, TFT_CYAN);
    char buf[8];
    snprintf(buf, sizeof(buf), "%u%%", (unsigned)diff);
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->fillRect(410, 164, 65, 28, TFT_BLACK);
    tft->drawString(buf, 410, 166);
  }

  // ── ERS deploy mode ──
  uint8_t mode = frame.status.ersDeployMode;
  if (mode != prevMode) {
    const char* names[] = {"NONE", "MEDIUM", "HOTLAP", "OVERTAKE"};
    const uint16_t cols[] = {TFT_DARKGREY, TFT_CYAN, TFT_YELLOW, TFT_RED};
    uint8_t mi = (mode <= 3) ? mode : 0;
    tft->fillRect(20, 226, 440, 28, TFT_BLACK);
    tft->fillRoundRect(20, 226, 16 + strlen(names[mi]) * 14, 26, 6, cols[mi]);
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(ML_DATUM);
    tft->setTextColor(TFT_BLACK, cols[mi]);
    tft->drawString(names[mi], 28, 239);
  }

  // ── ERS energy bar ──
  int ers = min(1000, (int)frame.status.ersEnergy);
  if (ers != prevErs) {
    drawHBar(tft, 20, 274, 380, 20, ers, 1000, TFT_BLUE);
    char buf[8];
    snprintf(buf, sizeof(buf), "%u%%", (unsigned)(ers / 10));
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->fillRect(410, 272, 65, 26, TFT_BLACK);
    tft->drawString(buf, 410, 274);
  }

  prevGear = gear; prevSpeed = speed;
  prevBias = bias; prevMode = mode; prevErs = ers;
  prevDiff = diff;
}
