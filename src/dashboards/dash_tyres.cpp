#include <TFT_eSPI.h>
#include "telemetry.h"
#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════
// TYRES DASHBOARD
// Shows 4 tyre boxes arranged as a car viewed from above.
// Note: we don't yet parse individual tyre temps/wear from
// CarTelemetryData; for now we show placeholder data from the
// available fields. The layout is ready for when specific tyre
// data is added to the parser.
// ═══════════════════════════════════════════════════════════════════

static bool bgDrawn = false;
static int prevErs = -1;
static uint8_t prevBias = 0xFF;
static int8_t prevGear = 127;
static uint16_t prevSpeed = 0xFFFF;

void resetTyresDashboardCache() {
  bgDrawn = false;
  prevErs = -1; prevBias = 0xFF;
  prevGear = 127; prevSpeed = 0xFFFF;
}

static void drawTyreBox(TFT_eSPI* tft, int cx, int cy,
                        const char* label, int temp, int wear) {
  const int bw = 110, bh = 70;
  int x = cx - bw / 2, y = cy - bh / 2;

  uint16_t tempCol;
  if      (temp < 80)  tempCol = TFT_BLUE;
  else if (temp < 100) tempCol = TFT_GREEN;
  else if (temp < 110) tempCol = TFT_YELLOW;
  else                 tempCol = TFT_RED;

  tft->drawRect(x, y, bw, bh, 0x6B4D);
  tft->fillRect(x + 1, y + 1, bw - 2, 6, tempCol);
  tft->fillRect(x + 1, y + 7, bw - 2, bh - 8, TFT_BLACK);

  tft->setTextFont(4); tft->setTextSize(1);
  tft->setTextDatum(MC_DATUM);
  tft->setTextColor(TFT_WHITE, TFT_BLACK);
  tft->drawString(label, cx, cy - 10);

  char tbuf[10];
  snprintf(tbuf, sizeof(tbuf), "%dC", temp);
  tft->setTextFont(2); tft->setTextSize(1);
  tft->setTextColor(tempCol, TFT_BLACK);
  tft->drawString(tbuf, cx, cy + 10);

  int barY = y + bh - 12;
  int barW = bw - 8;
  tft->fillRect(x + 4, barY, barW, 8, 0x2104);
  int fillW = (int)((long)wear * barW / 100);
  uint16_t wearCol = (wear > 60) ? TFT_GREEN : (wear > 30) ? TFT_YELLOW : TFT_RED;
  if (fillW > 0) tft->fillRect(x + 4, barY, fillW, 8, wearCol);
}

void drawTyresDashboard(TFT_eSPI* tft, const TelemetryFrame &frame) {
  if (!bgDrawn) {
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_GREEN, TFT_BLACK);
    tft->drawString("TYRES", 20, 4);
    tft->drawFastHLine(20, 28, 440, 0x4208);

    // Car body outline (center area between tyres)
    tft->drawRect(170, 50, 140, 220, 0x4208);

    // Labels inside car outline
    tft->setTextFont(2); tft->setTextSize(1);
    tft->setTextColor(0x6B4D, TFT_BLACK);
    tft->setTextDatum(MC_DATUM);
    tft->drawString("GEAR", 210, 72);
    tft->drawString("SPEED", 270, 72);

    bgDrawn = true;
  }

  // ── Gear + Speed inside car outline (central) ──
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
    tft->fillRect(180, 80, 60, 50, TFT_BLACK);
    tft->drawString(gtxt, 210, 105);
  }

  uint16_t speed = frame.telemetry.speedKmh;
  if (speed != prevSpeed) {
    char sbuf[8];
    snprintf(sbuf, sizeof(sbuf), "%u", (unsigned)speed);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->setTextFont(7); tft->setTextSize(1);
    tft->setTextDatum(MC_DATUM);
    tft->fillRect(240, 80, 65, 50, TFT_BLACK);
    tft->drawString(sbuf, 270, 105);
    tft->setTextFont(2); tft->setTextSize(1);
    tft->setTextColor(0x6B4D, TFT_BLACK);
    tft->drawString("km/h", 240, 135);
  }

  // Placeholder temperatures & wear
  int temp = map((int)frame.status.ersEnergy, 0, 1000, 60, 120);
  int wear = min(100, (int)frame.status.brakeBias);

  int ers = (int)frame.status.ersEnergy;
  uint8_t bias = frame.status.brakeBias;
  if (ers != prevErs || bias != prevBias) {
    drawTyreBox(tft,  80,  95, "FL", temp, wear);
    drawTyreBox(tft, 400,  95, "FR", temp, wear);
    drawTyreBox(tft,  80, 225, "RL", temp, wear);
    drawTyreBox(tft, 400, 225, "RR", temp, wear);
  }

  prevErs = ers; prevBias = bias;
  prevGear = gear; prevSpeed = speed;
}
