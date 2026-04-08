#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>
#include "../../include/telemetry.h"
#include "../../include/telemetry_parser.h"

// ─── helpers ─────────────────────────────────────────────────────────────────

static void writeU16(uint8_t* buf, size_t off, uint16_t v) {
  buf[off]   = v & 0xFF;
  buf[off+1] = (v >> 8) & 0xFF;
}
static void writeU32(uint8_t* buf, size_t off, uint32_t v) {
  buf[off]   = v & 0xFF;
  buf[off+1] = (v >> 8) & 0xFF;
  buf[off+2] = (v >> 16) & 0xFF;
  buf[off+3] = (v >> 24) & 0xFF;
}
static void writeF32(uint8_t* buf, size_t off, float v) {
  uint32_t bits = 0; memcpy(&bits, &v, 4);
  writeU32(buf, off, bits);
}

// ─── Legacy path (fmt < 2022) ─────────────────────────────────────────────────

static void test_legacy_telemetry() {
  uint8_t buf[64];
  memset(buf, 0, sizeof(buf));
  buf[5] = F1Packets::PACKET_ID_CAR_TELEMETRY;
  buf[24] = 123 & 0xFF;
  buf[25] = (123 >> 8) & 0xFF;
  buf[26] = 7500 & 0xFF;
  buf[27] = (7500 >> 8) & 0xFF;
  buf[32] = 200; // throttle
  buf[33] = 10;  // brake
  buf[36] = 4;   // gear
  buf[37] = 1;   // drs
  buf[38] = 1;   // mfdPanelIndex

  TelemetryFrame out{};
  telemetry_parse(buf, sizeof(buf), out);

  assert(out.telemetry.speedKmh == 123);
  assert(out.telemetry.rpm == 7500);
  assert(out.telemetry.throttle == 200);
  assert(out.telemetry.brake == 10);
  assert(out.telemetry.gear == 4);
  assert(out.telemetry.drsActive == true);
  assert(out.telemetry.mfdPanelIndex == 1);
  std::cout << "PASS: legacy_telemetry\n";
}

// ─── Official F1 23+ path ─────────────────────────────────────────────────────
// hdrSize=29, pidOff=6, fidOff=19, pciOff=27

static void test_f2023_session() {
  // sessionType at hdrSize+6 = 29+6 = 35
  uint8_t buf[40];
  memset(buf, 0, sizeof(buf));
  writeU16(buf, 0, 2023);
  buf[6] = F1Packets::PACKET_ID_SESSION;
  buf[35] = 10;  // sessionType = race

  TelemetryFrame out{};
  telemetry_parse(buf, sizeof(buf), out);

  assert(out.sessionType == 10);
  std::cout << "PASS: f2023_session\n";
}

static void test_f2023_car_telemetry() {
  // hdrSize=29, pci=0, CS=60, base=29; need base+CS=89 bytes
  const size_t BUFSZ = 89;
  uint8_t buf[BUFSZ];
  memset(buf, 0, BUFSZ);
  writeU16(buf, 0, 2023);
  buf[6]  = F1Packets::PACKET_ID_CAR_TELEMETRY;
  buf[27] = 0;  // pci = 0

  // base=29
  writeU16(buf, 29, 250);       // speed = 250 km/h
  writeF32(buf, 31, 0.8f);      // throttle → fto8 = 204
  writeF32(buf, 39, 0.5f);      // brake    → fto8 = 127
  buf[44] = 6;                   // gear = 6
  writeU16(buf, 45, 12000);     // rpm = 12000
  buf[47] = 1;                   // drs active
  // brakesTemp at base+22=51, packet order RL,RR,FL,FR
  writeU16(buf, 51, 300);  // RL
  writeU16(buf, 53, 310);  // RR
  writeU16(buf, 55, 320);  // FL
  writeU16(buf, 57, 330);  // FR
  // tyresSurfaceTemp at base+30=59, order RL,RR,FL,FR
  buf[59] = 80; buf[60] = 82; buf[61] = 85; buf[62] = 87;
  // tyresInnerTemp at base+34=63, order RL,RR,FL,FR
  buf[63] = 90; buf[64] = 92; buf[65] = 95; buf[66] = 97;
  // engineTemp at base+38=67
  writeU16(buf, 67, 105);

  TelemetryFrame out{};
  telemetry_parse(buf, BUFSZ, out);

  assert(out.telemetry.speedKmh == 250);
  assert(out.telemetry.throttle == 204);
  assert(out.telemetry.brake == 127);
  assert(out.telemetry.gear == 6);
  assert(out.telemetry.rpm == 12000);
  assert(out.telemetry.drsActive == true);
  // stored FL,FR,RL,RR
  assert(out.telemetry.brakesTemp[0] == 320);  // FL
  assert(out.telemetry.brakesTemp[1] == 330);  // FR
  assert(out.telemetry.brakesTemp[2] == 300);  // RL
  assert(out.telemetry.brakesTemp[3] == 310);  // RR
  // stored FL,FR,RL,RR
  assert(out.telemetry.tyresSurfaceTemp[0] == 85);  // FL
  assert(out.telemetry.tyresSurfaceTemp[1] == 87);  // FR
  assert(out.telemetry.tyresSurfaceTemp[2] == 80);  // RL
  assert(out.telemetry.tyresSurfaceTemp[3] == 82);  // RR
  assert(out.telemetry.tyresInnerTemp[0] == 95);  // FL
  assert(out.telemetry.tyresInnerTemp[1] == 97);  // FR
  assert(out.telemetry.tyresInnerTemp[2] == 90);  // RL
  assert(out.telemetry.tyresInnerTemp[3] == 92);  // RR
  assert(out.telemetry.engineTemp == 105);
  std::cout << "PASS: f2023_car_telemetry\n";
}

