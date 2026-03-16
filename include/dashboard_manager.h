#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "state.h"

enum class DashboardScreen {
  MAIN,
  TYRES,
  ERS,
  DAMAGE,
  PITS
};

// Initialize dashboard manager with TFT instance
void dashboard_init(TFT_eSPI* tft);
// Update logic and render appropriate parts (non-blocking)
void dashboard_update(const StateManager &state);
