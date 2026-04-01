#include <TFT_eSPI.h>
#include "telemetry.h"
#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════
// ENGINE WEAR DASHBOARD — MFD 4
// Shows % wear of each engine component (MGU-H, SAE, EC, MCI, MGU-K, TC, GEAR)
// ═══════════════════════════════════════════════════════════════════

static bool bgDrawn = false;
static int8_t prevGear = 127;
static uint16_t prevSpeed = 0xFFFF;
static uint8_t prevWear[7] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

void resetEngineDashboardCache() {
  bgDrawn = false;
  prevGear = 127; prevSpeed = 0xFFFF;
  memset(prevWear, 0xFF, 7);
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
    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(0x6B4D, TFT_BLACK);
    tft->drawString("GEAR", 155, 55);
    tft->drawString("SPEED", 235, 55);
    tft->drawFastHLine(20, 82, 440, 0x4208);

    // Component labels (7 rows, 30px each, starting at y=86)
    const char* labels[] = {"MGU-H", "SAE", "EC", "MCI", "MGU-K", "TC", "GEAR"};
    for (int i = 0; i < 7; ++i) {
      tft->setTextFont(4); tft->setTextSize(1);
      tft->setTextDatum(ML_DATUM);
      tft->setTextColor(0x6B4D, TFT_BLACK);
      tft->drawString(labels[i], 20, 97 + i * 30);
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

  // ── 7 engine wear bars (raw = wear %: 0=new, 100=worn) ──
  const uint8_t rawWear[] = {
    frame.damage.engineMGUHWear,
    frame.damage.engineESWear,
    frame.damage.engineCEWear,
    frame.damage.engineICEWear,
    frame.damage.engineMGUKWear,
    frame.damage.engineTCWear,
    frame.damage.gearBoxDamage
  };
  for (int i = 0; i < 7; ++i) {
    uint8_t wear = (rawWear[i] > 100) ? 100 : rawWear[i];
    if (wear != prevWear[i]) {
      int y = 88 + i * 30;
      drawWearBar(tft, 100, y, 300, 18, wear);
      char buf[8];
      snprintf(buf, sizeof(buf), "%u%%", (unsigned)wear);
      tft->setTextFont(4); tft->setTextSize(1);
      tft->setTextDatum(TL_DATUM);
      tft->setTextColor(TFT_WHITE, TFT_BLACK);
      tft->fillRect(410, y - 2, 65, 22, TFT_BLACK);
      tft->drawString(buf, 410, y);
      prevWear[i] = wear;
    }
  }

  prevGear = gear; prevSpeed = speed;
}
