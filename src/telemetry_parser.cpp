#include "telemetry_parser.h"
#include <Arduino.h>
#include <cstring>
#include "telemetry.h"

// Safe little-endian readers
static inline uint8_t rd8(const uint8_t* b, size_t len, size_t off) {
  return (off < len) ? b[off] : 0;
}
static inline uint16_t rd16(const uint8_t* b, size_t len, size_t off) {
  if (off + 1 >= len) return 0;
  return (uint16_t)b[off] | ((uint16_t)b[off+1] << 8);
}
static inline uint32_t rd32(const uint8_t* b, size_t len, size_t off) {
  if (off + 3 >= len) return 0;
  return (uint32_t)b[off] | ((uint32_t)b[off+1]<<8) | ((uint32_t)b[off+2]<<16) | ((uint32_t)b[off+3]<<24);
}
static inline float rdF(const uint8_t* b, size_t len, size_t off) {
  if (off + 3 >= len) return 0.0f;
  uint32_t v = rd32(b, len, off);
  float f; memcpy(&f, &v, sizeof(f)); return f;
}
static inline uint8_t fto8(float v) {
  if (v <= 0.0f) return 0;
  if (v >= 1.0f) return 255;
  return (uint8_t)(v * 255.0f);
}

// ── Legacy parser for Go telemetry_sender (custom simple format) ──
static void parseLegacy(const uint8_t* buf, size_t len, TelemetryFrame &out) {
  uint8_t packetId = rd8(buf, len, 5);
  if (len > 20) out.frameIdentifier = rd32(buf, len, 18);
  size_t base = 24;
  switch (packetId) {
    case F1Packets::PACKET_ID_CAR_TELEMETRY:
      if (len > base + 4) {
        out.telemetry.speedKmh = rd16(buf, len, base);
        out.telemetry.rpm      = rd16(buf, len, base + 2);
      }
      if (len > base + 9) {
        out.telemetry.throttle = rd8(buf, len, base + 8);
        out.telemetry.brake    = rd8(buf, len, base + 9);
      }
      if (len > base + 12) out.telemetry.gear = (int8_t)rd8(buf, len, base + 12);
      if (len > base + 13) out.telemetry.drsActive = (rd8(buf, len, base + 13) != 0);
      if (len > base + 14) out.telemetry.mfdPanelIndex = rd8(buf, len, base + 14);
      break;
    case F1Packets::PACKET_ID_CAR_STATUS:
      if (len > base + 8) {
        out.status.ersEnergy    = rd16(buf, len, base);
        out.status.ersDeployMode = rd8(buf, len, base + 2);
        out.status.fuel          = rdF(buf, len, base + 4);
        out.status.brakeBias     = rd8(buf, len, base + 8);
      }
      break;
    case F1Packets::PACKET_ID_LAP_DATA:
      if (len > base + 5) {
        out.lap.lastLapTime = rdF(buf, len, base);
        out.lap.position    = rd8(buf, len, base + 4);
        out.lap.pitStatus   = rd8(buf, len, base + 5);
      }
      break;
  }
}