static void test_f2023_car_telemetry_mfd() {
  // mfdPanelIndex at hdrSize + 22*CS = 29 + 22*60 = 1349
  const size_t BUFSZ = 1350;
  std::vector<uint8_t> buf(BUFSZ, 0);
  writeU16(buf.data(), 0, 2023);
  buf[6]  = F1Packets::PACKET_ID_CAR_TELEMETRY;
  buf[27] = 0;     // pci=0
  buf[1349] = 3;   // mfdPanelIndex = 3

  TelemetryFrame out{};
  telemetry_parse(buf.data(), BUFSZ, out);

  assert(out.telemetry.mfdPanelIndex == 3);
  std::cout << "PASS: f2023_car_telemetry_mfd\n";
}

static void test_f2023_car_status() {
  // hdrSize=29, CS=55, base=29; ersOff=37 → need base+37+5=71 bytes
  const size_t BUFSZ = 100;
  uint8_t buf[BUFSZ];
  memset(buf, 0, BUFSZ);
  writeU16(buf, 0, 2023);
  buf[6]  = F1Packets::PACKET_ID_CAR_STATUS;
  buf[27] = 0;  // pci=0

  // base=29
  buf[32] = 58;                  // brakeBias at base+3
  writeF32(buf, 34, 22.5f);     // fuel at base+5
  writeF32(buf, 42, 8.3f);      // fuelLaps at base+13
  buf[55] = 17;                  // tyreCompound (visual) at base+26
  buf[56] = 5;                   // tyresAgeLaps at base+27
  // ersOff=37 → base+37=66
  writeF32(buf, 66, 2000000.0f); // ersEnergy = 2 MJ → scaled 500
  buf[70] = 2;                   // ersDeployMode at base+41

  TelemetryFrame out{};
  telemetry_parse(buf, BUFSZ, out);

  assert(out.status.brakeBias == 58);
  assert(out.status.fuel > 22.4f && out.status.fuel < 22.6f);
  assert(out.status.fuelLaps > 8.2f && out.status.fuelLaps < 8.4f);
  assert(out.status.tyreCompound == 17);
  assert(out.status.tyresAgeLaps == 5);
  assert(out.status.ersEnergy == 500);
  assert(out.status.ersDeployMode == 2);
  std::cout << "PASS: f2023_car_status\n";
}

static void test_f2023_lap_data() {
  // hdrSize=29, CS=50, base=29; posOff=30→59, pitOff=32→61
  const size_t BUFSZ = 70;
  uint8_t buf[BUFSZ];
  memset(buf, 0, BUFSZ);
  writeU16(buf, 0, 2023);
  buf[6]  = F1Packets::PACKET_ID_LAP_DATA;
  buf[27] = 0;  // pci=0

  writeU32(buf, 29, 85300);  // lastLapTimeInMS = 85300 → 85.3 s
  buf[59] = 3;               // position at base+30
  buf[61] = 1;               // pitStatus at base+32

  TelemetryFrame out{};
  telemetry_parse(buf, BUFSZ, out);

  assert(out.lap.lastLapTime > 85.29f && out.lap.lastLapTime < 85.31f);
  assert(out.lap.position == 3);
  assert(out.lap.pitStatus == 1);
  std::cout << "PASS: f2023_lap_data\n";
}

