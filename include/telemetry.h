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
  // Temperatures (from packet 6) — ordered FL, FR, RL, RR
  uint16_t brakesTemp[4];       // °C
  uint8_t tyresSurfaceTemp[4];  // °C
  uint8_t tyresInnerTemp[4];    // °C
  uint16_t engineTemp;          // °C
};

// Car status (simplified)
struct CarStatus {
  uint16_t ersEnergy;   // 0-1000 (scaled)
  uint8_t ersDeployMode;
  float fuel;           // liters
  float fuelLaps;       // laps remaining
  uint8_t brakeBias;    // percent
  uint8_t tyreCompound; // visual compound (16=soft,17=med,18=hard,7=inter,8=wet)
  uint8_t tyresAgeLaps; // laps on current set
};

// Car damage / engine wear
struct CarDamage {
  uint8_t tyresWear[4];     // 0-100 % — FL, FR, RL, RR
  uint8_t engineMGUHWear;   // 0-100 %
  uint8_t engineESWear;     // 0-100 %
  uint8_t engineCEWear;     // 0-100 %
  uint8_t engineICEWear;    // 0-100 %
  uint8_t engineMGUKWear;   // 0-100 %
  uint8_t engineTCWear;     // 0-100 %
};

// Lap data minimal
struct LapData {
  float lastLapTime;    // seconds
  uint8_t position;     // race position
  uint8_t pitStatus;    // 0 none, 1 pitting, 2 in pit
};

// Packet detection constants
namespace F1Packets {
  constexpr uint8_t PACKET_ID_SESSION = 1;
  constexpr uint8_t PACKET_ID_LAP_DATA = 2;
  constexpr uint8_t PACKET_ID_CAR_TELEMETRY = 6;
  constexpr uint8_t PACKET_ID_CAR_STATUS = 7;
  constexpr uint8_t PACKET_ID_CAR_DAMAGE = 10;
}

// Lightweight parser result container
struct TelemetryFrame {
  CarTelemetry telemetry;
  CarStatus status;
  CarDamage damage;
  LapData lap;
  uint32_t frameIdentifier;
  uint8_t sessionType;  // 0=unknown,1-4=practice,5-9=quali,10-12=race,13=TT
};

// Forward declaration of parser function
// buffer: raw UDP packet, len: packet length, out: frame to update
void parseTelemetryPacket(const uint8_t* buffer, size_t len, TelemetryFrame &out);