// ── Official F1 game parser (F1 22 / 23 / 24 / 25) ──
void telemetry_parse(const uint8_t* buf, size_t len, TelemetryFrame &out) {
  if (len < 8) return;

  uint16_t fmt = rd16(buf, len, 0);
  // Detect official F1 game by format year
  if (fmt < 2022 || fmt > 2030) {
    parseLegacy(buf, len, out);
    return;
  }

  // Header offsets differ between F1 22 and F1 23+
  size_t hdrSize, pidOff, fidOff, pciOff;
  if (fmt >= 2023) {
    hdrSize = 29; pidOff = 6; fidOff = 19; pciOff = 27;
  } else {
    hdrSize = 24; pidOff = 5; fidOff = 18; pciOff = 22;
  }
  if (len < hdrSize) return;

  uint8_t packetId = rd8(buf, len, pidOff);
  uint8_t pci      = rd8(buf, len, pciOff);
  if (pci >= 22) pci = 0;
  out.frameIdentifier = rd32(buf, len, fidOff);

  switch (packetId) {

    // ── Session Data (packet 1) ──
    // m_sessionType at offset hdrSize + 6
    // 1-4=practice, 5-9=quali, 10-12=race, 13=time trial
    case F1Packets::PACKET_ID_SESSION: {
      if (hdrSize + 7 <= len) {
        out.sessionType = rd8(buf, len, hdrSize + 6);
      }
    } break;

    // ── Car Telemetry (packet 6) ──
    // Per-car struct: 60 bytes (same layout F1 22-25)
    //   +0  uint16 speed
    //   +2  float  throttle (0.0-1.0)
    //   +6  float  steer
    //   +10 float  brake    (0.0-1.0)
    //   +14 uint8  clutch
    //   +15 int8   gear
    //   +16 uint16 engineRPM
    //   +18 uint8  drs
    //   +22 uint16[4] brakesTemperature  (RL,RR,FL,FR)
    //   +30 uint8[4]  tyresSurfaceTemp   (RL,RR,FL,FR)
    //   +34 uint8[4]  tyresInnerTemp     (RL,RR,FL,FR)
    //   +38 uint16    engineTemperature
    case F1Packets::PACKET_ID_CAR_TELEMETRY: {
      const size_t CS = 60;
      size_t base = hdrSize + (size_t)pci * CS;
      if (base + 40 > len) break;
      out.telemetry.speedKmh = rd16(buf, len, base);
      out.telemetry.throttle = fto8(rdF(buf, len, base + 2));
      out.telemetry.brake    = fto8(rdF(buf, len, base + 10));
      out.telemetry.gear     = (int8_t)rd8(buf, len, base + 15);
      out.telemetry.rpm      = rd16(buf, len, base + 16);
      out.telemetry.drsActive = (rd8(buf, len, base + 18) != 0);
      // Brake temps: packet order RL,RR,FL,FR → store as FL,FR,RL,RR
      out.telemetry.brakesTemp[0] = rd16(buf, len, base + 22 + 4); // FL
      out.telemetry.brakesTemp[1] = rd16(buf, len, base + 22 + 6); // FR
      out.telemetry.brakesTemp[2] = rd16(buf, len, base + 22);     // RL
      out.telemetry.brakesTemp[3] = rd16(buf, len, base + 22 + 2); // RR
      // Tyre surface temps: RL,RR,FL,FR → FL,FR,RL,RR
      out.telemetry.tyresSurfaceTemp[0] = rd8(buf, len, base + 30 + 2); // FL
      out.telemetry.tyresSurfaceTemp[1] = rd8(buf, len, base + 30 + 3); // FR
      out.telemetry.tyresSurfaceTemp[2] = rd8(buf, len, base + 30);     // RL
      out.telemetry.tyresSurfaceTemp[3] = rd8(buf, len, base + 30 + 1); // RR
      // Tyre inner temps
      out.telemetry.tyresInnerTemp[0] = rd8(buf, len, base + 34 + 2); // FL
      out.telemetry.tyresInnerTemp[1] = rd8(buf, len, base + 34 + 3); // FR
      out.telemetry.tyresInnerTemp[2] = rd8(buf, len, base + 34);     // RL
      out.telemetry.tyresInnerTemp[3] = rd8(buf, len, base + 34 + 1); // RR
      // Engine temp
      out.telemetry.engineTemp = rd16(buf, len, base + 38);
      // mfdPanelIndex after all 22 cars
      size_t mfdOff = hdrSize + 22 * CS;
      if (mfdOff < len) out.telemetry.mfdPanelIndex = rd8(buf, len, mfdOff);
    } break;

    // ── Car Status (packet 7) ──
    // Per-car: F1 22 = 47 bytes, F1 23+ = 55 bytes
    //   +3  uint8  frontBrakeBias
    //   +5  float  fuelInTank
    //   +13 float  fuelRemainingLaps
    //   +25 uint8  actualTyreCompound
    //   +26 uint8  visualTyreCompound
    //   +27 uint8  tyresAgeLaps
    //   ERS store energy: F1 22 at +29, F1 23+ at +37 (float, joules, max ~4MJ)
    //   ERS deploy mode:  F1 22 at +33, F1 23+ at +41
    case F1Packets::PACKET_ID_CAR_STATUS: {
      size_t CS  = (fmt >= 2023) ? 55 : 47;
      size_t base = hdrSize + (size_t)pci * CS;
      if (base + 28 > len) break;
      out.status.brakeBias = rd8(buf, len, base + 3);
      out.status.fuel      = rdF(buf, len, base + 5);
      out.status.fuelLaps  = rdF(buf, len, base + 13);
      out.status.tyreCompound = rd8(buf, len, base + 26); // visual compound
      out.status.tyresAgeLaps = rd8(buf, len, base + 27);
      size_t ersOff = (fmt >= 2023) ? 37 : 29;
      if (base + ersOff + 5 > len) break;
      float ersJ = rdF(buf, len, base + ersOff);
      out.status.ersEnergy     = (uint16_t)(ersJ / 4000000.0f * 1000.0f);
      out.status.ersDeployMode = rd8(buf, len, base + ersOff + 4);
    } break;

    // ── Lap Data (packet 2) ──
    // Per-car sizes (packed, from official spec):
    //   F1 22: 43 bytes  → carPosition at +24, pitStatus at +26
    //   F1 23: 50 bytes  → carPosition at +30, pitStatus at +32
    //   F1 24+: 50 bytes → same as F1 23 (adjust if spec changes)
    //   +0  uint32 lastLapTimeInMS
    case F1Packets::PACKET_ID_LAP_DATA: {
      size_t CS, posOff, pitOff;
      if (fmt >= 2023)      { CS = 50; posOff = 30; pitOff = 32; }
      else                  { CS = 43; posOff = 24; pitOff = 26; }
      size_t base = hdrSize + (size_t)pci * CS;
      if (base + pitOff + 1 > len) break;
      uint32_t ms = rd32(buf, len, base);
      out.lap.lastLapTime = (float)ms / 1000.0f;
      out.lap.position    = rd8(buf, len, base + posOff);
      out.lap.pitStatus   = rd8(buf, len, base + pitOff);
    } break;

    // ── Car Damage (packet 10) ──
    // Per-car: 52 bytes (same F1 22-25)
    //   +0  float[4]  tyresWear (RL,RR,FL,FR) — 0-100%
    //   +46 uint8 engineMGUHWear
    //   +47 uint8 engineESWear
    //   +48 uint8 engineCEWear
    //   +49 uint8 engineICEWear
    //   +50 uint8 engineMGUKWear
    //   +51 uint8 engineTCWear
    case F1Packets::PACKET_ID_CAR_DAMAGE: {
      const size_t CS = 52;
      size_t base = hdrSize + (size_t)pci * CS;
      if (base + 52 > len) break;
      // Tyre wear: packet RL,RR,FL,FR → store FL,FR,RL,RR
      float wFL = rdF(buf, len, base + 8);  // index 2 = FL
      float wFR = rdF(buf, len, base + 12); // index 3 = FR
      float wRL = rdF(buf, len, base + 0);  // index 0 = RL
      float wRR = rdF(buf, len, base + 4);  // index 1 = RR
      out.damage.tyresWear[0] = (uint8_t)(wFL < 0 ? 0 : (wFL > 100 ? 100 : wFL));
      out.damage.tyresWear[1] = (uint8_t)(wFR < 0 ? 0 : (wFR > 100 ? 100 : wFR));
      out.damage.tyresWear[2] = (uint8_t)(wRL < 0 ? 0 : (wRL > 100 ? 100 : wRL));
      out.damage.tyresWear[3] = (uint8_t)(wRR < 0 ? 0 : (wRR > 100 ? 100 : wRR));
      // Engine component wear
      out.damage.engineMGUHWear = rd8(buf, len, base + 46);
      out.damage.engineESWear   = rd8(buf, len, base + 47);
      out.damage.engineCEWear   = rd8(buf, len, base + 48);
      out.damage.engineICEWear  = rd8(buf, len, base + 49);
      out.damage.engineMGUKWear = rd8(buf, len, base + 50);
      out.damage.engineTCWear   = rd8(buf, len, base + 51);
    } break;

    default: break;
  }
}
