#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <TFT_eSPI.h>
#include "qr_render.h"
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

// One-time CSRF token for the /do-reset-wifi action (0 = no pending reset)
static uint32_t g_resetToken = 0;

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

static void clearConfig() {
  memset(&g_cfg, 0, sizeof(g_cfg));
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

  // --- Left side: QR code ---
  String url = "http://" + apIP.toString();
  // QR version 3 = 29 modules; pixel size 4 => 116px
  int qrPixel = 4;
  int qrModules = 29; // version 3
  int qrSizePx = qrModules * qrPixel;
  int qrX = 30;
  int qrY = (320 - qrSizePx) / 2; // vertically centered
  drawQRCode(&tft, url.c_str(), qrX, qrY, qrPixel);

  // --- Right side: text info ---
  int textCenterX = 300; // center of right half

  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);

  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawString("WiFi Setup", textCenterX, 50);

  // Divider line
  tft.drawLine(200, 80, 460, 80, TFT_DARKGREY);

  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("1. Conecta a la red WiFi:", textCenterX, 105);

  tft.setTextFont(4);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("F1Dash-Setup", textCenterX, 140);

  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("2. Escanea el QR o abre:", textCenterX, 180);

  tft.setTextFont(4);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(url.c_str(), textCenterX, 215);

  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("3. Configura tu red WiFi", textCenterX, 255);

  // Footer
  tft.drawLine(200, 280, 460, 280, TFT_DARKGREY);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("F1 Dashboard - Setup Mode", textCenterX, 300);
}

