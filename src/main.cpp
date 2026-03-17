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
  String html = "<html><head><title>F1Dash Setup</title></head><body>";
  html += "<h3>F1Dash Setup</h3>";
  html += "<form method='POST' action='/save'>";
  html += "SSID:<br><input name='ssid' maxlength='31' value='" + String(g_cfg.ssid) + "'><br>";
  html += "Password:<br><input name='pass' maxlength='63' value=''><br>";
  html += "Listen IP (optional):<br><input name='bind_ip' maxlength='15' value='" + String(g_cfg.bind_ip) + "'><br>";
  html += "Listen Port:<br><input name='port' maxlength='6' value='" + String(g_cfg.port == 0 ? F1_UDP_PORT : g_cfg.port) + "'><br>";
  html += "<br><input type='submit' value='Save'>";
  html += "</form></body></html>";
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
  server.send(200, "text/html", "Saved config. Restarting...");
  delay(1000);
  ESP.restart();
}

static void startConfigPortal() {
  Serial.println("Starting AP config portal: F1Dash-Setup");
  WiFi.mode(WIFI_AP);
  WiFi.softAP("F1Dash-Setup");
  IPAddress apIP = WiFi.softAPIP();
  Serial.print("AP IP: "); Serial.println(apIP);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
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

  // init display and dashboard
  dashboard_init(&tft);

}

void loop() {
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
