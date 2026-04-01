#include <TFT_eSPI.h>
#include "telemetry.h"
#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════
// PITS DASHBOARD
// Shows pit status, position, last lap, fuel — useful info during
// pit stops and when approaching the pit lane.
// ═══════════════════════════════════════════════════════════════════

static bool bgDrawn = false;
static uint8_t prevPitSt = 0xFF;
static uint8_t prevPos = 0xFF;
static float prevLap = -1.0f;
static int prevFuel10 = -1;
static int8_t prevGear = 127;
static uint16_t prevSpeed = 0xFFFF;

void resetPitsDashboardCache() {
  bgDrawn = false;
  prevPitSt = 0xFF; prevPos = 0xFF; prevLap = -1.0f;
  prevFuel10 = -1; prevGear = 127; prevSpeed = 0xFFFF;
}

void drawPitsDashboard(TFT_eSPI* tft, const TelemetryFrame &frame) {
  if (!bgDrawn) {
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_MAGENTA, TFT_BLACK);
    tft->drawString("PIT LANE", 20, 4);
    tft->drawFastHLine(20, 28, 440, 0x4208);

    // Gear / Speed labels
    tft->setTextFont(2); tft->setTextSize(1);
    tft->setTextColor(0x6B4D, TFT_BLACK);
    tft->drawString("GEAR", 40, 32);
    tft->drawString("SPEED", 340, 32);

    tft->drawFastHLine(20, 82, 440, 0x4208);

    // Section labels
    tft->drawString("PIT STATUS", 20, 88);
    tft->drawString("POSITION", 260, 88);
    tft->drawString("LAST LAP", 20, 168);
    tft->drawString("FUEL", 260, 168);
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

  // ── Pit status badge ──
  uint8_t pitSt = frame.lap.pitStatus;
  if (pitSt != prevPitSt) {
    const char* labels[] = {"ON TRACK", "PITTING", "IN PIT"};
    const uint16_t cols[] = {TFT_GREEN, TFT_YELLOW, TFT_RED};
    uint8_t si = (pitSt <= 2) ? pitSt : 0;
    tft->fillRect(20, 104, 220, 36, TFT_BLACK);
    tft->fillRoundRect(20, 104, 12 + strlen(labels[si]) * 14, 32, 6, cols[si]);
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(ML_DATUM);
    tft->setTextColor(TFT_BLACK, cols[si]);
    tft->drawString(labels[si], 28, 120);
  }

  // ── Position ──
  uint8_t pos = frame.lap.position;
  if (pos != prevPos) {
    char buf[8];
    snprintf(buf, sizeof(buf), "P%u", (unsigned)pos);
    tft->setTextFont(6); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_YELLOW, TFT_BLACK);
    tft->fillRect(260, 104, 180, 45, TFT_BLACK);
    tft->drawString(buf, 260, 104);
  }

  // ── Last lap ──
  float lapT = frame.lap.lastLapTime;
  if (lapT != prevLap) {
    int mins = (int)(lapT / 60.0f);
    float secs = lapT - mins * 60.0f;
    char buf[16];
    if (lapT > 0.1f)
      snprintf(buf, sizeof(buf), "%d:%05.2f", mins, (double)secs);
    else
      strcpy(buf, "--:--.--");
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->fillRect(20, 186, 220, 30, TFT_BLACK);
    tft->drawString(buf, 20, 188);
  }

  // ── Fuel ──
  int fuel10 = (int)(frame.status.fuel * 10);
  if (fuel10 != prevFuel10) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%.1fL", (double)frame.status.fuel);
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_CYAN, TFT_BLACK);
    tft->fillRect(260, 186, 180, 30, TFT_BLACK);
    tft->drawString(buf, 260, 188);
  }

  prevPitSt = pitSt; prevPos = pos; prevLap = lapT;
  prevFuel10 = fuel10; prevGear = gear; prevSpeed = speed;
}
