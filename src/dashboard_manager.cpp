#include "dashboard_manager.h"
#include <TFT_eSPI.h>
#include <Arduino.h>
#include "qr_render.h"
#include "telemetry_parser.h"
#include "telemetry.h"
#include "state.h"

// Forward declarations for dashboard draw functions
void drawMainDashboard(TFT_eSPI* tft, const TelemetryFrame &frame);
void drawSetupDashboard(TFT_eSPI* tft, const TelemetryFrame &frame);
void drawPitsDashboard(TFT_eSPI* tft, const TelemetryFrame &frame);
void drawTyresDashboard(TFT_eSPI* tft, const TelemetryFrame &frame);
void drawTempsDashboard(TFT_eSPI* tft, const TelemetryFrame &frame);
void drawEngineDashboard(TFT_eSPI* tft, const TelemetryFrame &frame);
void resetMainDashboardCache();
void resetSetupDashboardCache();
void resetPitsDashboardCache();
void resetTyresDashboardCache();
void resetTempsDashboardCache();
void resetEngineDashboardCache();

static TFT_eSPI* g_tft = nullptr;
static DashboardScreen currentScreen = DashboardScreen::NONE;  // NONE forces tab draw on first update
static bool g_showNetworkInfo = false;

static uint8_t g_rawMfd = 255;
static uint8_t g_stableMfd = 255;
static uint32_t g_mfdChangeMs = 0;
static const uint32_t MFD_DEBOUNCE_MS = 200;

static uint8_t g_sessionType = 0;
static bool isRaceSession() { return g_sessionType >= 10 && g_sessionType <= 12; }

// MFD index → screen mapping — varies by session type!
// Race:     0=Setup, 1=Pits, 2=Tyres, 3=Temps, 4=Engine
// Non-race: 0=Setup, 1=Tyres, 2=Temps, 3=Engine  (no Pits panel)
static DashboardScreen mapMfdToScreen(uint8_t mfd) {
  if (isRaceSession()) {
    switch (mfd) {
      case 0: return DashboardScreen::SETUP;
      case 1: return DashboardScreen::PITS;
      case 2: return DashboardScreen::TYRES;
      case 3: return DashboardScreen::TEMPS;
      case 4: return DashboardScreen::ENGINE;
      default: return DashboardScreen::MAIN;
    }
  } else {
    // Practice, qualifying, time trial — no pit stop panel
    switch (mfd) {
      case 0: return DashboardScreen::SETUP;
      case 1: return DashboardScreen::TYRES;
      case 2: return DashboardScreen::TEMPS;
      case 3: return DashboardScreen::ENGINE;
      default: return DashboardScreen::MAIN;
    }
  }
}

static void resetAllDashboardCaches() {
  resetMainDashboardCache();
  resetSetupDashboardCache();
  resetPitsDashboardCache();
  resetTyresDashboardCache();
  resetTempsDashboardCache();
  resetEngineDashboardCache();
}

static const char* screenLabel(DashboardScreen s) {
  switch (s) {
    case DashboardScreen::MAIN:   return "RACE";
    case DashboardScreen::SETUP:  return "SETUP";
    case DashboardScreen::PITS:   return "PITS";
    case DashboardScreen::TYRES:  return "TYRES";
    case DashboardScreen::TEMPS:  return "TEMPS";
    case DashboardScreen::ENGINE: return "MOTOR";
  }
  return "?";
}

static void drawScreenTab(TFT_eSPI* tft, DashboardScreen active) {
  const DashboardScreen tabs[] = {
    DashboardScreen::MAIN,  DashboardScreen::SETUP,
    DashboardScreen::PITS,  DashboardScreen::TYRES,
    DashboardScreen::TEMPS, DashboardScreen::ENGINE
  };
  const int N = 6;
  const int tabW = 480 / N; // 80px each
  const int tabY = 300;
  const int tabH = 20;
  for (int i = 0; i < N; ++i) {
    bool sel = (tabs[i] == active);
    uint16_t bg = sel ? TFT_WHITE : 0x2104;
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
  g_sessionType = f.sessionType;  // keep session type updated
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

  // When MFD is open (0-4), show that screen
  if (g_stableMfd <= 4) {
    return mapMfdToScreen(g_stableMfd);
  }
  // When MFD is closed (255), return to main race screen
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

  // Show reset-wifi QR (right side) with label (left side)
  g_tft->drawLine(40, 240, 440, 240, TFT_DARKGREY);
  char resetUrl[64];
  snprintf(resetUrl, sizeof(resetUrl), "http://%s/reset-wifi", ip);
  // QR: version 3 = 29 modules, pixel size 2 => 58px + 8px border = 66px total
  int qrPixel = 2;
  int qrSize = 29 * qrPixel; // 58px
  int qrBorder = qrPixel * 2;
  int qrX = 380;
  int qrY = 252;
  drawQRCode(g_tft, resetUrl, qrX, qrY, qrPixel);
  // Text to the left of the QR
  g_tft->setTextDatum(MR_DATUM); // middle-right aligned
  g_tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
  g_tft->setTextFont(2);
  int textX = qrX - qrBorder - 12;
  int textCY = qrY + qrSize / 2;
  g_tft->drawString("Cambiar red WiFi ->", textX, textCY - 8);
  g_tft->setTextFont(1);
  g_tft->drawString("Escanea el QR", textX, textCY + 10);
  g_tft->setTextDatum(MC_DATUM); // restore default
}

void dashboard_hideNetworkInfo() {
  if (!g_tft) return;
  if (!g_showNetworkInfo) return;
  g_showNetworkInfo = false;
  resetAllDashboardCaches();
  g_tft->fillScreen(TFT_BLACK);
  drawScreenTab(g_tft, currentScreen);
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
    case DashboardScreen::SETUP:  drawSetupDashboard(g_tft, f);  break;
    case DashboardScreen::PITS:   drawPitsDashboard(g_tft, f);   break;
    case DashboardScreen::TYRES:  drawTyresDashboard(g_tft, f);  break;
    case DashboardScreen::TEMPS:  drawTempsDashboard(g_tft, f);  break;
    case DashboardScreen::ENGINE: drawEngineDashboard(g_tft, f); break;
  }
}
