// Framebuffer-based TFT stub for emulator: draws to an ASCII canvas
// This class now supports colors and basic primitives.
// It redraws the terminal in-place to avoid scrolling.
#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <iostream>
#include <cstdarg>
#include <cstring>
#include <mutex>

enum textdatum_t { MC_DATUM };

class TFT_eSPI {
public:
  // terminal canvas size (characters)
  static constexpr int COLS = 64;
  static constexpr int ROWS = 32;
  // logical display resolution (pixels) used by dashboards
  static constexpr int DISP_W = 480;
  static constexpr int DISP_H = 320;

  TFT_eSPI() { init(); }
  void init() { std::lock_guard<std::mutex> g(mu); clearCanvas(); }
  void setRotation(int) {}

  void fillScreen(uint32_t color) {
    std::lock_guard<std::mutex> g(mu);
    bgColor = color;
    for (int r=0;r<ROWS;r++) for (int c=0;c<COLS;c++) {
      canvas[r][c] = ' ';
      cellBg[r][c] = bgColor;
      cellFg[r][c] = 0xFFFF;
    }
  }

  void setTextFont(int) {}
  void setTextSize(int) {}
  void setTextColor(uint32_t fg, uint32_t bg=0) { curFg = fg; curBg = bg; }
  void setCursor(int x, int y) { cursorX = x; cursorY = y; }

  void drawString(const char* s, int x, int y) {
    std::lock_guard<std::mutex> g(mu);
    int cx = pixelToCol(x);
    int ry = pixelToRow(y);
    for (size_t i=0; s[i]; ++i) {
      int cpos = cx + (int)i;
      if (cpos >= 0 && cpos < COLS && ry >= 0 && ry < ROWS) {
        canvas[ry][cpos] = s[i];
        cellFg[ry][cpos] = curFg;
        if (curBg) cellBg[ry][cpos] = curBg; // if bg provided
      }
    }
  }

  void printf(const char* fmt, ...) {
    char buf[256];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
    drawString(buf, cursorX, cursorY);
  }

  void fillRect(int x, int y, int w, int h, uint32_t color) {
    std::lock_guard<std::mutex> g(mu);
    int x0 = pixelToCol(x);
    int y0 = pixelToRow(y);
    int x1 = pixelToCol(x+w-1);
    int y1 = pixelToRow(y+h-1);
    for (int r=y0;r<=y1;r++) for (int c=x0;c<=x1;c++) {
      if (r>=0 && r<ROWS && c>=0 && c<COLS) {
        canvas[r][c] = ' ';
        cellBg[r][c] = color;
        cellFg[r][c] = 0xFFFF;
      }
    }
  }

  void drawRect(int x, int y, int w, int h, uint32_t color) {
    std::lock_guard<std::mutex> g(mu);
    int x0 = pixelToCol(x);
    int y0 = pixelToRow(y);
    int x1 = pixelToCol(x+w-1);
    int y1 = pixelToRow(y+h-1);
    for (int c=x0;c<=x1;c++) {
      if (y0>=0 && y0<ROWS && c>=0 && c<COLS) { canvas[y0][c] = '-'; cellFg[y0][c]=color; }
      if (y1>=0 && y1<ROWS && c>=0 && c<COLS) { canvas[y1][c] = '-'; cellFg[y1][c]=color; }
    }
    for (int r=y0;r<=y1;r++) {
      if (x0>=0 && x0<COLS && r>=0 && r<ROWS) { canvas[r][x0] = '|'; cellFg[r][x0]=color; }
      if (x1>=0 && x1<COLS && r>=0 && r<ROWS) { canvas[r][x1] = '|'; cellFg[r][x1]=color; }
    }
  }

  void setTextDatum(textdatum_t) {}

  // Flush canvas to terminal in-place (avoids scrolling)
  void flush() {
    std::lock_guard<std::mutex> g(mu);
    // Move cursor to top-left and clear screen
    std::cout << "\x1b[H\x1b[2J";
    for (int r=0;r<ROWS;r++) {
      for (int c=0;c<COLS;c++) {
        uint32_t bg = cellBg[r][c];
        uint32_t fg = cellFg[r][c];
        int ansi = colorToAnsiBg(bg);
        int fgansi = colorToAnsiFg(fg);
        // set bg and fg
        std::cout << "\x1b["<<fgansi<<";"<<ansi<<"m" << canvas[r][c];
      }
      std::cout << "\x1b[0m\n"; // reset at end of line
    }
    std::cout << std::flush;
  }

private:
  uint32_t bgColor = 0x0000;
  char canvas[ROWS][COLS];
  uint32_t cellBg[ROWS][COLS];
  uint32_t cellFg[ROWS][COLS];
  int cursorX = 0, cursorY = 0; // pixel coords
  uint32_t curFg = 0xFFFF, curBg = 0;
  std::mutex mu;

  void clearCanvas() {
    for (int r=0;r<ROWS;r++) for (int c=0;c<COLS;c++) { canvas[r][c] = ' '; cellBg[r][c]=bgColor; cellFg[r][c]=0xFFFF; }
  }

  inline int pixelToCol(int x) const { return (x * COLS) / DISP_W; }
  inline int pixelToRow(int y) const { return (y * ROWS) / DISP_H; }

  static int colorToAnsiBg(uint32_t c) {
    // returns ANSI background code like 40..47
    if (c == TFT_BLACK) return 40;
    if (c == TFT_RED) return 41;
    if (c == TFT_GREEN) return 42;
    if (c == TFT_YELLOW || c == TFT_ORANGE) return 43;
    if (c == TFT_BLUE) return 44;
    if (c == TFT_WHITE) return 47;
    if (c == TFT_DARKGREY) return 100; // bright black
    return 40;
  }

  static int colorToAnsiFg(uint32_t c) {
    // returns ANSI code for foreground like 30..37
    if (c == TFT_BLACK) return 30;
    if (c == TFT_RED) return 31;
    if (c == TFT_GREEN) return 32;
    if (c == TFT_YELLOW || c == TFT_ORANGE) return 33;
    if (c == TFT_BLUE) return 34;
    if (c == TFT_WHITE) return 37;
    if (c == TFT_DARKGREY) return 90;
    return 37;
  }
};

