#include <TFT_eSPI.h>
#include "telemetry.h"
#include <Arduino.h>
#include <cstring>

// ═══════════════════════════════════════════════════════════════════
// MAIN RACE DASHBOARD — F1 steering wheel display style
// Layout (480×300 usable, bottom 20px = tab bar):
//   Row 0-14:   RPM LED bar (15 segments, green→yellow→red→purple)
//   Row 18-20:  DRS indicator (centered)
//   Row 30-180: Gear (huge, centered)
//               Left column: Throttle vertical bar + label
//               Right column: Brake vertical bar + label
//   Row 185-215: Speed (large, centered)
//   Row 218-252: Position (left), Lap time (center), Fuel (right)
//   Row 254-288: Brake bias (left), ERS mode (right)
//   Row 291-299: ERS battery bar (horizontal)
// ═══════════════════════════════════════════════════════════════════

static bool bgDrawn = false;
static uint16_t prevSpeed = 0xFFFF;
static int8_t prevGear = 127;
static uint16_t prevRpm = 0xFFFF;
static int prevTh = -1;
static int prevBr = -1;
static bool prevDrs = false;
static uint8_t prevPos = 0xFF;
static float prevLap = -1.0f;
static int prevFuel = -1;
static uint8_t prevBias = 0xFF;
static uint8_t prevErsMode = 0xFF;
static uint16_t prevErsEnergy = 0xFFFF;

void resetMainDashboardCache() {
  bgDrawn = false;
  prevSpeed = 0xFFFF; prevGear = 127; prevRpm = 0xFFFF;
  prevTh = -1; prevBr = -1; prevDrs = false;
  prevPos = 0xFF; prevLap = -1.0f; prevFuel = -1;
  prevBias = 0xFF; prevErsMode = 0xFF; prevErsEnergy = 0xFFFF;
}

// Helper: draw a vertical bar (bottom-up fill)
static void drawVBar(TFT_eSPI* tft, int x, int y, int w, int h,
                     int val, int maxVal, uint16_t fillCol) {
  int fillH = (int)((long)val * h / maxVal);
  if (fillH > h) fillH = h;
  // Empty part (top)
  if (fillH < h)
    tft->fillRect(x, y, w, h - fillH, 0x2104);
  // Filled part (bottom)
  if (fillH > 0)
    tft->fillRect(x, y + h - fillH, w, fillH, fillCol);
}

