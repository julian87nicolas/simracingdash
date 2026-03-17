#include <TFT_eSPI.h>
#include "telemetry.h"
#include <Arduino.h>
#include <cstring>

// Draw main dashboard: gear, speed, RPM bar, throttle & brake bars.
// Uses partial updates by redrawing small rectangles.

static uint16_t prevSpeed = 0;
static int8_t prevGear = 127;
static uint16_t prevRpm = 0;

void drawMainDashboard(TFT_eSPI* tft, const TelemetryFrame &frame) {
  // For emulator-friendly layout: clear and draw a wheel-like dashboard
  tft->fillScreen(TFT_BLACK);

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
    tft->fillRect(segX + i*segW, segY, segW-1, segH, (rpm >= threshold) ? col : TFT_DARKGREY);
  }

  // Gear in center
  int8_t gear = frame.telemetry.gear;
  tft->setTextDatum(MC_DATUM);
  tft->setTextColor(TFT_YELLOW, TFT_BLACK);
  char gtxt[8];
  if (gear == 0) strcpy(gtxt, "N");
  else if (gear == -1) strcpy(gtxt, "R");
  else snprintf(gtxt, sizeof(gtxt), "%d", gear);
  tft->drawString(gtxt, 240, 120);

  // Speed below center
  uint16_t speed = frame.telemetry.speedKmh;
  tft->setTextColor(TFT_WHITE, TFT_BLACK);
  char sbuf[32];
  snprintf(sbuf, sizeof(sbuf), "%u km/h", (unsigned int)speed);
  tft->drawString(sbuf, 200, 200);

  // Throttle & Brake small bars
  int th = frame.telemetry.throttle; // 0-255
  int br = frame.telemetry.brake;
  const int bw = 40, bh = 4;
  // left throttle
  tft->fillRect(100, 240, bw, bh, TFT_DARKGREY);
  tft->fillRect(100, 240, map(th, 0, 255, 0, bw), bh, TFT_GREEN);
  // right brake
  tft->fillRect(340, 240, bw, bh, TFT_DARKGREY);
  tft->fillRect(340, 240, map(br, 0, 255, 0, bw), bh, TFT_RED);

  prevRpm = rpm;
  prevSpeed = speed;
  prevGear = gear;
}
