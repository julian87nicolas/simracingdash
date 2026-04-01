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
    tft->drawString("PIT LANE", 20, 8);
    tft->drawFastHLine(20, 35, 440, 0x4208);

    // Labels
    tft->setTextFont(2); tft->setTextSize(1);
    tft->setTextColor(0x6B4D, TFT_BLACK);
    tft->drawString("PIT STATUS", 20, 48);
    tft->drawString("POSITION", 260, 48);
    tft->drawString("SPEED", 20, 118);
    tft->drawString("GEAR", 260, 118);
    tft->drawString("LAST LAP", 20, 188);
    tft->drawString("FUEL", 260, 188);
    bgDrawn = true;
  }

  // Pit status badge
  uint8_t pitSt = frame.lap.pitStatus;
  if (pitSt != prevPitSt) {
    const char* labels[] = {"ON TRACK", "PITTING", "IN PIT"};
    const uint16_t cols[] = {TFT_GREEN, TFT_YELLOW, TFT_RED};
    uint8_t si = (pitSt <= 2) ? pitSt : 0;
    tft->fillRect(20, 65, 220, 36, TFT_BLACK);
    tft->fillRoundRect(20, 65, 12 + strlen(labels[si]) * 14, 32, 6, cols[si]);
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(ML_DATUM);
    tft->setTextColor(TFT_BLACK, cols[si]);
    tft->drawString(labels[si], 28, 81);
  }

  // Position
  uint8_t pos = frame.lap.position;
  if (pos != prevPos) {
    char buf[8];
    snprintf(buf, sizeof(buf), "P%u", (unsigned)pos);
    tft->setTextFont(6); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_YELLOW, TFT_BLACK);
    tft->fillRect(260, 65, 180, 45, TFT_BLACK);
    tft->drawString(buf, 260, 65);
  }

  // Speed
  uint16_t speed = frame.telemetry.speedKmh;
  if (speed != prevSpeed) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%u km/h", (unsigned)speed);
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->fillRect(20, 136, 220, 30, TFT_BLACK);
    tft->drawString(buf, 20, 138);
  }

  // Gear
  int8_t gear = frame.telemetry.gear;
  if (gear != prevGear) {
    char gtxt[4];
    if (gear == 0) strcpy(gtxt, "N");
    else if (gear == -1) strcpy(gtxt, "R");
    else snprintf(gtxt, sizeof(gtxt), "%d", gear);
    tft->setTextFont(6); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->fillRect(260, 130, 80, 45, TFT_BLACK);
    tft->drawString(gtxt, 260, 130);
  }

  // Last lap
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
    tft->fillRect(20, 206, 220, 30, TFT_BLACK);
    tft->drawString(buf, 20, 208);
  }

  // Fuel
  int fuel10 = (int)(frame.status.fuel * 10);
  if (fuel10 != prevFuel10) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%.1fL", (double)frame.status.fuel);
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_CYAN, TFT_BLACK);
    tft->fillRect(260, 206, 180, 30, TFT_BLACK);
    tft->drawString(buf, 260, 208);
  }

  prevPitSt = pitSt; prevPos = pos; prevLap = lapT;
  prevFuel10 = fuel10; prevGear = gear; prevSpeed = speed;
}
