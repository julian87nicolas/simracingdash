#include <TFT_eSPI.h>
#include "telemetry.h"
#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════
// TYRE WEAR DASHBOARD — MFD 2
// 4 tyre boxes with actual wear data from CarDamage packet
// ═══════════════════════════════════════════════════════════════════

static bool bgDrawn = false;
static int8_t prevGear = 127;
static uint16_t prevSpeed = 0xFFFF;
static uint8_t prevWear[4] = {0xFF,0xFF,0xFF,0xFF};
static uint8_t prevCompound = 0xFF;
static uint8_t prevAge = 0xFF;

void resetTyresDashboardCache() {
  bgDrawn = false;
  prevGear = 127; prevSpeed = 0xFFFF;
  memset(prevWear, 0xFF, 4);
  prevCompound = 0xFF; prevAge = 0xFF;
}

static void drawTyreBox(TFT_eSPI* tft, int cx, int cy,
                        const char* label, uint8_t wear) {
  const int bw = 110, bh = 70;
  int x = cx - bw / 2, y = cy - bh / 2;

  uint16_t wearCol = (wear < 30) ? TFT_GREEN : (wear < 60) ? TFT_YELLOW : TFT_RED;

  tft->drawRect(x, y, bw, bh, 0x6B4D);
  tft->fillRect(x + 1, y + 1, bw - 2, 6, wearCol);
  tft->fillRect(x + 1, y + 7, bw - 2, bh - 8, TFT_BLACK);

  // Label
  tft->setTextFont(4); tft->setTextSize(1);
  tft->setTextDatum(MC_DATUM);
  tft->setTextColor(TFT_WHITE, TFT_BLACK);
  tft->drawString(label, cx, cy - 12);

  // Wear %
  char buf[8];
  snprintf(buf, sizeof(buf), "%u%%", (unsigned)wear);
  tft->setTextFont(4); tft->setTextSize(1);
  tft->setTextColor(wearCol, TFT_BLACK);
  tft->drawString(buf, cx, cy + 14);
}

void drawTyresDashboard(TFT_eSPI* tft, const TelemetryFrame &frame) {
  if (!bgDrawn) {
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_GREEN, TFT_BLACK);
    tft->drawString("TYRES", 20, 4);
    tft->drawFastHLine(20, 28, 440, 0x4208);

    // Car body outline
    tft->drawRect(170, 50, 140, 220, 0x4208);

    // Labels inside car outline
    tft->setTextFont(2); tft->setTextSize(1);
    tft->setTextColor(0x6B4D, TFT_BLACK);
    tft->setTextDatum(MC_DATUM);
    tft->drawString("GEAR", 210, 72);
    tft->drawString("SPEED", 270, 72);
    tft->drawString("COMPOUND", 215, 170);
    tft->drawString("AGE", 265, 170);
    bgDrawn = true;
  }

  // ── Gear + Speed inside car outline (redraw both together to avoid overlap) ──
  int8_t gear = frame.telemetry.gear;
  uint16_t speed = frame.telemetry.speedKmh;
  if (gear != prevGear || speed != prevSpeed) {
    // Clear the entire gear+speed row at once
    tft->fillRect(172, 80, 136, 50, TFT_BLACK);
    // Gear (left half)
    char gtxt[4];
    if (gear == 0) strcpy(gtxt, "N");
    else if (gear == -1) strcpy(gtxt, "R");
    else snprintf(gtxt, sizeof(gtxt), "%d", gear);
    uint16_t gc = (gear == 0) ? TFT_GREEN : (gear == -1) ? TFT_RED : TFT_WHITE;
    tft->setTextColor(gc, TFT_BLACK);
    tft->setTextFont(7); tft->setTextSize(1);
    tft->setTextDatum(MC_DATUM);
    tft->drawString(gtxt, 205, 105);
    // Speed (right half)
    char sbuf[8];
    snprintf(sbuf, sizeof(sbuf), "%u", (unsigned)speed);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(MC_DATUM);
    tft->drawString(sbuf, 270, 100);
    tft->setTextFont(2); tft->setTextSize(1);
    tft->setTextColor(0x6B4D, TFT_BLACK);
    tft->drawString("km/h", 270, 120);
  }

  // ── Compound + Age in car center ──
  uint8_t compound = frame.status.tyreCompound;
  if (compound != prevCompound) {
    const char* names[] = {"S","M","H","I","W","?"};
    const uint16_t cols[] = {TFT_RED,TFT_YELLOW,TFT_WHITE,TFT_GREEN,TFT_BLUE,TFT_DARKGREY};
    int ci = (compound==16)?0:(compound==17)?1:(compound==18)?2:(compound==7)?3:(compound==8)?4:5;
    tft->fillRect(185, 182, 50, 30, TFT_BLACK);
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(cols[ci], TFT_BLACK);
    tft->drawString(names[ci], 215, 197);
  }
  uint8_t age = frame.status.tyresAgeLaps;
  if (age != prevAge) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%uL", (unsigned)age);
    tft->fillRect(245, 182, 50, 30, TFT_BLACK);
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->drawString(buf, 265, 197);
  }

  // ── 4 tyre boxes with actual wear ──
  const char* names[] = {"FL", "FR", "RL", "RR"};
  const int cx[] = {80, 400, 80, 400};
  const int cy[] = {95, 95, 225, 225};
  for (int i = 0; i < 4; ++i) {
    uint8_t w = frame.damage.tyresWear[i];
    if (w != prevWear[i]) {
      drawTyreBox(tft, cx[i], cy[i], names[i], w);
      prevWear[i] = w;
    }
  }

  prevGear = gear; prevSpeed = speed;
  prevCompound = compound; prevAge = age;
}