static void test_f2023_car_setup() {
  // hdrSize=29, CS=49, base=29; diffOnThrottle at base+2=31, offThrottle at base+3=32
  const size_t BUFSZ = 40;
  uint8_t buf[BUFSZ];
  memset(buf, 0, BUFSZ);
  writeU16(buf, 0, 2023);
  buf[6]  = F1Packets::PACKET_ID_CAR_SETUP;
  buf[27] = 0;  // pci=0

  buf[31] = 65;  // diffOnThrottle
  buf[32] = 55;  // diffOffThrottle

  TelemetryFrame out{};
  telemetry_parse(buf, BUFSZ, out);

  assert(out.status.diffOnThrottle  == 65);
  assert(out.status.diffOffThrottle == 55);
  std::cout << "PASS: f2023_car_setup\n";
}

static void test_f2023_car_damage() {
  // hdrSize=29, CS=42, base=29, ewOff=34; need base+CS=71 bytes
  const size_t BUFSZ = 80;
  uint8_t buf[BUFSZ];
  memset(buf, 0, BUFSZ);
  writeU16(buf, 0, 2023);
  buf[6]  = F1Packets::PACKET_ID_CAR_DAMAGE;
  buf[27] = 0;  // pci=0

  // tyresWear as float[4] at base+0..+15, packet order RL,RR,FL,FR
  writeF32(buf, 29, 15.0f);  // RL
  writeF32(buf, 33, 20.0f);  // RR
  writeF32(buf, 37, 25.0f);  // FL
  writeF32(buf, 41, 30.0f);  // FR
  // engine wear at base+ewOff = 29+34=63
  buf[63] = 10;  // MGUHWear
  buf[64] = 20;  // ESWear
  buf[65] = 30;  // CEWear
  buf[66] = 40;  // ICEWear
  buf[67] = 50;  // MGUKWear
  buf[68] = 60;  // TCWear

  TelemetryFrame out{};
  telemetry_parse(buf, BUFSZ, out);

  // stored FL,FR,RL,RR
  assert(out.damage.tyresWear[0] == 25);  // FL
  assert(out.damage.tyresWear[1] == 30);  // FR
  assert(out.damage.tyresWear[2] == 15);  // RL
  assert(out.damage.tyresWear[3] == 20);  // RR
  assert(out.damage.engineMGUHWear == 10);
  assert(out.damage.engineESWear   == 20);
  assert(out.damage.engineCEWear   == 30);
  assert(out.damage.engineICEWear  == 40);
  assert(out.damage.engineMGUKWear == 50);
  assert(out.damage.engineTCWear   == 60);
  std::cout << "PASS: f2023_car_damage\n";
}

// ─── Official F1 22 path ──────────────────────────────────────────────────────
// hdrSize=24, pidOff=5, fidOff=18, pciOff=22

static void test_f2022_car_status() {
  // CS=47, ersOff=29; need base+ersOff+5 = 24+29+5 = 58 bytes
  const size_t BUFSZ = 90;
  uint8_t buf[BUFSZ];
  memset(buf, 0, BUFSZ);
  writeU16(buf, 0, 2022);
  buf[5]  = F1Packets::PACKET_ID_CAR_STATUS;
  buf[22] = 0;  // pci=0

  // base=24
  buf[27] = 52;                  // brakeBias at base+3
  writeF32(buf, 29, 18.0f);     // fuel at base+5
  writeF32(buf, 37, 5.5f);      // fuelLaps at base+13
  buf[50] = 16;                  // tyreCompound at base+26
  buf[51] = 3;                   // tyresAgeLaps at base+27
  // ersOff=29 → base+29=53
  writeF32(buf, 53, 1000000.0f); // ersEnergy = 1 MJ → scaled 250
  buf[57] = 3;                   // ersDeployMode at base+33

  TelemetryFrame out{};
  telemetry_parse(buf, BUFSZ, out);

  assert(out.status.brakeBias == 52);
  assert(out.status.fuel > 17.9f && out.status.fuel < 18.1f);
  assert(out.status.fuelLaps > 5.4f && out.status.fuelLaps < 5.6f);
  assert(out.status.tyreCompound == 16);
  assert(out.status.tyresAgeLaps == 3);
  assert(out.status.ersEnergy == 250);
  assert(out.status.ersDeployMode == 3);
  std::cout << "PASS: f2022_car_status\n";
}

// ─── F1 24 path (same header as F1 23, but CarSetup is 50 bytes) ──────────────

static void test_f2024_car_setup() {
  // hdrSize=29, CS=50 (F1 24+), diffOnThrottle at base+2, offThrottle at base+3
  const size_t BUFSZ = 80;
  uint8_t buf[BUFSZ];
  memset(buf, 0, BUFSZ);
  writeU16(buf, 0, 2024);
  buf[6]  = F1Packets::PACKET_ID_CAR_SETUP;
  buf[27] = 0;  // pci=0

  buf[31] = 70;  // diffOnThrottle at base+2
  buf[32] = 45;  // diffOffThrottle at base+3

  TelemetryFrame out{};
  telemetry_parse(buf, BUFSZ, out);

  assert(out.status.diffOnThrottle  == 70);
  assert(out.status.diffOffThrottle == 45);
  std::cout << "PASS: f2024_car_setup\n";
}

