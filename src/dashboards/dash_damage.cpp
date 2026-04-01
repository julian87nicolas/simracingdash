#include <TFT_eSPI.h>
#include "telemetry.h"
#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════
// ENGINE WEAR DASHBOARD — MFD 4
// Shows % wear of each engine component (ICE, TC, MGUH, MGUK, ES, CE)
// ═══════════════════════════════════════════════════════════════════

static bool bgDrawn = false;
static int8_t prevGear = 127;
static uint16_t prevSpeed = 0xFFFF;
static uint8_t prevWear[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

void resetEngineDashboardCache() {
  bgDrawn = false;
  prevGear = 127; prevSpeed = 0xFFFF;
  memset(prevWear, 0xFF, 6);
}

static void drawWearBar(TFT_eSPI* tft, int x, int y, int w, int h,
                        uint8_t wear) {
  tft->drawRect(x, y, w, h, 0x6B4D);
  tft->fillRect(x + 1, y + 1, w - 2, h - 2, 0x2104);
  if (wear > 100) wear = 100;
  int fill = (int)((long)wear * (w - 2) / 100);
  uint16_t col = (wear < 40) ? TFT_GREEN : (wear < 70) ? TFT_YELLOW : TFT_RED;
  if (fill > 0) tft->fillRect(x + 1, y + 1, fill, h - 2, col);
}

void drawEngineDashboard(TFT_eSPI* tft, const TelemetryFrame &frame) {
  if (!bgDrawn) {
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_ORANGE, TFT_BLACK);
    tft->drawString("ENGINE WEAR", 20, 4);
    tft->drawFastHLine(20, 28, 440, 0x4208);

    tft->setTextFont(2); tft->setTextSize(1);
    tft->setTextColor(0x6B4D, TFT_BLACK);
    tft->drawString("GEAR", 40, 32);
    tft->drawString("SPEED", 340, 32);
    tft->drawFastHLine(20, 82, 440, 0x4208);

    // Component labels (6 rows, 34px each, starting at y=86)
    const char* labels[] = {"ICE", "TC", "MGU-H", "MGU-K", "ES", "CE"};
    for (int i = 0; i < 6; ++i) {
      tft->setTextFont(4); tft->setTextSize(1);
      tft->setTextDatum(ML_DATUM);
      tft->setTextColor(0x6B4D, TFT_BLACK);
      tft->drawString(labels[i], 20, 98 + i * 34);
    }
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

  // ── 6 engine wear bars ──
  const uint8_t wears[] = {
    frame.damage.engineICEWear,
    frame.damage.engineTCWear,
    frame.damage.engineMGUHWear,
    frame.damage.engineMGUKWear,
    frame.damage.engineESWear,
    frame.damage.engineCEWear
  };
  for (int i = 0; i < 6; ++i) {
    if (wears[i] != prevWear[i]) {
      int y = 88 + i * 34;
      drawWearBar(tft, 100, y, 300, 20, wears[i]);
      char buf[8];
      snprintf(buf, sizeof(buf), "%u%%", (unsigned)wears[i]);
      tft->setTextFont(4); tft->setTextSize(1);
      tft->setTextDatum(TL_DATUM);
      tft->setTextColor(TFT_WHITE, TFT_BLACK);
      tft->fillRect(410, y - 2, 65, 26, TFT_BLACK);
      tft->drawString(buf, 410, y);
      prevWear[i] = wears[i];
    }
  }

  prevGear = gear; prevSpeed = speed;
}
