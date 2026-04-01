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
static bool g_showNetworkInfo = false;

void dashboard_init(TFT_eSPI* tft) {
  g_tft = tft;
  g_tft->init();
  g_tft->setRotation(1);
  // Quick panel test pattern to verify command/data path on boot.
  g_tft->fillScreen(TFT_RED);
  delay(180);
  g_tft->fillScreen(TFT_GREEN);
  delay(180);
  g_tft->fillScreen(TFT_BLUE);
  delay(180);
  g_tft->fillScreen(TFT_BLACK);
  g_tft->setTextFont(2);
  g_tft->setTextSize(1);

  // Boot greeting screen
  g_tft->setTextColor(TFT_WHITE, TFT_BLACK);
  g_tft->setTextDatum(MC_DATUM);
  g_tft->drawString("SIMRACING DASH", 240, 130);
  g_tft->setTextColor(TFT_CYAN, TFT_BLACK);
  g_tft->drawString("Iniciando...", 240, 165);
  delay(1200);
  g_tft->setTextDatum(TL_DATUM);
  g_tft->fillScreen(TFT_BLACK);
}

static DashboardScreen selectScreenFromState(const StateManager &state) {
  const auto &f = state.current();
  // Keep MAIN readable as primary view while telemetry mapping is refined.
  if (f.lap.pitStatus != 0) return DashboardScreen::PITS;
  return DashboardScreen::MAIN;
}

void dashboard_showNetworkInfo(const char* ip, uint16_t port, bool waitingData) {
  if (!g_tft) return;
  g_showNetworkInfo = true;

  g_tft->fillScreen(TFT_BLACK);
  g_tft->setTextDatum(MC_DATUM);
  g_tft->setTextFont(4);

  g_tft->setTextColor(TFT_CYAN, TFT_BLACK);
  g_tft->drawString("SIMRACING DASH", 240, 30);

  g_tft->setTextColor(TFT_WHITE, TFT_BLACK);
  g_tft->drawString("Destino UDP del juego:", 240, 80);

  char addrBuf[48];
  snprintf(addrBuf, sizeof(addrBuf), "%s:%u", ip, (unsigned int)port);
  g_tft->setTextColor(TFT_YELLOW, TFT_BLACK);
  g_tft->drawString(addrBuf, 240, 130);

  g_tft->setTextColor(TFT_WHITE, TFT_BLACK);
  g_tft->setTextFont(2);
  if (waitingData) {
    g_tft->drawString("Esperando telemetria...", 240, 190);
    g_tft->drawString("Envia datos a esta IP:PORT desde el juego", 240, 220);
  } else {
    g_tft->drawString("Sin datos hace 10s", 240, 190);
    g_tft->drawString("Reanudando al recibir telemetria", 240, 220);
  }
}

void dashboard_hideNetworkInfo() {
  if (!g_tft) return;
  if (!g_showNetworkInfo) return;
  g_showNetworkInfo = false;
  g_tft->fillScreen(TFT_BLACK);
}

void dashboard_update(const StateManager &state) {
  if (!g_tft) return;
  if (g_showNetworkInfo) return;
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