// ─── F1 25 path ───────────────────────────────────────────────────────────────

static void test_f2025_car_setup() {
  // F1 25 uses CS=50 (same as F1 24). hdrSize=29, pciOff=27
  const size_t BUFSZ = 80;
  uint8_t buf[BUFSZ];
  memset(buf, 0, BUFSZ);
  writeU16(buf, 0, 2025);
  buf[6]  = F1Packets::PACKET_ID_CAR_SETUP;
  buf[27] = 0;

  buf[31] = 80;  // diffOnThrottle
  buf[32] = 35;  // diffOffThrottle

  TelemetryFrame out{};
  telemetry_parse(buf, BUFSZ, out);

  assert(out.status.diffOnThrottle  == 80);
  assert(out.status.diffOffThrottle == 35);
  std::cout << "PASS: f2025_car_setup\n";
}

static void test_f2025_car_damage() {
  // F1 25: CS=46, ewOff=38, gboxOff=36. hdrSize=29, base=29
  const size_t BUFSZ = 80;
  uint8_t buf[BUFSZ];
  memset(buf, 0, BUFSZ);
  writeU16(buf, 0, 2025);
  buf[6]  = F1Packets::PACKET_ID_CAR_DAMAGE;
  buf[27] = 0;

  // tyresWear as float[4] at base+0..+15, packet order RL,RR,FL,FR
  writeF32(buf, 29, 10.0f);  // RL
  writeF32(buf, 33, 12.0f);  // RR
  writeF32(buf, 37, 18.0f);  // FL
  writeF32(buf, 41, 22.0f);  // FR
  // engine wear at base+ewOff = 29+38 = 67
  buf[67] = 5;   // MGUHWear
  buf[68] = 10;  // ESWear
  buf[69] = 15;  // CEWear
  buf[70] = 20;  // ICEWear
  buf[71] = 25;  // MGUKWear
  buf[72] = 30;  // TCWear
  // gearbox at base+gboxOff = 29+36 = 65
  buf[65] = 8;

  TelemetryFrame out{};
  telemetry_parse(buf, BUFSZ, out);

  // stored FL,FR,RL,RR
  assert(out.damage.tyresWear[0] == 18);  // FL
  assert(out.damage.tyresWear[1] == 22);  // FR
  assert(out.damage.tyresWear[2] == 10);  // RL
  assert(out.damage.tyresWear[3] == 12);  // RR
  assert(out.damage.engineMGUHWear == 5);
  assert(out.damage.engineESWear   == 10);
  assert(out.damage.engineCEWear   == 15);
  assert(out.damage.engineICEWear  == 20);
  assert(out.damage.engineMGUKWear == 25);
  assert(out.damage.engineTCWear   == 30);
  assert(out.damage.gearBoxDamage  == 8);
  std::cout << "PASS: f2025_car_damage\n";
}

static void test_f2025_lap_data() {
  // F1 25 uses same lap data as F1 23+: CS=50, posOff=30, pitOff=32
  const size_t BUFSZ = 70;
  uint8_t buf[BUFSZ];
  memset(buf, 0, BUFSZ);
  writeU16(buf, 0, 2025);
  buf[6]  = F1Packets::PACKET_ID_LAP_DATA;
  buf[27] = 0;

  writeU32(buf, 29, 73456);  // 73.456s
  buf[59] = 1;               // P1
  buf[61] = 0;               // not in pit

  TelemetryFrame out{};
  telemetry_parse(buf, BUFSZ, out);

  assert(out.lap.lastLapTime > 73.45f && out.lap.lastLapTime < 73.46f);
  assert(out.lap.position == 1);
  assert(out.lap.pitStatus == 0);
  std::cout << "PASS: f2025_lap_data\n";
}

int main() {
  test_legacy_telemetry();

  test_f2023_session();
  test_f2023_car_telemetry();
  test_f2023_car_telemetry_mfd();
  test_f2023_car_status();
  test_f2023_lap_data();
  test_f2023_car_setup();
  test_f2023_car_damage();

  test_f2022_car_status();

  test_f2024_car_setup();

  test_f2025_car_setup();
  test_f2025_car_damage();
  test_f2025_lap_data();

  std::cout << "All telemetry_parser tests passed\n";
  return 0;
}
