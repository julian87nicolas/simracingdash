// Minimal Arduino stub for PlatformIO native tests
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>

using uint8_t = std::uint8_t;
using uint16_t = std::uint16_t;
using uint32_t = std::uint32_t;

inline void delay(unsigned long) {}
inline unsigned long millis() { static unsigned long t = 0; return t++; }

inline int min(int a, int b) { return (a < b) ? a : b; }
inline int max(int a, int b) { return (a > b) ? a : b; }
inline int map(int x, int in_min, int in_max, int out_min, int out_max) {
  if (in_max == in_min) return out_min;
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

inline void pinMode(int, int) {}

struct SerialClass {
  void begin(int) {}
  void println(const char* s) { puts(s); }
  void print(const char* s) { fputs(s, stdout); }
  void printf(const char* fmt, ...) { va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap); }
};

extern SerialClass Serial;
