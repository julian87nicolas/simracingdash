#include <TFT_eSPI.h>
#include "telemetry.h"
#include <Arduino.h>

// Draw main dashboard: gear, speed, RPM bar, throttle & brake bars.
// Uses partial updates by redrawing small rectangles.

static uint16_t prevSpeed = 0;
static int8_t prevGear = 127;
static uint16_t prevRpm = 0;

void drawMainDashboard(TFT_eSPI* tft, const TelemetryFrame &frame) {
  // Speed
  uint16_t speed = frame.telemetry.speedKmh;
  if (speed != prevSpeed) {
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->setTextSize(4);
    tft->setCursor(10, 20);
    tft->printf("%3u km/h  ", speed);
    prevSpeed = speed;
  }

  // Gear large centered
  int8_t gear = frame.telemetry.gear;
  if (gear != prevGear) {
    tft->fillRect(220, 40, 120, 120, TFT_BLACK);
    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(TFT_YELLOW, TFT_BLACK);
    tft->setTextSize(8);
    char gtxt[8];
    if (gear == 0) strcpy(gtxt, "N");
    else if (gear == -1) strcpy(gtxt, "R");
    else snprintf(gtxt, sizeof(gtxt), "%d", gear);
    tft->drawString(gtxt, 280, 100);
    prevGear = gear;
  }

  // RPM Bar (horizontal LED-style)
  uint16_t rpm = frame.telemetry.rpm;
  if (rpm != prevRpm) {
    const int barX = 10, barY = 160, barW = 460, barH = 20;
    tft->fillRect(barX, barY, barW, barH, TFT_DARKGREY);
    // Map RPM (0-15000) to bar width
    uint16_t maxRpm = 15000;
    int fillW = map(min(rpm, maxRpm), 0, maxRpm, 0, barW);
    uint16_t color = (rpm > 12000) ? TFT_RED : (rpm > 8000) ? TFT_ORANGE : TFT_GREEN;
    tft->fillRect(barX, barY, fillW, barH, color);
    prevRpm = rpm;
  }

  // Throttle & Brake bars
  int th = frame.telemetry.throttle; // 0-255
  int br = frame.telemetry.brake;
  const int w = 220, h = 14;
  // throttle
  tft->fillRect(10, 190, w, h, TFT_DARKGREY);
  tft->fillRect(10, 190, map(th, 0, 255, 0, w), h, TFT_GREEN);
  // brake
  tft->fillRect(250, 190, w, h, TFT_DARKGREY);
  tft->fillRect(250, 190, map(br, 0, 255, 0, w), h, TFT_RED);

}
