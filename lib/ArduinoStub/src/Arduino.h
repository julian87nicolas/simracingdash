// Minimal Arduino stub for PlatformIO native tests
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdarg>

using uint8_t = std::uint8_t;
using uint16_t = std::uint16_t;
using uint32_t = std::uint32_t;

inline void delay(unsigned long) {}

struct SerialClass {
  void begin(int) {}
  void println(const char* s) { puts(s); }
  void print(const char* s) { fputs(s, stdout); }
  void printf(const char* fmt, ...) { va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap); }
};

extern SerialClass Serial;