static void handleRoot() {
  String html = R"rawliteral(
<!doctype html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>F1 Dashboard - Setup</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:'Segoe UI',system-ui,-apple-system,sans-serif;background:#0a0a0a;color:#e0e0e0;min-height:100vh;display:flex;justify-content:center;align-items:center}
.card{background:#141414;border:1px solid #2a2a2a;border-radius:16px;padding:36px 32px;max-width:420px;width:90%;box-shadow:0 8px 32px rgba(0,0,0,.6)}
.logo{text-align:center;margin-bottom:24px}
.logo h1{font-size:1.6em;font-weight:700;color:#fff;letter-spacing:1px}
.logo h1 span{color:#e10600}
.logo p{font-size:.85em;color:#666;margin-top:4px}
.divider{height:1px;background:linear-gradient(90deg,transparent,#e10600,transparent);margin:20px 0}
label{display:block;margin-bottom:14px}
label .lbl{font-size:.8em;color:#888;text-transform:uppercase;letter-spacing:.5px;margin-bottom:6px;display:block}
input,select{width:100%;padding:12px 14px;background:#1e1e1e;border:1px solid #333;border-radius:8px;color:#fff;font-size:.95em;transition:border .2s}
input:focus,select:focus{outline:none;border-color:#e10600}
input::placeholder{color:#555}
select option{background:#1e1e1e;color:#fff}
.hint{font-size:.75em;color:#555;margin-top:3px}
.ssid-wrap select{margin-bottom:6px}
.ssid-wrap .btn-row{display:flex;gap:8px}
.ssid-wrap .btn-row button{flex:1;padding:10px;background:#1e1e1e;border:1px solid #333;border-radius:8px;color:#888;font-size:.8em;cursor:pointer;margin:0}
.ssid-wrap .btn-row button:hover{border-color:#e10600;color:#e10600}
.ssid-wrap .btn-row button.active{border-color:#e10600;color:#e10600}
.ssid-wrap .btn-row button.loading{color:#555;pointer-events:none}
btn,button{display:block;width:100%;padding:14px;background:linear-gradient(135deg,#e10600,#b30500);color:#fff;border:none;border-radius:10px;font-size:1em;font-weight:600;cursor:pointer;letter-spacing:.5px;margin-top:8px;transition:transform .15s,box-shadow .15s}
button:hover{transform:translateY(-1px);box-shadow:0 4px 16px rgba(225,6,0,.35)}
button:active{transform:translateY(0)}
.footer{text-align:center;margin-top:20px;font-size:.72em;color:#444}
</style>
</head>
<body>
<div class="card">
<div class="logo">
<h1><span>F1</span> Dashboard</h1>
<p>Configuraci&oacute;n de red</p>
</div>
<div class="divider"></div>
<form method="POST" action="/save" onsubmit="return validate()">
<label>
<span class="lbl">Red WiFi (SSID)</span>
<div class="ssid-wrap">
<select name="ssid" id="ssidSel"><option value="">-- Escaneando... --</option></select>
<input name="ssid_manual" id="ssidMan" maxlength="31" placeholder="Escribe el nombre de la red" style="display:none" autocomplete="off">
<div class="btn-row">
<button type="button" id="scanBtn" onclick="scanWifi()">Buscar redes</button>
<button type="button" id="manBtn" onclick="toggleManual()">Escribir manual</button>
</div>
</div>
</label>
<label>
<span class="lbl">Contrase&ntilde;a</span>
<input name="pass" type="password" maxlength="63" placeholder="Contrase&ntilde;a WiFi (opcional)" autocomplete="off">
</label>
<label>
<span class="lbl">Puerto UDP</span>
<input name="port" maxlength="6" placeholder="20777" value=")rawliteral";
  html += String(g_cfg.port == 0 ? F1_UDP_PORT : g_cfg.port);
  html += R"rawliteral(">
<span class="hint">Puerto de escucha para telemetr&iacute;a F1 (default: 20777)</span>
</label>
<button type="submit">Guardar y reiniciar</button>
</form>
<div class="footer">F1 Dashboard &bull; SimRacing Dash</div>
</div>
<script>
function validate(){
var sel=document.getElementById('ssidSel');
var man=document.getElementById('ssidMan');
var useManual=man.style.display!=='none';
var ssid=useManual?man.value:sel.value;
if(!ssid){alert('Selecciona o escribe una red WiFi');return false;}
if(useManual){sel.disabled=true;man.name='ssid';}else{man.disabled=true;sel.name='ssid';}
var p=document.forms[0].port.value;
if(p&&(isNaN(p)||p<1||p>65535)){alert('Puerto inv\u00e1lido');return false;}
return true;
}
function scanWifi(){
var btn=document.getElementById('scanBtn');
var sel=document.getElementById('ssidSel');
btn.classList.add('loading');btn.textContent='Buscando...';
sel.innerHTML='<option value="">Escaneando...</option>';
fetch('/scan').then(function(r){return r.json();}).then(function(nets){
sel.innerHTML='';
if(!nets.length){sel.innerHTML='<option value="">No se encontraron redes</option>';}
else{
var seen={};
nets.sort(function(a,b){return b.rssi-a.rssi;});
nets.forEach(function(n){
if(!n.ssid||seen[n.ssid])return;seen[n.ssid]=1;
var o=document.createElement('option');
var pct=Math.min(100,Math.max(0,2*(n.rssi+100)));
var sig=pct>=75?'Excelente':pct>=50?'Buena':pct>=25?'Regular':'Debil';
o.value=n.ssid;
o.textContent=n.ssid+(n.enc?' (protegida)':' (abierta)')+' - '+sig;
sel.appendChild(o);
});
}
btn.classList.remove('loading');btn.textContent='Buscar redes';
}).catch(function(){sel.innerHTML='<option value="">Error al escanear</option>';btn.classList.remove('loading');btn.textContent='Buscar redes';});
}
function toggleManual(){
var man=document.getElementById('ssidMan');
var sel=document.getElementById('ssidSel');
var manBtn=document.getElementById('manBtn');
var scanBtn=document.getElementById('scanBtn');
if(man.style.display==='none'){man.style.display='';sel.style.display='none';manBtn.classList.add('active');scanBtn.classList.remove('active');}
else{man.style.display='none';sel.style.display='';manBtn.classList.remove('active');scanBtn.classList.remove('active');}
}
scanWifi();
)rawliteral";
  html += "if(location.hostname!='" + WiFi.softAPIP().toString() + "')location.href='http://" + WiFi.softAPIP().toString() + "';";
  html += R"rawliteral(
</script>
</body>
</html>)rawliteral";
  server.send(200, "text/html", html);
}

static void handleScan() {
  // Scan must be done in STA or AP_STA mode
  WiFi.mode(WIFI_AP_STA);
  int n = WiFi.scanNetworks();
  String json = "[";
  for (int i = 0; i < n; i++) {
    if (i > 0) json += ",";
    json += "{\"ssid\":\"";
    // Escape any quotes in SSID
    String ssid = WiFi.SSID(i);
    ssid.replace("\"", "\\\"");
    json += ssid;
    json += "\",\"rssi\":";
    json += String(WiFi.RSSI(i));
    json += ",\"enc\":";
    json += String(WiFi.encryptionType(i) != ENC_TYPE_NONE ? 1 : 0);
    json += "}";
  }
  json += "]";
  WiFi.scanDelete();
  WiFi.mode(WIFI_AP);
  server.send(200, "application/json", json);
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
  server.on("/scan", HTTP_GET, handleScan);
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

  // Check FLASH button (GPIO0 / D3) — hold during boot to force config portal
  pinMode(D3, INPUT_PULLUP);
  delay(50); // debounce
  if (digitalRead(D3) == LOW) {
    Serial.println("FLASH button held - forcing config portal");
    clearConfig();
  }

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

  // register status endpoint and reset-wifi endpoint, then start webserver
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/reset-wifi", HTTP_GET, [](){
    // Generate a fresh one-time token for this page load
    do { g_resetToken = ESP.random(); } while (g_resetToken == 0);
    char tokenStr[16];
    snprintf(tokenStr, sizeof(tokenStr), "%lu", (unsigned long)g_resetToken);
    String html = R"rawliteral(
<!doctype html><html lang="es"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>F1 Dashboard - Reset WiFi</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:'Segoe UI',system-ui,sans-serif;background:#0a0a0a;color:#e0e0e0;min-height:100vh;display:flex;justify-content:center;align-items:center}
.card{background:#141414;border:1px solid #2a2a2a;border-radius:16px;padding:36px 32px;max-width:420px;width:90%;box-shadow:0 8px 32px rgba(0,0,0,.6);text-align:center}
h1{font-size:1.4em;color:#fff;margin-bottom:8px}h1 span{color:#e10600}
p{color:#888;font-size:.9em;margin-bottom:20px}
.divider{height:1px;background:linear-gradient(90deg,transparent,#e10600,transparent);margin:20px 0}
button{display:block;width:100%;padding:14px;background:linear-gradient(135deg,#e10600,#b30500);color:#fff;border:none;border-radius:10px;font-size:1em;font-weight:600;cursor:pointer;margin-top:8px}
button:hover{box-shadow:0 4px 16px rgba(225,6,0,.35)}
.cancel{background:#333;margin-top:10px}
.cancel:hover{background:#444;box-shadow:none}
</style></head><body>
<div class="card">
<h1><span>F1</span> Dashboard</h1>
<p>Reconfigurar WiFi</p>
<div class="divider"></div>
<p>Se borrar&aacute; la configuraci&oacute;n WiFi actual y el dispositivo reiniciar&aacute; en modo AP para que puedas elegir otra red.</p>
<form method="POST" action="/do-reset-wifi">
<input type="hidden" name="csrf_token" value=")rawliteral";
    html += tokenStr;
    html += R"rawliteral(">
<button type="submit">Borrar WiFi y reiniciar</button>
</form>
<button class="cancel" onclick="history.back()">Cancelar</button>
</div></body></html>)rawliteral";
    server.send(200, "text/html", html);
  });
  server.on("/do-reset-wifi", HTTP_POST, [](){
    // Validate CSRF token: must match the one issued by GET /reset-wifi
    static const char* kForbidden = "<html><body style='background:#0a0a0a;color:#fff;font-family:sans-serif;text-align:center;padding-top:40vh'><h2>Solicitud inv&aacute;lida (403)</h2></body></html>";
    auto rejectRequest = [&]() {
      g_resetToken = 0; // invalidate on any failed attempt
      server.send(403, "text/html", kForbidden);
    };

    String submitted = server.arg("csrf_token");
    // Validate: non-empty, digits only
    if (submitted.length() == 0) { rejectRequest(); return; }
    for (unsigned int i = 0; i < submitted.length(); i++) {
      if (!isdigit((unsigned char)submitted[i])) { rejectRequest(); return; }
    }
    // Guard against values that would overflow uint32_t on 64-bit hosts
    unsigned long parsed = strtoul(submitted.c_str(), nullptr, 10);
    if (parsed > 0xFFFFFFFFUL) { rejectRequest(); return; }
    uint32_t submittedToken = (uint32_t)parsed;
    if (g_resetToken == 0 || submittedToken != g_resetToken) {
      rejectRequest(); return;
    }
    g_resetToken = 0; // consume the token (one-time use)
    clearConfig();
    server.send(200, "text/html", "<html><body style='background:#0a0a0a;color:#fff;font-family:sans-serif;text-align:center;padding-top:40vh'><h2>WiFi borrado. Reiniciando...</h2></body></html>");
    delay(800);
    ESP.restart();
  });
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
