#include "telemetry_parser.h"
#include <Arduino.h>
#include <cstring>
#include "telemetry.h"

// Helper: safe reads (little-endian)
static inline uint8_t read_u8(const uint8_t* b, size_t len, size_t off) {
  return (off < len) ? b[off] : 0;
}
static inline uint16_t read_u16(const uint8_t* b, size_t len, size_t off) {
  if (off + 1 >= len) return 0;
  return (uint16_t)b[off] | ((uint16_t)b[off+1] << 8);
}
static inline uint32_t read_u32(const uint8_t* b, size_t len, size_t off) {
  if (off + 3 >= len) return 0;
  return (uint32_t)b[off] | ((uint32_t)b[off+1] << 8) | ((uint32_t)b[off+2] << 16) | ((uint32_t)b[off+3] << 24);
}
static inline float read_float(const uint8_t* b, size_t len, size_t off) {
  if (off + 3 >= len) return 0.0f;
  uint32_t v = read_u32(b, len, off);
  float f;
  memcpy(&f, &v, sizeof(f));
  return f;
}

// Very small, conservative parser: reads common fields with bounds checks.
// Offsets are based on typical F1 UDP packet header patterns; adjust per year if needed.
void telemetry_parse(const uint8_t* buf, size_t len, TelemetryFrame &out) {
  // preserve previous values if parsing fails
  // detect packetId at offset 5 (common in F1 UDP header)
  uint8_t packetId = 0;
  if (len > 5) packetId = read_u8(buf, len, 5);

  // frame identifier often appears after header; try safe read
  if (len > 20) {
    out.frameIdentifier = read_u32(buf, len, 18);
  }

  switch (packetId) {
    case F1Packets::PACKET_ID_CAR_TELEMETRY: {
      // Attempt to parse a single-car telemetry entry at a safe offset.
      // Many F1 UDP formats place speed (uint16) and rpm (uint16) in the telemetry struct.
      size_t base = 24; // conservative base offset after header
      if (len > base + 4) {
        uint16_t speed = read_u16(buf, len, base + 0);
        uint16_t rpm = read_u16(buf, len, base + 2);
        out.telemetry.speedKmh = speed;
        out.telemetry.rpm = rpm;
      }
      // throttle & brake bytes (scaled 0-255)
      if (len > base + 8) {
        out.telemetry.throttle = read_u8(buf, len, base + 8);
        out.telemetry.brake = read_u8(buf, len, base + 9);
      }
      // gear byte
      if (len > base + 12) out.telemetry.gear = (int8_t)read_u8(buf, len, base + 12);
      // drs flag
      if (len > base + 13) out.telemetry.drsActive = (read_u8(buf, len, base + 13) != 0);
      // mfd panel index (if present)
      if (len > base + 14) out.telemetry.mfdPanelIndex = read_u8(buf, len, base + 14);
    } break;

    case F1Packets::PACKET_ID_CAR_STATUS: {
      size_t base = 24;
      if (len > base + 0) out.status.ersEnergy = read_u16(buf, len, base + 0);
      if (len > base + 2) out.status.ersDeployMode = read_u8(buf, len, base + 2);
      if (len > base + 4) out.status.fuel = read_float(buf, len, base + 4);
      if (len > base + 8) out.status.brakeBias = read_u8(buf, len, base + 8);
    } break;

    case F1Packets::PACKET_ID_LAP_DATA: {
      size_t base = 24;
      if (len > base + 0) out.lap.lastLapTime = read_float(buf, len, base + 0);
      if (len > base + 4) out.lap.position = read_u8(buf, len, base + 4);
      if (len > base + 5) out.lap.pitStatus = read_u8(buf, len, base + 5);
    } break;

    default:
      // Unknown or unsupported packet - ignore
      break;
  }
}
