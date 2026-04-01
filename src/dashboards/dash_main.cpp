#include <TFT_eSPI.h>
#include "telemetry.h"
#include <Arduino.h>
#include <cstring>

// Draw main dashboard: gear, speed, RPM bar, throttle & brake bars.
// Keeps a static background and updates value areas to reduce flicker.

static bool bgDrawn = false;
static uint16_t prevSpeed = 0xFFFF;
static int8_t prevGear = 127;
static uint16_t prevRpm = 0xFFFF;
static int prevTh = -1;
static int prevBr = -1;

void drawMainDashboard(TFT_eSPI* tft, const TelemetryFrame &frame) {
  if (!bgDrawn) {
    tft->fillScreen(TFT_BLACK);
    tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft->setTextFont(2);
    tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->drawString("GEAR", 210, 55);
    tft->drawString("SPEED", 205, 165);
    tft->drawString("THR", 70, 226);
    tft->drawString("BRK", 350, 226);
    bgDrawn = true;
  }

  // RPM LED arc (30 segments)
  uint16_t rpm = frame.telemetry.rpm;
  const int segs = 30;
  const int segW = 460 / segs;
  const int segX = 10;
  const int segY = 20;
  const int segH = 10;
  for (int i = 0; i < segs; ++i) {
    // threshold per segment
    uint16_t threshold = (uint16_t)((i+1) * (15000.0f / segs));
    uint32_t col = (threshold > 12000) ? TFT_RED : (threshold > 8000) ? TFT_ORANGE : TFT_GREEN;
    if (prevRpm == 0xFFFF || (rpm >= threshold) != (prevRpm >= threshold)) {
      tft->fillRect(segX + i*segW, segY, segW-1, segH, (rpm >= threshold) ? col : TFT_DARKGREY);
    }
  }

  // Gear in center
  int8_t gear = frame.telemetry.gear;
  tft->setTextDatum(MC_DATUM);
  tft->setTextColor(TFT_YELLOW, TFT_BLACK);
  char gtxt[8];
  if (gear == 0) strcpy(gtxt, "N");
  else if (gear == -1) strcpy(gtxt, "R");
  else snprintf(gtxt, sizeof(gtxt), "%d", gear);
  if (gear != prevGear) {
    tft->setTextFont(8);
    tft->setTextSize(1);
    tft->fillRect(130, 76, 220, 80, TFT_BLACK);
    tft->drawString(gtxt, 240, 116);
  }

  // Speed below center
  uint16_t speed = frame.telemetry.speedKmh;
  tft->setTextColor(TFT_WHITE, TFT_BLACK);
  char sbuf[32];
  snprintf(sbuf, sizeof(sbuf), "%u km/h", (unsigned int)speed);
  if (speed != prevSpeed) {
    tft->setTextFont(4);
    tft->setTextSize(1);
    tft->fillRect(100, 180, 300, 45, TFT_BLACK);
    tft->drawString(sbuf, 240, 202);
  }

  // Throttle & Brake small bars
  int th = frame.telemetry.throttle; // 0-255
  int br = frame.telemetry.brake;
  const int bw = 40, bh = 4;
  // left throttle
  if (th != prevTh) {
    tft->fillRect(100, 240, bw, bh, TFT_DARKGREY);
    tft->fillRect(100, 240, map(th, 0, 255, 0, bw), bh, TFT_GREEN);
  }
  // right brake
  if (br != prevBr) {
    tft->fillRect(340, 240, bw, bh, TFT_DARKGREY);
    tft->fillRect(340, 240, map(br, 0, 255, 0, bw), bh, TFT_RED);
  }

  prevRpm = rpm;
  prevSpeed = speed;
  prevGear = gear;
  prevTh = th;
  prevBr = br;
}
