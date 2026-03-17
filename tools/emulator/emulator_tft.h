// Simple TFT stub for emulator: prints meaningful drawing actions to stdout
#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <iostream>

enum textdatum_t { MC_DATUM };

class TFT_eSPI {
public:
  void init() { std::cout << "[TFT] init\n"; }
  void setRotation(int r) { std::cout << "[TFT] setRotation(" << r << ")\n"; }
  void fillScreen(uint32_t c) { std::cout << "[TFT] fillScreen color=" << c << "\n"; }
  void setTextFont(int f) { (void)f; }
  void setTextSize(int s) { (void)s; }
  void setTextColor(uint32_t c, uint32_t bg=0) { (void)c; (void)bg; }
  void setCursor(int x, int y) { cursorX = x; cursorY = y; }
  void drawString(const char* s, int x, int y) { std::cout << "[TFT] drawString("<< x <<","<< y <<"): " << s <<"\n"; }
  void printf(const char* fmt, ...) {
    char buf[256];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
    std::cout << "[TFT] printf: " << buf << "\n";
  }
  void fillRect(int x, int y, int w, int h, uint32_t color) {
    std::cout << "[TFT] fillRect("<<x<<","<<y<<","<<w<<","<<h<<") color="<<color<<"\n";
  }
  void drawRect(int x, int y, int w, int h, uint32_t color) {
    std::cout << "[TFT] drawRect("<<x<<","<<y<<","<<w<<","<<h<<") color="<<color<<"\n";
  }
  void setTextDatum(textdatum_t d) { (void)d; }
private:
  int cursorX=0, cursorY=0;
};
