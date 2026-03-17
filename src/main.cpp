#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <TFT_eSPI.h>
#include "udp_listener.h"
#include "telemetry_parser.h"
#include "state.h"
#include "dashboard_manager.h"

// Edit these to match your WiFi network
const char* WIFI_SSID = "YOUR_SSID";
const char* WIFI_PASS = "YOUR_PASS";

// Display instance
static TFT_eSPI tft = TFT_eSPI();

// Buffers
static uint8_t packetBuf[1024]; // keep small to save RAM

// State
static StateManager stateMgr;

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("F1 2025 UDP Dashboard - ESP8266" );

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  uint32_t tstart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - tstart < 15000) {
    Serial.print('.'); delay(500);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("Connected, IP: "); Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("WiFi connect failed - continue in fallback mode");
  }

  // init UDP
  if (!udp_init(F1_UDP_PORT)) {
    Serial.println("UDP init failed");
  } else {
    Serial.printf("Listening UDP port %d\n", F1_UDP_PORT);
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

  // update dashboard at ~25 FPS
  static uint32_t lastRender = 0;
  uint32_t now = millis();
  if (now - lastRender >= 40) {
    dashboard_update(stateMgr);
    lastRender = now;
  }
}
