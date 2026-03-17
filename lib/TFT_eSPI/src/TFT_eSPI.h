// Minimal TFT_eSPI stub for PlatformIO native tests
#pragma once
#include <cstdint>
#include <cstdio>

// Color constants
#define TFT_BLACK    0x0000
#define TFT_WHITE    0xFFFF
#define TFT_RED      0xF800
#define TFT_GREEN    0x07E0
#define TFT_BLUE     0x001F
#define TFT_YELLOW   0xFFE0
#define TFT_ORANGE   0xFC00
#define TFT_DARKGREY 0x5294

enum textdatum_t { MC_DATUM };

class TFT_eSPI {
public:
  void init() {}
  void setRotation(int) {}
  void fillScreen(uint32_t) {}
  void setTextFont(int) {}
  void setTextSize(int) {}
  void setTextColor(uint32_t, uint32_t=0) {}
  void setCursor(int, int) {}
  void fillRect(int, int, int, int, uint32_t) {}
  void drawRect(int, int, int, int, uint32_t) {}
  void setTextDatum(textdatum_t) {}
  void drawString(const char* s, int, int) { (void)s; }
  void printf(const char* , ...) {}
};
