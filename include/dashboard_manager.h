#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "state.h"

enum class DashboardScreen {
  NONE,     // Sentinel: no screen drawn yet (forces initial tab bar render)
  MAIN,     // Default race screen
  SETUP,    // MFD 0: brake bias, differential, ERS deploy
  PITS,     // MFD 1: pit stop config
  TYRES,    // MFD 2: tyre wear status
  TEMPS,    // MFD 3: temperatures
  ENGINE    // MFD 4: engine component wear
};

// Initialize dashboard manager with TFT instance
void dashboard_init(TFT_eSPI* tft);
// Show an info screen with current target IP/port.
void dashboard_showNetworkInfo(const char* ip, uint16_t port, bool waitingData);
// Hide info screen and return to dashboards.
void dashboard_hideNetworkInfo();
// Update logic and render appropriate parts (non-blocking)
void dashboard_update(const StateManager &state);
