#pragma once
#include <Arduino.h>

// Minimal telemetry structs optimized for low RAM usage.
// Only fields required by the dashboards are stored.

// Car telemetry (per-player simplified)
struct CarTelemetry {
  uint16_t speedKmh;    // km/h
  uint16_t rpm;         // rpm
  int8_t gear;          // gear number (-1 reverse, 0 neutral, 1+ forward)
  uint8_t throttle;     // 0-255
  uint8_t brake;        // 0-255
  bool drsActive;
  uint8_t mfdPanelIndex; // MFD panel index reported by game
};

// Car status (simplified)
struct CarStatus {
  uint16_t ersEnergy;   // 0-1000 (scaled)
  uint8_t ersDeployMode;
  float fuel;           // liters
  uint8_t brakeBias;    // percent
};

// Lap data minimal
struct LapData {
  float lastLapTime;    // seconds
  uint8_t position;     // race position
  uint8_t pitStatus;    // 0 none, 1 pitting, 2 in pit
};

// Packet detection constants
namespace F1Packets {
  constexpr uint8_t PACKET_ID_LAP_DATA = 2;
  constexpr uint8_t PACKET_ID_CAR_TELEMETRY = 6;
  constexpr uint8_t PACKET_ID_CAR_STATUS = 7;
}

// Lightweight parser result container
struct TelemetryFrame {
  CarTelemetry telemetry;
  CarStatus status;
  LapData lap;
  uint32_t frameIdentifier;
};

// Forward declaration of parser function
// buffer: raw UDP packet, len: packet length, out: frame to update
void parseTelemetryPacket(const uint8_t* buffer, size_t len, TelemetryFrame &out);
