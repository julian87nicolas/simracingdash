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
void drawPitsDashboard(TFT_eSPI* tft, const TelemetryFrame &frame);
void resetMainDashboardCache();
void resetTyresDashboardCache();
void resetERSDashboardCache();
void resetDamageDashboardCache();
void resetPitsDashboardCache();

static TFT_eSPI* g_tft = nullptr;
static DashboardScreen currentScreen = DashboardScreen::MAIN;
static bool g_showNetworkInfo = false;

// Navigation state ─ latch last valid MFD selection so closing MFD keeps screen
static DashboardScreen g_lastMfdScreen = DashboardScreen::MAIN;
static bool g_everReceivedMfd = false;
static uint8_t g_rawMfd = 255;
static uint8_t g_stableMfd = 255;
static uint32_t g_mfdChangeMs = 0;
static const uint32_t MFD_DEBOUNCE_MS = 200;

static DashboardScreen mapMfdToScreen(uint8_t mfd) {
  switch (mfd) {
    case 0: return DashboardScreen::MAIN;    // car setup → main dash
    case 1: return DashboardScreen::PITS;    // pits
    case 2: return DashboardScreen::DAMAGE;  // damage
    case 3: return DashboardScreen::ERS;     // engine/ERS
    case 4: return DashboardScreen::TYRES;   // temperatures
    default: return DashboardScreen::MAIN;
  }
}

static void resetAllDashboardCaches() {
  resetMainDashboardCache();
  resetTyresDashboardCache();
  resetERSDashboardCache();
  resetDamageDashboardCache();
  resetPitsDashboardCache();
}

// Screen indicator labels drawn at the bottom of every dashboard
static const char* screenLabel(DashboardScreen s) {
  switch (s) {
    case DashboardScreen::MAIN:   return "RACE";
    case DashboardScreen::TYRES:  return "TYRES";
    case DashboardScreen::ERS:    return "ERS";
    case DashboardScreen::DAMAGE: return "DMG";
    case DashboardScreen::PITS:   return "PITS";
  }
  return "?";
}

static void drawScreenTab(TFT_eSPI* tft, DashboardScreen active) {
  const DashboardScreen tabs[] = {
    DashboardScreen::MAIN, DashboardScreen::ERS,
    DashboardScreen::TYRES, DashboardScreen::DAMAGE, DashboardScreen::PITS
  };
  const int N = 5;
  const int tabW = 480 / N;
  const int tabY = 300;
  const int tabH = 20;
  for (int i = 0; i < N; ++i) {
    bool sel = (tabs[i] == active);
    uint16_t bg = sel ? TFT_WHITE : 0x2104; // dark grey
    uint16_t fg = sel ? TFT_BLACK : TFT_DARKGREY;
    tft->fillRect(i * tabW, tabY, tabW, tabH, bg);
    tft->setTextDatum(MC_DATUM);
    tft->setTextFont(2);
    tft->setTextSize(1);
    tft->setTextColor(fg, bg);
    tft->drawString(screenLabel(tabs[i]), i * tabW + tabW / 2, tabY + tabH / 2);
  }
}

void dashboard_init(TFT_eSPI* tft) {
  g_tft = tft;
  g_tft->init();
  g_tft->setRotation(1);
  g_tft->fillScreen(TFT_BLACK);
  g_tft->setTextFont(2);
  g_tft->setTextSize(1);

  g_tft->setTextColor(TFT_WHITE, TFT_BLACK);
  g_tft->setTextDatum(MC_DATUM);
  g_tft->drawString("SIMRACING DASH", 240, 140);
  g_tft->setTextColor(TFT_CYAN, TFT_BLACK);
  g_tft->drawString("Iniciando...", 240, 170);
  delay(800);
  g_tft->fillScreen(TFT_BLACK);
}

static DashboardScreen selectScreenFromState(const StateManager &state) {
  const auto &f = state.current();
  uint8_t mfd = f.telemetry.mfdPanelIndex;
  uint32_t now = millis();

  // Debounce: track raw → stable transition
  if (mfd != g_rawMfd) {
    g_rawMfd = mfd;
    g_mfdChangeMs = now;
  }
  if ((now - g_mfdChangeMs) >= MFD_DEBOUNCE_MS) {
    g_stableMfd = g_rawMfd;
  }

  // When MFD is open (0-4), latch that screen
  if (g_stableMfd <= 4) {
    g_lastMfdScreen = mapMfdToScreen(g_stableMfd);
    g_everReceivedMfd = true;
  }
  // When MFD is closed (255) → keep last latched screen
  // This prevents jumping back to MAIN every time MFD closes.

  return g_lastMfdScreen;
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
  resetAllDashboardCaches();
}

void dashboard_update(const StateManager &state) {
  if (!g_tft) return;
  if (g_showNetworkInfo) return;
  DashboardScreen screen = selectScreenFromState(state);
  if (screen != currentScreen) {
    resetAllDashboardCaches();
    g_tft->fillScreen(TFT_BLACK);
    drawScreenTab(g_tft, screen);
    currentScreen = screen;
  }

  const TelemetryFrame &f = state.current();
  switch (currentScreen) {
    case DashboardScreen::MAIN:   drawMainDashboard(g_tft, f);   break;
    case DashboardScreen::TYRES:  drawTyresDashboard(g_tft, f);  break;
    case DashboardScreen::ERS:    drawERSDashboard(g_tft, f);    break;
    case DashboardScreen::DAMAGE: drawDamageDashboard(g_tft, f); break;
    case DashboardScreen::PITS:   drawPitsDashboard(g_tft, f);   break;
  }
}
