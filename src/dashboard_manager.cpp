#include "dashboard_manager.h"
#include <TFT_eSPI.h>
#include <Arduino.h>
#include "telemetry_parser.h"
#include "telemetry.h"
#include "state.h"

// Forward declarations for dashboard draw functions
void drawMainDashboard(TFT_eSPI* tft, const TelemetryFrame &frame);
void drawTyresDashboard(TFT_eSPI* tft, const TelemetryFrame &frame);
void drawERSDashboard(TFT_eSPI* tft, const TelemetryFrame &frame);
void drawDamageDashboard(TFT_eSPI* tft, const TelemetryFrame &frame);

static TFT_eSPI* g_tft = nullptr;
static DashboardScreen currentScreen = DashboardScreen::MAIN;
static TelemetryFrame lastFrame{};

void dashboard_init(TFT_eSPI* tft) {
  g_tft = tft;
  g_tft->init();
  g_tft->setRotation(1);
  g_tft->fillScreen(TFT_BLACK);
  g_tft->setTextFont(2);
  g_tft->setTextSize(1);
}

static DashboardScreen selectScreenFromState(const StateManager &state) {
  const auto &f = state.current();
  // Priority: pits > damage (not implemented) > tyres (mfd) > ers > main
  if (f.lap.pitStatus != 0) return DashboardScreen::PITS;
  uint8_t idx = f.telemetry.mfdPanelIndex;
  switch (idx) {
    case 1: return DashboardScreen::TYRES;
    case 2: return DashboardScreen::ERS;
    case 3: return DashboardScreen::DAMAGE;
    default: return DashboardScreen::MAIN;
  }
}

void dashboard_update(const StateManager &state) {
  if (!g_tft) return;
  DashboardScreen screen = selectScreenFromState(state);
  if (screen != currentScreen) {
    // simple screen change: clear small areas
    g_tft->fillScreen(TFT_BLACK);
    currentScreen = screen;
  }

  // Call renderers (they should attempt partial updates internally)
  const TelemetryFrame &f = state.current();
  switch (currentScreen) {
    case DashboardScreen::MAIN: drawMainDashboard(g_tft, f); break;
    case DashboardScreen::TYRES: drawTyresDashboard(g_tft, f); break;
    case DashboardScreen::ERS: drawERSDashboard(g_tft, f); break;
    case DashboardScreen::DAMAGE: drawDamageDashboard(g_tft, f); break;
    case DashboardScreen::PITS: drawDamageDashboard(g_tft, f); break; // reuse damage view for pits
  }

  lastFrame = f;
}
