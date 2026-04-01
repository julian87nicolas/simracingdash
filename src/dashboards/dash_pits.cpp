#include <TFT_eSPI.h>
#include "telemetry.h"
#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════
// PIT STOP DASHBOARD — MFD 1
// Pit status, current tyre compound + age, fuel + laps remaining
// ═══════════════════════════════════════════════════════════════════

static bool bgDrawn = false;
static int8_t prevGear = 127;
static uint16_t prevSpeed = 0xFFFF;
static uint8_t prevPitSt = 0xFF;
static uint8_t prevCompound = 0xFF;
static uint8_t prevAge = 0xFF;
static int prevFuel10 = -1;
static int prevFuelLaps10 = -1;
static uint8_t prevPos = 0xFF;

void resetPitsDashboardCache() {
  bgDrawn = false;
  prevGear = 127; prevSpeed = 0xFFFF;
  prevPitSt = 0xFF; prevCompound = 0xFF; prevAge = 0xFF;
  prevFuel10 = -1; prevFuelLaps10 = -1; prevPos = 0xFF;
}

static void getCompoundInfo(uint8_t vc, const char* &name, uint16_t &col) {
  switch (vc) {
    case 16: name = "SOFT";  col = TFT_RED;    break;
    case 17: name = "MED";   col = TFT_YELLOW; break;
    case 18: name = "HARD";  col = TFT_WHITE;  break;
    case 7:  name = "INTER"; col = TFT_GREEN;  break;
    case 8:  name = "WET";   col = TFT_BLUE;   break;
    default: name = "?";     col = TFT_DARKGREY; break;
  }
}

void drawPitsDashboard(TFT_eSPI* tft, const TelemetryFrame &frame) {
  if (!bgDrawn) {
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_MAGENTA, TFT_BLACK);
    tft->drawString("PIT STOP", 20, 4);
    tft->drawFastHLine(20, 28, 440, 0x4208);

    tft->setTextFont(2); tft->setTextSize(1);
    tft->setTextColor(0x6B4D, TFT_BLACK);
    tft->drawString("GEAR", 40, 32);
    tft->drawString("SPEED", 340, 32);
    tft->drawFastHLine(20, 82, 440, 0x4208);

    tft->drawString("PIT STATUS", 20, 88);
    tft->drawString("POSITION", 300, 88);
    tft->drawString("TYRE COMPOUND", 20, 158);
    tft->drawString("TYRE AGE", 300, 158);
    tft->drawString("FUEL", 20, 228);
    tft->drawString("FUEL LAPS", 300, 228);
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

  // ── Pit status badge ──
  uint8_t pitSt = frame.lap.pitStatus;
  if (pitSt != prevPitSt) {
    const char* labels[] = {"ON TRACK", "PITTING", "IN PIT"};
    const uint16_t cols[] = {TFT_GREEN, TFT_YELLOW, TFT_RED};
    uint8_t si = (pitSt <= 2) ? pitSt : 0;
    tft->fillRect(20, 104, 260, 36, TFT_BLACK);
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
    tft->fillRect(300, 104, 160, 40, TFT_BLACK);
    tft->drawString(buf, 300, 106);
  }

  // ── Tyre compound badge ──
  uint8_t compound = frame.status.tyreCompound;
  if (compound != prevCompound) {
    const char* name; uint16_t col;
    getCompoundInfo(compound, name, col);
    tft->fillRect(20, 174, 260, 36, TFT_BLACK);
    tft->fillRoundRect(20, 174, 14 + strlen(name) * 14, 32, 6, col);
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(ML_DATUM);
    uint16_t tc = (compound == 18 || compound == 17) ? TFT_BLACK : TFT_WHITE;
    tft->setTextColor(tc, col);
    tft->drawString(name, 28, 190);
  }

  // ── Tyre age ──
  uint8_t age = frame.status.tyresAgeLaps;
  if (age != prevAge) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%u laps", (unsigned)age);
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->fillRect(300, 174, 160, 30, TFT_BLACK);
    tft->drawString(buf, 300, 178);
  }

  // ── Fuel ──
  int fuel10 = (int)(frame.status.fuel * 10);
  if (fuel10 != prevFuel10) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%.1fL", (double)frame.status.fuel);
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_CYAN, TFT_BLACK);
    tft->fillRect(20, 246, 260, 30, TFT_BLACK);
    tft->drawString(buf, 20, 248);
  }

  // ── Fuel laps remaining ──
  int fuelLaps10 = (int)(frame.status.fuelLaps * 10);
  if (fuelLaps10 != prevFuelLaps10) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%.1f", (double)frame.status.fuelLaps);
    uint16_t fc = (frame.status.fuelLaps < 2.0f) ? TFT_RED :
                  (frame.status.fuelLaps < 5.0f) ? TFT_YELLOW : TFT_GREEN;
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(fc, TFT_BLACK);
    tft->fillRect(300, 246, 160, 30, TFT_BLACK);
    tft->drawString(buf, 300, 248);
  }

  prevGear = gear; prevSpeed = speed; prevPitSt = pitSt;
  prevCompound = compound; prevAge = age;
  prevFuel10 = fuel10; prevFuelLaps10 = fuelLaps10; prevPos = pos;
}
