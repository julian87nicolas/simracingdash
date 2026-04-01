#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <TFT_eSPI.h>
#include "udp_listener.h"
#include "telemetry_parser.h"
#include "state.h"
#include "dashboard_manager.h"

// EEPROM config storage
struct SavedConfig {
  uint32_t magic;
  char ssid[32];
  char pass[64];
  uint16_t port;
};
static const uint32_t CONFIG_MAGIC = 0xA5A5A5A5;

// Defaults (used if no saved config)
const char* WIFI_SSID = "";
const char* WIFI_PASS = "";

// runtime config loaded from EEPROM
static SavedConfig g_cfg;

// simple webserver for setup portal
static ESP8266WebServer server(80);

// Display instance
static TFT_eSPI tft = TFT_eSPI();
static bool g_displayInit = false;

// Buffers
static uint8_t packetBuf[1500]; // F1 telemetry packets can be ~1352 bytes

// State
static StateManager stateMgr;
static uint16_t g_listenPort = F1_UDP_PORT;
static uint32_t g_lastPacketMs = 0;
static bool g_networkInfoVisible = false;

// helpers
static void loadConfig() {
  EEPROM.begin(512);
  EEPROM.get(0, g_cfg);
  if (g_cfg.magic != CONFIG_MAGIC) {
    // no valid config
    memset(&g_cfg, 0, sizeof(g_cfg));
    g_cfg.port = 0; // use default
  }
}

static void saveConfig() {
  g_cfg.magic = CONFIG_MAGIC;
  EEPROM.put(0, g_cfg);
  EEPROM.commit();
}

static void ensureDisplayInit() {
  if (g_displayInit) return;
  tft.init();
  tft.setRotation(1);
  // Color test to wake up the panel
  tft.fillScreen(TFT_RED);   delay(150);
  tft.fillScreen(TFT_GREEN); delay(150);
  tft.fillScreen(TFT_BLUE);  delay(150);
  tft.fillScreen(TFT_BLACK);
  g_displayInit = true;
}

static void showConfigByIPScreen(const IPAddress &apIP) {
  ensureDisplayInit();
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("WiFi no configurado", 240, 40);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Configura en:", 240, 90);

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  String url = "http://" + apIP.toString();
  tft.drawString(url.c_str(), 240, 140);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("SSID: F1Dash-Setup", 240, 200);

  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setTextFont(2);
  tft.drawString("Conecta a esa red y abre la URL", 240, 260);
}

