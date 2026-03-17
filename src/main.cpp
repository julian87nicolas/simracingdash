#include <Arduino.h>
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
  char bind_ip[16];
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

// Buffers
static uint8_t packetBuf[1024]; // keep small to save RAM

// State
static StateManager stateMgr;

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

static IPAddress ipFromString(const char* s) {
  int a,b,c,d;
  if (sscanf(s, "%d.%d.%d.%d", &a,&b,&c,&d) == 4) {
    return IPAddress(a,b,c,d);
  }
  return IPAddress(0,0,0,0);
}

static void handleRoot() {
  String html = "<!doctype html><html><head><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">";
  html += "<title>F1Dash Setup</title><style>body{font-family:Arial,Helvetica,sans-serif;padding:12px;}label{display:block;margin-top:8px;}input{width:100%;padding:8px;margin-top:4px;box-sizing:border-box;}button{margin-top:12px;padding:10px 14px;background:#0b79d0;color:#fff;border:none;border-radius:6px;}small{color:#666;}</style></head><body>";
  html += "<h2>F1Dash Setup</h2><p>Conecta tu móvil a la WiFi <strong>F1Dash-Setup</strong> y abre cualquier página; serás redirigido aquí.</p>";
  html += "<form method='POST' action='/save' onsubmit=\"return validate()\">";
  html += "<label>SSID<input name='ssid' maxlength='31' placeholder='Tu SSID' value='" + String(g_cfg.ssid) + "'></label>";
  html += "<label>Password<input name='pass' maxlength='63' placeholder='Contraseña (opcional)'></label>";
  html += "<label>Listen IP (opcional) <small>ej. 192.168.1.50</small><input name='bind_ip' maxlength='15' placeholder='0.0.0.0' value='" + String(g_cfg.bind_ip) + "'></label>";
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
  if (server.hasArg("bind_ip")) {
    String ip = server.arg("bind_ip");
    ip.toCharArray(g_cfg.bind_ip, sizeof(g_cfg.bind_ip));
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

  // If user requested a specific bind IP, attempt to apply static IP using
  // gateway/subnet discovered via DHCP above. Reconnect with static IP.
  if (strlen(g_cfg.bind_ip) > 0) {
    IPAddress gw = WiFi.gatewayIP();
    IPAddress sn = WiFi.subnetMask();
    IPAddress desired = ipFromString(g_cfg.bind_ip);
    if (desired != IPAddress(0,0,0,0)) {
      Serial.print("Applying static IP: "); Serial.println(desired);
      WiFi.disconnect(true);
      delay(500);
      bool okcfg = WiFi.config(desired, gw, sn);
      (void)okcfg;
      WiFi.begin(g_cfg.ssid, g_cfg.pass);
      uint32_t t2 = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - t2 < 10000) {
        delay(200);
      }
      if (WiFi.status() == WL_CONNECTED) {
        Serial.print("Connected with static IP: "); Serial.println(WiFi.localIP());
      } else {
        Serial.println("Static IP config failed, continuing with DHCP");
      }
    }
  }

  // init UDP with configured port (fallback to default)
  uint16_t listenPort = (g_cfg.port != 0) ? g_cfg.port : F1_UDP_PORT;
  if (!udp_init(listenPort)) {
    Serial.println("UDP init failed");
  } else {
    Serial.printf("Listening UDP port %d\n", listenPort);
  }

  // register status endpoint and start webserver in STA mode
  server.on("/status", HTTP_GET, handleStatus);
  server.begin();

  // init display and dashboard
  dashboard_init(&tft);

}

void loop() {
  // handle web server clients (status endpoint) in STA mode
  server.handleClient();

  // read UDP packet
  size_t len = udp_read_packet(packetBuf, sizeof(packetBuf));
  if (len > 0) {
    TelemetryFrame frame;
    // keep previous values if parse only partial
    frame = stateMgr.current();
    telemetry_parse(packetBuf, len, frame);
    stateMgr.updateFrame(frame);
  }

  // Pretty-print received telemetry to Serial (rate-limited)
  static uint32_t lastSerial = 0;
  uint32_t nowSerial = millis();
  if (nowSerial - lastSerial >= 200) { // at most 5 prints/sec
    const TelemetryFrame &f = stateMgr.current();
    Serial.println("---------------- Telemetry Frame ----------------");
    Serial.printf("Frame ID: %lu\n", (unsigned long)f.frameIdentifier);
    Serial.printf("Speed: %u km/h   RPM: %u   Gear: %d   DRS: %s\n",
                  (unsigned int)f.telemetry.speedKmh,
                  (unsigned int)f.telemetry.rpm,
                  (int)f.telemetry.gear,
                  f.telemetry.drsActive ? "ON" : "OFF");
    Serial.printf("Throttle: %u   Brake: %u   MFD: %u\n",
                  (unsigned int)f.telemetry.throttle,
                  (unsigned int)f.telemetry.brake,
                  (unsigned int)f.telemetry.mfdPanelIndex);
    Serial.printf("ERS: %u (mode %u)   Fuel: %.1f L   BrakeBias: %u\n",
                  (unsigned int)f.status.ersEnergy,
                  (unsigned int)f.status.ersDeployMode,
                  f.status.fuel,
                  (unsigned int)f.status.brakeBias);
    Serial.printf("Lap: last=%.3f s   Pos: %u   PitStatus: %u\n",
                  f.lap.lastLapTime,
                  (unsigned int)f.lap.position,
                  (unsigned int)f.lap.pitStatus);
    lastSerial = nowSerial;
  }

  // update dashboard at ~25 FPS
  static uint32_t lastRender = 0;
  uint32_t now = millis();
  if (now - lastRender >= 40) {
    dashboard_update(stateMgr);
    lastRender = now;
  }
}
