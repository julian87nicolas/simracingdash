#include <TFT_eSPI.h>
#include "telemetry.h"
#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════
// TEMPERATURES DASHBOARD — MFD 3
// Shows tyre surface/inner temps (4 corners), brake temps, engine temp
// ═══════════════════════════════════════════════════════════════════

static bool bgDrawn = false;
static int8_t prevGear = 127;
static uint16_t prevSpeed = 0xFFFF;
static uint8_t prevSurf[4] = {0xFF,0xFF,0xFF,0xFF};
static uint8_t prevInner[4] = {0xFF,0xFF,0xFF,0xFF};
static uint16_t prevBrake[4] = {0xFFFF,0xFFFF,0xFFFF,0xFFFF};
static uint16_t prevEngine = 0xFFFF;

void resetTempsDashboardCache() {
  bgDrawn = false;
  prevGear = 127; prevSpeed = 0xFFFF;
  memset(prevSurf, 0xFF, sizeof(prevSurf)); memset(prevInner, 0xFF, sizeof(prevInner));
  memset(prevBrake, 0xFF, sizeof(prevBrake)); prevEngine = 0xFFFF;
}

// Color from temperature value
static uint16_t tempColor(int t, int cold, int optimal, int hot) {
  if (t < cold)    return TFT_BLUE;
  if (t < optimal) return TFT_GREEN;
  if (t < hot)     return TFT_YELLOW;
  return TFT_RED;
}

// Draw a compact temp box: label, surface, inner, brake
static void drawCorner(TFT_eSPI* tft, int x, int y,
                       const char* label, uint8_t surf, uint8_t inner,
                       uint16_t brake) {
  const int bw = 100, bh = 90;

  tft->drawRect(x, y, bw, bh, 0x6B4D);
  tft->fillRect(x + 1, y + 1, bw - 2, bh - 2, TFT_BLACK);

  // Corner label
  tft->setTextFont(2); tft->setTextSize(1);
  tft->setTextDatum(MC_DATUM);
  tft->setTextColor(TFT_WHITE, TFT_BLACK);
  tft->drawString(label, x + bw / 2, y + 10);

  // Surface temp
  char buf[8];
  uint16_t sc = tempColor(surf, 70, 90, 110);
  snprintf(buf, sizeof(buf), "S:%u", (unsigned)surf);
  tft->setTextFont(4); tft->setTextSize(1);
  tft->setTextColor(sc, TFT_BLACK);
  tft->drawString(buf, x + bw / 2, y + 30);

  // Inner temp
  uint16_t ic = tempColor(inner, 80, 100, 115);
  snprintf(buf, sizeof(buf), "I:%u", (unsigned)inner);
  tft->setTextColor(ic, TFT_BLACK);
  tft->drawString(buf, x + bw / 2, y + 52);

  // Brake temp
  uint16_t bc = tempColor(brake, 200, 600, 900);
  snprintf(buf, sizeof(buf), "%uC", (unsigned)brake);
  tft->setTextFont(2); tft->setTextSize(1);
  tft->setTextColor(bc, TFT_BLACK);
  tft->drawString(buf, x + bw / 2, y + 76);
}

void drawTempsDashboard(TFT_eSPI* tft, const TelemetryFrame &frame) {
  if (!bgDrawn) {
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_YELLOW, TFT_BLACK);
    tft->drawString("TEMPERATURES", 20, 4);
    tft->drawFastHLine(20, 28, 440, 0x4208);

    // Gear / Speed labels
    tft->setTextFont(2); tft->setTextSize(1);
    tft->setTextColor(0x6B4D, TFT_BLACK);
    tft->drawString("GEAR", 40, 32);
    tft->drawString("SPEED", 340, 32);
    tft->drawFastHLine(20, 82, 440, 0x4208);

    // Legend at center
    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(0x6B4D, TFT_BLACK);
    tft->drawString("S=surface I=inner", 240, 130);
    tft->drawString("bottom=brake", 240, 148);
    tft->drawString("ENGINE", 240, 210);

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

  // ── 4 corner temp boxes (FL, FR, RL, RR) ──
  // FL=0, FR=1, RL=2, RR=3
  const int cx[] = {20, 360, 20, 360};
  const int cy[] = {88, 88, 195, 195};
  const char* labels[] = {"FL", "FR", "RL", "RR"};

  for (int i = 0; i < 4; ++i) {
    uint8_t s = frame.telemetry.tyresSurfaceTemp[i];
    uint8_t n = frame.telemetry.tyresInnerTemp[i];
    uint16_t b = frame.telemetry.brakesTemp[i];
    if (s != prevSurf[i] || n != prevInner[i] || b != prevBrake[i]) {
      drawCorner(tft, cx[i], cy[i], labels[i], s, n, b);
      prevSurf[i] = s; prevInner[i] = n; prevBrake[i] = b;
    }
  }

  // ── Engine temp (center bottom) ──
  uint16_t eng = frame.telemetry.engineTemp;
  if (eng != prevEngine) {
    uint16_t ec = tempColor(eng, 80, 110, 130);
    char buf[12];
    snprintf(buf, sizeof(buf), "%uC", (unsigned)eng);
    tft->setTextFont(4); tft->setTextSize(1);
    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(ec, TFT_BLACK);
    tft->fillRect(170, 225, 140, 30, TFT_BLACK);
    tft->drawString(buf, 240, 240);
  }

  prevGear = gear; prevSpeed = speed; prevEngine = eng;
}
