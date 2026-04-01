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
// Show an info screen with current target IP/port.
void dashboard_showNetworkInfo(const char* ip, uint16_t port, bool waitingData);
// Hide info screen and return to dashboards.
void dashboard_hideNetworkInfo();
// Update logic and render appropriate parts (non-blocking)
void dashboard_update(const StateManager &state);