static void handleRoot() {
  String html = "<!doctype html><html><head><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">";
  html += "<title>F1Dash Setup</title><style>body{font-family:Arial,Helvetica,sans-serif;padding:12px;}label{display:block;margin-top:8px;}input{width:100%;padding:8px;margin-top:4px;box-sizing:border-box;}button{margin-top:12px;padding:10px 14px;background:#0b79d0;color:#fff;border:none;border-radius:6px;}small{color:#666;}</style></head><body>";
  html += "<h2>F1Dash Setup</h2><p>Conecta tu móvil a la WiFi <strong>F1Dash-Setup</strong> y abre cualquier página; serás redirigido aquí.</p>";
  html += "<form method='POST' action='/save' onsubmit=\"return validate()\">";
  html += "<label>SSID<input name='ssid' maxlength='31' placeholder='Tu SSID' value='" + String(g_cfg.ssid) + "'></label>";
  html += "<label>Password<input name='pass' maxlength='63' placeholder='Contraseña (opcional)'></label>";
  html += "<label>Listen Port <input name='port' maxlength='6' placeholder='20777' value='" + String(g_cfg.port == 0 ? F1_UDP_PORT : g_cfg.port) + "'></label>";
  html += "<button type='submit'>Guardar y reiniciar</button>";
  html += "</form>";
  html += "<script>function validate(){let p=document.forms[0].port.value; if(p && (isNaN(p)||p<1||p>65535)){alert('Puerto inválido');return false;} return true;}";
  html += "if(location.hostname!='" + WiFi.softAPIP().toString() + "') location.href='http://" + WiFi.softAPIP().toString() + "';</script>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

static void handleSave() {
  if (server.hasArg("ssid")) {
    String s = server.arg("ssid");
    s.toCharArray(g_cfg.ssid, sizeof(g_cfg.ssid));
  }
  if (server.hasArg("pass")) {
    String p = server.arg("pass");
    p.toCharArray(g_cfg.pass, sizeof(g_cfg.pass));
  }
  if (server.hasArg("port")) {
    String sp = server.arg("port");
    int port = sp.toInt();
    if (port > 0 && port <= 65535) g_cfg.port = (uint16_t)port;
  }
  saveConfig();
  server.send(200, "text/html", "Saved config. Reiniciando...");
  delay(800);
  ESP.restart();
}

static void handleStatus() {
  String out = "{";
  // WiFi info
  out += "\"wifi\":{\"status\":\"" + String(WiFi.status() == WL_CONNECTED ? "connected" : "disconnected") + "\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
  // UDP listen port
  uint16_t listenPort = (g_cfg.port != 0) ? g_cfg.port : F1_UDP_PORT;
  out += ",\"udp\":{\"listen_port\":" + String(listenPort) + "}";
  // simple telemetry snapshot
  const TelemetryFrame &f = stateMgr.current();
  out += ",\"telemetry\":{";
  out += "\"speed_kmh\":" + String((unsigned int)f.telemetry.speedKmh) + ",";
  out += "\"rpm\":" + String((unsigned int)f.telemetry.rpm) + ",";
  out += "\"gear\":" + String((int)f.telemetry.gear) + ",";
  out += "\"throttle\":" + String((unsigned int)f.telemetry.throttle) + ",";
  out += "\"brake\":" + String((unsigned int)f.telemetry.brake) + ",";
  out += "\"fuel\":" + String(f.status.fuel, 2) + ",";
  out += "\"ers\":" + String((unsigned int)f.status.ersEnergy);
  out += "}";
  out += "}";
  server.send(200, "application/json", out);
}

static void startConfigPortal() {
  Serial.println("Starting AP config portal: F1Dash-Setup");
  WiFi.mode(WIFI_AP);
  WiFi.softAP("F1Dash-Setup");
  IPAddress apIP = WiFi.softAPIP();
  Serial.print("AP IP: "); Serial.println(apIP);
  showConfigByIPScreen(apIP);

  // Note: we don't run a DNS server here to avoid library/version conflicts.
  // The portal relies on the HTTP redirect below; on most mobile clients
  // opening any page will hit the AP and be redirected to the setup page.

  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  // redirect any unknown request to root (helps captive portals on mobiles)
  server.onNotFound([](){
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
    server.send(302, "text/plain", "");
  });

  server.begin();

  // block here serving portal until restart (handleSave will restart)
  while (true) {
    server.handleClient();
    delay(2);
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("F1 2025 UDP Dashboard - ESP8266" );

  // load config from EEPROM
  loadConfig();

  // If we have saved credentials, try to connect; otherwise start AP portal
  if (strlen(g_cfg.ssid) == 0) {
    // register status endpoint even in AP mode
    server.on("/status", HTTP_GET, handleStatus);
    startConfigPortal(); // does not return (restarts after save)
  }

  // Try connecting (DHCP first) to capture gateway/subnet if needed for static IP
  WiFi.mode(WIFI_STA);
  WiFi.begin(g_cfg.ssid, g_cfg.pass);
  Serial.print("Connecting to WiFi");
  uint32_t tstart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - tstart < 15000) {
    Serial.print('.'); delay(500);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("Connected (DHCP), IP: "); Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("WiFi connect failed - starting config portal");
    startConfigPortal();
  }

  // init UDP with configured port (fallback to default)
  g_listenPort = (g_cfg.port != 0) ? g_cfg.port : F1_UDP_PORT;
  if (!udp_init(g_listenPort)) {
    Serial.println("UDP init failed");
  } else {
    Serial.printf("Listening UDP port %d\n", g_listenPort);
  }

  // register status endpoint and start webserver in STA mode
  server.on("/status", HTTP_GET, handleStatus);
  server.begin();

  // init display and dashboard
  dashboard_init(&tft);
  String ip = WiFi.localIP().toString();
  dashboard_showNetworkInfo(ip.c_str(), g_listenPort, true);
  g_networkInfoVisible = true;

}

void loop() {
  // ═══════════════════════════════════════════════════════════════════
  // PRIORITY 1 — Drain ALL queued UDP packets (lowest latency path)
  // ═══════════════════════════════════════════════════════════════════
  // Parse into a single accumulator frame so we only keep the freshest
  // data and do a single state update after draining.
  bool gotNewData = false;
  TelemetryFrame frame = stateMgr.current();
  for (;;) {
    size_t len = udp_read_packet(packetBuf, sizeof(packetBuf));
    if (len == 0) break;
    g_lastPacketMs = millis();
    gotNewData = true;
    telemetry_parse(packetBuf, len, frame);
  }
  if (gotNewData) {
    stateMgr.updateFrame(frame);
  }

  uint32_t now = millis();

  // ═══════════════════════════════════════════════════════════════════
  // PRIORITY 2 — Render display (~20 FPS, only when data is fresh)
  // ═══════════════════════════════════════════════════════════════════
  static uint32_t lastRender = 0;
  if (now - lastRender >= 50) {
    // Network info overlay
    bool hasEverReceived = (g_lastPacketMs != 0);
    bool showNetInfo = (!hasEverReceived) || ((now - g_lastPacketMs) >= 10000);
    if (showNetInfo && !g_networkInfoVisible) {
      String ip = WiFi.localIP().toString();
      dashboard_showNetworkInfo(ip.c_str(), g_listenPort, !hasEverReceived);
      g_networkInfoVisible = true;
    } else if (!showNetInfo && g_networkInfoVisible) {
      dashboard_hideNetworkInfo();
      g_networkInfoVisible = false;
    }

    if (!g_networkInfoVisible) {
      dashboard_update(stateMgr);
    }
    lastRender = now;

    // Let LWIP process packets that arrived during SPI render.
    // WiFi hardware buffers raw frames while SPI blocks the CPU;
    // yield() gives LWIP time to move them into the UDP socket buffer
    // so the next drain loop finds them immediately.
    yield();
  }

  // ═══════════════════════════════════════════════════════════════════
  // LOW PRIORITY — Serial debug + web server
  // ═══════════════════════════════════════════════════════════════════
  static uint32_t lastSerial = 0;
  if (g_lastPacketMs != 0 && now - lastSerial >= 2000) {
    const TelemetryFrame &f = stateMgr.current();
    Serial.printf("[TEL] Spd:%u RPM:%u G:%d Thr:%u Brk:%u Pos:%u\n",
                  (unsigned)f.telemetry.speedKmh, (unsigned)f.telemetry.rpm,
                  (int)f.telemetry.gear, (unsigned)f.telemetry.throttle,
                  (unsigned)f.telemetry.brake, (unsigned)f.lap.position);
    lastSerial = now;
  }

  static uint32_t lastWeb = 0;
  if (now - lastWeb >= 1000) {
    server.handleClient();
    lastWeb = now;
  }
}