void drawMainDashboard(TFT_eSPI* tft, const TelemetryFrame &frame) {
  // ── Background: labels + outlines (drawn once) ──
  if (!bgDrawn) {
    // RPM label area
    tft->setTextFont(2); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(0x4208, TFT_BLACK); // dark grey labels

    // Throttle/Brake labels
    tft->setTextDatum(MC_DATUM);
    tft->drawString("THR", 30, 172);
    tft->drawString("BRK", 450, 172);

    // Vertical bar outlines
    tft->drawRect(14, 30, 32, 130, 0x4208);  // throttle outline
    tft->drawRect(434, 30, 32, 130, 0x4208); // brake outline

    // Bottom row labels
    tft->setTextFont(2); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(0x6B4D, TFT_BLACK);
    tft->drawString("POS", 80, 218);
    tft->drawString("LAP", 200, 218);
    tft->drawString("FUEL", 370, 218);
    tft->drawString("BIAS", 80, 256);
    tft->drawString("ERS", 330, 256);

    // ERS battery bar outline
    tft->drawRect(19, 290, 442, 10, 0x4208);

    bgDrawn = true;
  }

  uint16_t rpm = frame.telemetry.rpm;

  // ── RPM LED bar (15 segments) ──
  {
    const int segs = 15;
    const int segW = 440 / segs;
    const int segX = 20;
    const int segY = 4;
    const int segH = 14;
    for (int i = 0; i < segs; ++i) {
      uint16_t thresh = (uint16_t)((i + 1) * (15000.0f / segs));
      uint16_t col;
      if      (thresh <= 5000)  col = TFT_GREEN;
      else if (thresh <= 8000)  col = TFT_YELLOW;
      else if (thresh <= 11000) col = TFT_ORANGE;
      else if (thresh <= 13000) col = TFT_RED;
      else                      col = TFT_MAGENTA; // rev limit zone
      if (prevRpm == 0xFFFF || (rpm >= thresh) != (prevRpm >= thresh)) {
        tft->fillRect(segX + i * segW, segY, segW - 2, segH,
                      (rpm >= thresh) ? col : 0x2104);
      }
    }
  }

  // ── DRS indicator ──
  bool drs = frame.telemetry.drsActive;
  if (drs != prevDrs || !bgDrawn) {
    tft->setTextFont(2); tft->setTextSize(1);
    tft->setTextDatum(MC_DATUM);
    if (drs) {
      tft->fillRoundRect(205, 20, 70, 18, 4, TFT_GREEN);
      tft->setTextColor(TFT_BLACK, TFT_GREEN);
      tft->drawString("DRS", 240, 29);
    } else {
      tft->fillRect(205, 20, 70, 18, TFT_BLACK);
    }
  }

  // ── Gear (huge, centered) ──
  int8_t gear = frame.telemetry.gear;
  if (gear != prevGear) {
    char gtxt[4];
    if (gear == 0) strcpy(gtxt, "N");
    else if (gear == -1) strcpy(gtxt, "R");
    else snprintf(gtxt, sizeof(gtxt), "%d", gear);

    tft->setTextDatum(MC_DATUM);
    uint16_t gearCol = (gear == 0) ? TFT_GREEN :
                       (gear == -1) ? TFT_RED : TFT_WHITE;
    tft->setTextColor(gearCol, TFT_BLACK);
    tft->setTextFont(8); tft->setTextSize(1);
    tft->fillRect(150, 45, 180, 100, TFT_BLACK);
    tft->drawString(gtxt, 240, 95);
  }

  // ── Throttle vertical bar (left side) ──
  int th = frame.telemetry.throttle >> 2;
  if (th != prevTh) {
    drawVBar(tft, 15, 31, 30, 128, th, 63, TFT_GREEN);
  }

  // ── Brake vertical bar (right side) ──
  int br = frame.telemetry.brake >> 2;
  if (br != prevBr) {
    drawVBar(tft, 435, 31, 30, 128, br, 63, TFT_RED);
  }

  // ── Speed ──
  uint16_t speed = frame.telemetry.speedKmh;
  if (speed != prevSpeed) {
    char sbuf[16];
    snprintf(sbuf, sizeof(sbuf), "%3u", (unsigned)speed);
    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->setTextFont(7); tft->setTextSize(1);
    tft->fillRect(130, 155, 220, 55, TFT_BLACK);
    tft->drawString(sbuf, 228, 182);
    // km/h label
    tft->setTextFont(2); tft->setTextSize(1);
    tft->setTextColor(0x6B4D, TFT_BLACK);
    tft->drawString("km/h", 350, 182);
  }

  // ── Position ──
  uint8_t pos = frame.lap.position;
  if (pos != prevPos) {
    char pbuf[8];
    snprintf(pbuf, sizeof(pbuf), "P%u", (unsigned)pos);
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_YELLOW, TFT_BLACK);
    tft->fillRect(80, 230, 80, 25, TFT_BLACK);
    tft->drawString(pbuf, 80, 230);
  }

  // ── Last lap time ──
  float lapT = frame.lap.lastLapTime;
  if (lapT != prevLap) {
    int mins = (int)(lapT / 60.0f);
    float secs = lapT - mins * 60.0f;
    char lbuf[16];
    if (lapT > 0.1f)
      snprintf(lbuf, sizeof(lbuf), "%d:%05.2f", mins, (double)secs);
    else
      strcpy(lbuf, "--:--.--");
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->fillRect(200, 230, 140, 25, TFT_BLACK);
    tft->drawString(lbuf, 200, 230);
  }

  // ── Fuel ──
  int fuel = (int)(frame.status.fuel * 10); // 1 decimal via int
  if (fuel != prevFuel) {
    char fbuf[12];
    snprintf(fbuf, sizeof(fbuf), "%.1fL", (double)frame.status.fuel);
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_CYAN, TFT_BLACK);
    tft->fillRect(370, 230, 100, 25, TFT_BLACK);
    tft->drawString(fbuf, 370, 230);
  }

  // ── Brake bias ──
  uint8_t bias = frame.status.brakeBias;
  if (bias != prevBias) {
    char bbuf[12];
    snprintf(bbuf, sizeof(bbuf), "%u%%", (unsigned)bias);
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->fillRect(80, 268, 80, 22, TFT_BLACK);
    tft->drawString(bbuf, 80, 268);
  }

  // ── ERS deploy mode ──
  uint8_t ersM = frame.status.ersDeployMode;
  if (ersM != prevErsMode) {
    const char* modes[] = {"NONE", "MED", "HOT", "OVT"};
    const uint16_t mcols[] = {TFT_DARKGREY, TFT_CYAN, TFT_YELLOW, TFT_RED};
    uint8_t mi = (ersM <= 3) ? ersM : 0;
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(mcols[mi], TFT_BLACK);
    tft->fillRect(330, 268, 110, 22, TFT_BLACK);
    tft->drawString(modes[mi], 330, 268);
  }

  // ── ERS battery bar (0-1000 scaled) ──
  uint16_t ersE = frame.status.ersEnergy;
  if (ersE != prevErsEnergy) {
    const int barX = 20, barY = 291, barW = 440, barH = 8;
    tft->fillRect(barX, barY, barW, barH, 0x2104); // empty bg
    int fill = (int)((long)ersE * barW / 1000);
    if (fill > barW) fill = barW;
    if (fill > 0) {
      uint16_t col;
      if (ersE < 100)      col = TFT_RED;
      else if (ersE <= 200) col = TFT_ORANGE;
      else                  col = TFT_YELLOW;
      tft->fillRect(barX, barY, fill, barH, col);
    }
    prevErsEnergy = ersE;
  }

  prevRpm = rpm; prevSpeed = speed; prevGear = gear;
  prevTh = th; prevBr = br; prevDrs = drs;
  prevPos = pos; prevLap = lapT; prevFuel = fuel;
  prevBias = bias; prevErsMode = ersM;
}
