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

void resetTyresDashboardCache() {
  bgDrawn = false;
  prevErs = -1; prevBias = 0xFF;
}

static void drawTyreBox(TFT_eSPI* tft, int cx, int cy,
                        const char* label, int temp, int wear) {
  const int bw = 120, bh = 80;
  int x = cx - bw / 2, y = cy - bh / 2;

  // Temp → color: <80 blue, 80-100 green, 100-110 yellow, >110 red
  uint16_t tempCol;
  if      (temp < 80)  tempCol = TFT_BLUE;
  else if (temp < 100) tempCol = TFT_GREEN;
  else if (temp < 110) tempCol = TFT_YELLOW;
  else                 tempCol = TFT_RED;

  // Box with colored top strip
  tft->drawRect(x, y, bw, bh, 0x6B4D);
  tft->fillRect(x + 1, y + 1, bw - 2, 6, tempCol); // temp color strip
  tft->fillRect(x + 1, y + 7, bw - 2, bh - 8, TFT_BLACK); // body

  // Label (FL, FR, etc.)
  tft->setTextFont(4); tft->setTextSize(1);
  tft->setTextDatum(MC_DATUM);
  tft->setTextColor(TFT_WHITE, TFT_BLACK);
  tft->drawString(label, cx, cy - 12);

  // Temperature value
  char tbuf[10];
  snprintf(tbuf, sizeof(tbuf), "%dC", temp);
  tft->setTextFont(2); tft->setTextSize(1);
  tft->setTextColor(tempCol, TFT_BLACK);
  tft->drawString(tbuf, cx, cy + 12);

  // Wear bar at bottom
  int barY = y + bh - 12;
  int barW = bw - 8;
  tft->fillRect(x + 4, barY, barW, 8, 0x2104);
  int fillW = (int)((long)wear * barW / 100);
  uint16_t wearCol = (wear > 60) ? TFT_GREEN : (wear > 30) ? TFT_YELLOW : TFT_RED;
  if (fillW > 0) tft->fillRect(x + 4, barY, fillW, 8, wearCol);
}

void drawTyresDashboard(TFT_eSPI* tft, const TelemetryFrame &frame) {
  if (!bgDrawn) {
    // Title
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_GREEN, TFT_BLACK);
    tft->drawString("TYRES", 20, 8);
    tft->drawFastHLine(20, 35, 440, 0x4208);

    // Car body outline (simple rectangle between tyres)
    tft->drawRect(170, 60, 140, 200, 0x4208);
    bgDrawn = true;
  }

  // Placeholder temperatures & wear (until specific tyre parse is added)
  int temp = map((int)frame.status.ersEnergy, 0, 1000, 60, 120);
  int wear = min(100, (int)frame.status.brakeBias);

  // Only redraw when source data changes
  int ers = (int)frame.status.ersEnergy;
  uint8_t bias = frame.status.brakeBias;
  if (ers != prevErs || bias != prevBias) {
    //       FL           FR
    drawTyreBox(tft,  90, 100, "FL", temp, wear);
    drawTyreBox(tft, 390, 100, "FR", temp, wear);
    //       RL           RR
    drawTyreBox(tft,  90, 220, "RL", temp, wear);
    drawTyreBox(tft, 390, 220, "RR", temp, wear);
  }

  prevErs = ers; prevBias = bias;
}
