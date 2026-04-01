// Minimal TFT_eSPI stub for PlatformIO native tests
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdarg>

enum textdatum_t { TL_DATUM, TC_DATUM, TR_DATUM, ML_DATUM, MC_DATUM, MR_DATUM, BL_DATUM, BC_DATUM, BR_DATUM };

#define TFT_BLACK    0x0000
#define TFT_WHITE    0xFFFF
#define TFT_RED      0xF800
#define TFT_GREEN    0x07E0
#define TFT_BLUE     0x001F
#define TFT_CYAN     0x07FF
#define TFT_MAGENTA  0xF81F
#define TFT_YELLOW   0xFFE0
#define TFT_ORANGE   0xFC00
#define TFT_DARKGREY 0x5294

class TFT_eSPI {
public:
  int call_count = 0;
  void init() { call_count++; }
  void setRotation(int) { call_count++; }
  void fillScreen(uint32_t) { call_count++; }
  void setTextFont(int) { call_count++; }
  void setTextSize(int) { call_count++; }
  void setTextColor(uint32_t, uint32_t=0) { call_count++; }
  void setCursor(int, int) { call_count++; }
  void fillRect(int, int, int, int, uint32_t) { call_count++; }
  void fillRoundRect(int, int, int, int, int, uint32_t) { call_count++; }
  void drawRect(int, int, int, int, uint32_t) { call_count++; }
  void drawRoundRect(int, int, int, int, int, uint32_t) { call_count++; }
  void drawFastHLine(int, int, int, uint32_t) { call_count++; }
  void drawFastVLine(int, int, int, uint32_t) { call_count++; }
  void drawLine(int, int, int, int, uint32_t) { call_count++; }
  void fillCircle(int, int, int, uint32_t) { call_count++; }
  void setTextDatum(textdatum_t) { call_count++; }
  void drawString(const char* s, int, int) { call_count++; }
  void printf(const char* fmt, ...) { call_count++; }
};
