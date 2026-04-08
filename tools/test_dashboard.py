#!/usr/bin/env python3
"""
Interactive F1 telemetry simulator for testing the SimRacing Dashboard.

Sends fake F1 2025 UDP telemetry packets so you can preview every dashboard
screen on the NodeMCU without the actual game running.

Two modes:
  MANUAL — You control every value with the keyboard.
  PLAY   — Auto-drives a realistic lap simulation while you switch screens.

Controls (both modes):
  ← / →    Cycle through screens (MAIN, SETUP, PITS, TYRES, TEMPS, ENGINE)
  SPACE    Toggle PLAY / MANUAL mode
  q        Quit

Manual-only controls:
  ↑ / ↓    Change RPM (±1000)
  g / G    Gear up / down
  t / T    Throttle up / down (±25%)
  b / B    Brake up / down (±25%)
  d        Toggle DRS
  p        Cycle pit status (track → pitting → in pit)
  c        Cycle tyre compound (Soft → Medium → Hard → Inter → Wet)
  r        Randomize all values

Usage:
  python3 tools/test_dashboard.py [ESP_IP] [PORT]
  Default: 192.168.1.255:20777 (broadcast)
"""

import socket
import struct
import sys
import math
import random
import time
import select
import tty
import termios

# ─── Config ───────────────────────────────────────────────────────────
DEFAULT_IP   = "192.168.1.255"
DEFAULT_PORT = 20777
FMT_YEAR     = 2025
HEADER_SIZE  = 29
PLAYER_INDEX = 0
NUM_CARS     = 22
SEND_HZ      = 20   # packets per second

# ─── Screen definitions ──────────────────────────────────────────────
# In race mode: mfd 0=SETUP,1=PITS,2=TYRES,3=TEMPS,4=ENGINE,255=MAIN
SCREENS = [
    ("MAIN",   255),
    ("SETUP",    0),
    ("PITS",     1),
    ("TYRES",    2),
    ("TEMPS",    3),
    ("ENGINE",   4),
]

COMPOUNDS = [
    ("Soft",  16),
    ("Medium", 17),
    ("Hard",  18),
    ("Inter",   7),
    ("Wet",     8),
]

ERS_MODES = ["NONE", "MEDIUM", "HOTLAP", "OVERTAKE"]

# ─── ANSI Colors ──────────────────────────────────────────────────────
class C:
    RST     = '\033[0m'
    BOLD    = '\033[1m'
    DIM     = '\033[2m'
    # Foreground
    RED     = '\033[91m'
    GREEN   = '\033[92m'
    YELLOW  = '\033[93m'
    BLUE    = '\033[94m'
    MAGENTA = '\033[95m'
    CYAN    = '\033[96m'
    WHITE   = '\033[97m'
    GRAY    = '\033[90m'
    # Background
    BG_RED    = '\033[41m'
    BG_GREEN  = '\033[42m'
    BG_YELLOW = '\033[43m'
    BG_BLUE   = '\033[44m'
    BG_CYAN   = '\033[46m'
    BG_WHITE  = '\033[47m'
    BG_GRAY   = '\033[100m'

# ─── Track simulation for PLAY mode ──────────────────────────────────
# A simplified lap: sequence of (duration_secs, target_speed, braking, drs_zone)
# Simulates straights, braking zones, corners, accelerations
TRACK_SEGMENTS = [
    # (duration, target_speed_kmh, is_braking_zone, drs_possible)
    (4.0,  320, False, True),   # Main straight (DRS)
    (1.5,   80, True,  False),  # Braking into T1
    (2.0,  120, False, False),  # T1-T2 slow section
    (1.0,  160, False, False),  # Short accel
    (1.2,   70, True,  False),  # Braking T3
    (2.5,  140, False, False),  # Medium speed corners
    (3.0,  290, False, True),   # Back straight (DRS)
    (1.5,   90, True,  False),  # Braking into chicane
    (2.0,  110, False, False),  # Chicane
    (1.0,  180, False, False),  # Accel out
    (1.5,  260, False, False),  # Fast kink
    (1.2,  100, True,  False),  # Braking last corner
    (2.0,  150, False, False),  # Last corner exit
]


class PlaySimulator:
    """Drives realistic auto-evolving telemetry values."""

    def __init__(self):
        self.lap_time = 0.0  # seconds into current lap
        self.seg_idx = 0
        self.seg_time = 0.0
        self.lap_count = 0
        self.total_lap_duration = sum(s[0] for s in TRACK_SEGMENTS)
        # Smooth current values
        self._speed = 250.0
        self._rpm = 10000.0
        self._throttle = 0.5
        self._brake = 0.0
        self._gear = 6.0

    def tick(self, s: 'SimState', dt: float):
        """Advance simulation by dt seconds and update SimState."""
        self.seg_time += dt
        self.lap_time += dt

        seg = TRACK_SEGMENTS[self.seg_idx]
        seg_dur, target_speed, is_braking, drs_zone = seg

        # Advance segment
        if self.seg_time >= seg_dur:
            self.seg_time -= seg_dur
            self.seg_idx = (self.seg_idx + 1) % len(TRACK_SEGMENTS)
            if self.seg_idx == 0:
                # Completed a lap
                s.last_lap_ms = int(self.lap_time * 1000) + random.randint(-500, 500)
                self.lap_time = 0.0
                self.lap_count += 1
                s.tyre_age = min(s.tyre_age + 1, 50)
                # Fuel consumption
                s.fuel = max(0.0, s.fuel - random.uniform(1.4, 1.8))
                s.fuel_laps = max(0.0, s.fuel_laps - 1.0)

        # Smooth speed towards target
        speed_diff = target_speed - self._speed
        accel_rate = 120.0 if speed_diff > 0 else 250.0  # brake faster than accel
        self._speed += clamp(speed_diff, -accel_rate * dt, accel_rate * dt)
        self._speed += random.uniform(-2, 2)  # jitter
        s.speed = int(clamp(self._speed, 0, 350))

        # Throttle & brake from speed change
        if is_braking:
            target_thr = 0.0
            target_brk = clamp(0.3 + abs(speed_diff) / 400.0, 0.0, 1.0)
        elif speed_diff > 20:
            target_thr = clamp(0.7 + speed_diff / 300.0, 0.0, 1.0)
            target_brk = 0.0
        else:
            target_thr = clamp(0.4 + speed_diff / 200.0, 0.1, 1.0)
            target_brk = 0.0

        self._throttle += clamp(target_thr - self._throttle, -3 * dt, 3 * dt)
        self._brake += clamp(target_brk - self._brake, -4 * dt, 4 * dt)
        s.throttle = clamp(self._throttle, 0.0, 1.0)
        s.brake = clamp(self._brake, 0.0, 1.0)

        # Gear from speed
        gear_table = [0, 60, 100, 145, 190, 240, 285, 330]
        target_gear = 1
        for g in range(1, 8):
            if s.speed >= gear_table[g]:
                target_gear = g + 1
            else:
                break
        target_gear = min(target_gear, 8)
        if is_braking and self._speed < target_speed + 20:
            target_gear = max(target_gear - 1, 1)
        s.gear = target_gear

        # RPM from speed within gear range
        gear_min_rpm = [4000, 5000, 6000, 6500, 7000, 8000, 8500, 9000]
        gear_max_rpm = [12000, 12500, 12800, 13000, 13200, 13500, 14000, 15000]
        gi = clamp(s.gear - 1, 0, 7)
        rpm_range_frac = clamp((s.speed - gear_table[gi]) / max(1, gear_table[min(gi+1, 7)] - gear_table[gi]), 0, 1)
        target_rpm = gear_min_rpm[gi] + rpm_range_frac * (gear_max_rpm[gi] - gear_min_rpm[gi])
        self._rpm += clamp(target_rpm - self._rpm, -4000 * dt, 4000 * dt)
        s.rpm = int(clamp(self._rpm + random.uniform(-100, 100), 3000, 15000))

        # DRS
        s.drs = drs_zone and s.speed > 200

        # ERS: depletes on straights, recovers on braking
        if is_braking:
            s.ers_energy_j = min(4_000_000, s.ers_energy_j + 180_000 * dt)
        elif drs_zone:
            s.ers_energy_j = max(0, s.ers_energy_j - 250_000 * dt)
        else:
            s.ers_energy_j = min(4_000_000, s.ers_energy_j + 30_000 * dt)

        # Temperatures respond to braking/speed
        for i in range(4):
            if is_braking:
                s.brakes_temp[i] = int(clamp(s.brakes_temp[i] + random.uniform(8, 15) * dt * 20, 200, 1100))
                s.tyre_surface[i] = int(clamp(s.tyre_surface[i] + random.uniform(0.5, 1.5) * dt * 10, 60, 140))
            else:
                s.brakes_temp[i] = int(clamp(s.brakes_temp[i] - random.uniform(3, 8) * dt * 10, 200, 1100))
                s.tyre_surface[i] = int(clamp(s.tyre_surface[i] - random.uniform(0.1, 0.3) * dt * 5, 60, 140))
            s.tyre_inner[i] = int(clamp(s.tyre_surface[i] + random.randint(3, 8), 60, 150))

        s.engine_temp = int(clamp(s.engine_temp + random.uniform(-0.5, 0.5), 95, 120))

        # Tyre wear slowly increases
        for i in range(4):
            s.tyre_wear[i] = clamp(s.tyre_wear[i] + random.uniform(0.001, 0.005), 0, 100)

# ─── Simulated state ─────────────────────────────────────────────────
class SimState:
    def __init__(self):
        self.screen_idx = 0
        self.speed = 280
        self.rpm = 11000
        self.gear = 7
        self.throttle = 0.8
        self.brake = 0.0
        self.drs = False
        self.position = 3
        self.last_lap_ms = 78432  # 1:18.432
        self.pit_status = 0
        self.fuel = 42.5
        self.fuel_laps = 12.3
        self.brake_bias = 56
        self.ers_energy_j = 2_800_000.0  # joules (max 4MJ)
        self.ers_mode = 1  # MEDIUM
        self.diff_on = 65
        self.diff_off = 50
        self.compound_idx = 0
        self.tyre_age = 14
        self.session_type = 10  # Race
        # Tyre wear FL,FR,RL,RR
        self.tyre_wear = [32.0, 35.0, 28.0, 30.0]
        # Temps
        self.brakes_temp = [650, 680, 620, 640]    # FL,FR,RL,RR °C
        self.tyre_surface = [95, 97, 90, 92]       # °C
        self.tyre_inner   = [100, 102, 96, 98]     # °C
        self.engine_temp  = 105                     # °C
        # Engine wear
        self.engine_wear = {
            'mguh': 15, 'es': 20, 'ce': 10,
            'ice': 25, 'mguk': 18, 'tc': 12,
            'gearbox': 8
        }
        self.frame = 0

    @property
    def mfd_panel(self):
        return SCREENS[self.screen_idx][1]

    @property
    def screen_name(self):
        return SCREENS[self.screen_idx][0]

    @property
    def compound_value(self):
        return COMPOUNDS[self.compound_idx][1]

    @property
    def compound_name(self):
        return COMPOUNDS[self.compound_idx][0]

    def randomize(self):
        self.speed = random.randint(60, 340)
        self.rpm = random.randint(3000, 15000)
        self.gear = random.randint(1, 8)
        self.throttle = random.random()
        self.brake = random.random() * 0.3
        self.drs = random.choice([True, False])
        self.position = random.randint(1, 20)
        self.last_lap_ms = random.randint(70000, 110000)
        self.pit_status = random.randint(0, 2)
        self.fuel = random.uniform(5.0, 80.0)
        self.fuel_laps = random.uniform(1.0, 30.0)
        self.brake_bias = random.randint(50, 70)
        self.ers_energy_j = random.uniform(0, 4_000_000)
        self.ers_mode = random.randint(0, 3)
        self.diff_on = random.randint(50, 100)
        self.diff_off = random.randint(50, 100)
        self.compound_idx = random.randint(0, len(COMPOUNDS) - 1)
        self.tyre_age = random.randint(0, 40)
        self.tyre_wear = [random.uniform(0, 80) for _ in range(4)]
        self.brakes_temp = [random.randint(200, 1000) for _ in range(4)]
        self.tyre_surface = [random.randint(60, 130) for _ in range(4)]
        self.tyre_inner   = [random.randint(70, 140) for _ in range(4)]
        self.engine_temp  = random.randint(80, 130)
        for k in self.engine_wear:
            self.engine_wear[k] = random.randint(0, 80)


# ─── Packet builders ─────────────────────────────────────────────────

def make_header(packet_id: int, frame: int) -> bytes:
    """Build a 29-byte F1 2023+ header."""
    hdr = bytearray(HEADER_SIZE)
    struct.pack_into('<H', hdr, 0, FMT_YEAR)       # format
    hdr[6] = packet_id                               # packetId
    struct.pack_into('<I', hdr, 19, frame)           # frameIdentifier
    hdr[27] = PLAYER_INDEX                            # playerCarIndex
    return bytes(hdr)


def make_legacy_telemetry(s: SimState) -> bytes:
    """Tiny 39-byte legacy-format packet carrying mfdPanelIndex + basic telemetry.

    The firmware's parseLegacy() triggers when format year < 2022.
    Layout: byte 5 = packetId, base = 24.
    This packet is small enough to be delivered reliably over ESP8266 WiFi."""
    pkt = bytearray(39)
    # format = 0 → triggers legacy parser (fmt < 2022)
    struct.pack_into('<H', pkt, 0, 0)
    pkt[5] = 6  # PACKET_ID_CAR_TELEMETRY
    struct.pack_into('<I', pkt, 18, s.frame)  # frameIdentifier
    base = 24
    struct.pack_into('<H', pkt, base + 0, s.speed)               # speed
    struct.pack_into('<H', pkt, base + 2, s.rpm)                 # rpm
    pkt[base + 8] = int(clamp(s.throttle * 255, 0, 255))        # throttle (uint8)
    pkt[base + 9] = int(clamp(s.brake * 255, 0, 255))           # brake (uint8)
    pkt[base + 12] = s.gear & 0xFF                               # gear
    pkt[base + 13] = 1 if s.drs else 0                           # drs
    pkt[base + 14] = s.mfd_panel                                 # mfdPanelIndex
    return bytes(pkt)


def make_legacy_status(s: SimState) -> bytes:
    """Tiny legacy-format packet for car status (ERS, fuel, brake bias)."""
    pkt = bytearray(33)  # 24 header + 9 payload
    struct.pack_into('<H', pkt, 0, 0)  # legacy format
    pkt[5] = 7  # PACKET_ID_CAR_STATUS
    struct.pack_into('<I', pkt, 18, s.frame)
    base = 24
    struct.pack_into('<H', pkt, base + 0, int(s.ers_energy_j / 4_000_000 * 1000))
    pkt[base + 2] = s.ers_mode
    # pad byte 3 (alignment)
    struct.pack_into('<f', pkt, base + 4, s.fuel)
    pkt[base + 8] = s.brake_bias
    return bytes(pkt)


def make_legacy_lap(s: SimState) -> bytes:
    """Tiny legacy-format packet for lap data (position, pit status)."""
    pkt = bytearray(30)  # 24 header + 6 payload
    struct.pack_into('<H', pkt, 0, 0)  # legacy format
    pkt[5] = 2  # PACKET_ID_LAP_DATA
    struct.pack_into('<I', pkt, 18, s.frame)
    base = 24
    struct.pack_into('<f', pkt, base + 0, s.last_lap_ms / 1000.0)  # lastLapTime as float seconds
    pkt[base + 4] = s.position
    pkt[base + 5] = s.pit_status
    return bytes(pkt)


def make_session_packet(s: SimState) -> bytes:
    """Packet ID 1 - Session data."""
    hdr = make_header(1, s.frame)
    # sessionType at hdrSize+6 = offset 35 from packet start
    # We need at least 7 bytes after header
    payload = bytearray(32)
    payload[6] = s.session_type
    return hdr + payload


def make_telemetry_packet(s: SimState) -> bytes:
    """Packet ID 6 - Car Telemetry (60 bytes × 22 cars + 1 byte mfd)."""
    hdr = make_header(6, s.frame)
    cars = bytearray(60 * NUM_CARS)
    base = PLAYER_INDEX * 60

    struct.pack_into('<H', cars, base + 0, s.speed)            # speed
    struct.pack_into('<f', cars, base + 2, s.throttle)         # throttle
    struct.pack_into('<f', cars, base + 6, 0.0)                # steer
    struct.pack_into('<f', cars, base + 10, s.brake)           # brake
    cars[base + 14] = 0                                         # clutch
    cars[base + 15] = s.gear & 0xFF                            # gear (int8)
    struct.pack_into('<H', cars, base + 16, s.rpm)             # engineRPM
    cars[base + 18] = 1 if s.drs else 0                        # drs

    # Brakes temp: packet order RL,RR,FL,FR (parser reorders to FL,FR,RL,RR)
    struct.pack_into('<H', cars, base + 22, s.brakes_temp[2])  # RL
    struct.pack_into('<H', cars, base + 24, s.brakes_temp[3])  # RR
    struct.pack_into('<H', cars, base + 26, s.brakes_temp[0])  # FL
    struct.pack_into('<H', cars, base + 28, s.brakes_temp[1])  # FR

    # Tyre surface temp: RL,RR,FL,FR
    cars[base + 30] = s.tyre_surface[2]  # RL
    cars[base + 31] = s.tyre_surface[3]  # RR
    cars[base + 32] = s.tyre_surface[0]  # FL
    cars[base + 33] = s.tyre_surface[1]  # FR

    # Tyre inner temp: RL,RR,FL,FR
    cars[base + 34] = s.tyre_inner[2]    # RL
    cars[base + 35] = s.tyre_inner[3]    # RR
    cars[base + 36] = s.tyre_inner[0]    # FL
    cars[base + 37] = s.tyre_inner[1]    # FR

    # Engine temp
    struct.pack_into('<H', cars, base + 38, s.engine_temp)

    # mfdPanelIndex after all 22 cars
    mfd = bytearray(1)
    mfd[0] = s.mfd_panel
    return hdr + cars + mfd


def make_status_packet(s: SimState) -> bytes:
    """Packet ID 7 - Car Status (55 bytes × 22 cars for F1 2023+)."""
    hdr = make_header(7, s.frame)
    CS = 55
    cars = bytearray(CS * NUM_CARS)
    base = PLAYER_INDEX * CS

    cars[base + 3] = s.brake_bias
    struct.pack_into('<f', cars, base + 5, s.fuel)
    struct.pack_into('<f', cars, base + 13, s.fuel_laps)
    cars[base + 25] = s.compound_value      # actualTyreCompound
    cars[base + 26] = s.compound_value      # visualTyreCompound
    cars[base + 27] = s.tyre_age

    # ERS at offset 37 (F1 2023+)
    struct.pack_into('<f', cars, base + 37, s.ers_energy_j)
    cars[base + 41] = s.ers_mode

    return hdr + cars


def make_lap_packet(s: SimState) -> bytes:
    """Packet ID 2 - Lap Data (50 bytes × 22 cars for F1 2023+)."""
    hdr = make_header(2, s.frame)
    CS = 50
    cars = bytearray(CS * NUM_CARS)
    base = PLAYER_INDEX * CS

    struct.pack_into('<I', cars, base + 0, s.last_lap_ms)
    cars[base + 30] = s.position
    cars[base + 32] = s.pit_status

    return hdr + cars


def make_setup_packet(s: SimState) -> bytes:
    """Packet ID 5 - Car Setup (50 bytes × 22 cars for F1 2024+)."""
    hdr = make_header(5, s.frame)
    CS = 50
    cars = bytearray(CS * NUM_CARS)
    base = PLAYER_INDEX * CS

    cars[base + 2] = s.diff_on
    cars[base + 3] = s.diff_off

    return hdr + cars


def make_damage_packet(s: SimState) -> bytes:
    """Packet ID 10 - Car Damage (46 bytes × 22 cars for F1 2025)."""
    hdr = make_header(10, s.frame)
    CS = 46
    cars = bytearray(CS * NUM_CARS)
    base = PLAYER_INDEX * CS

    # Tyre wear floats: packet order RL,RR,FL,FR
    struct.pack_into('<f', cars, base + 0,  s.tyre_wear[2])   # RL
    struct.pack_into('<f', cars, base + 4,  s.tyre_wear[3])   # RR
    struct.pack_into('<f', cars, base + 8,  s.tyre_wear[0])   # FL
    struct.pack_into('<f', cars, base + 12, s.tyre_wear[1])   # FR

    # Gearbox at +36, engine wear at +38
    cars[base + 36] = s.engine_wear['gearbox']
    cars[base + 38] = s.engine_wear['mguh']
    cars[base + 39] = s.engine_wear['es']
    cars[base + 40] = s.engine_wear['ce']
    cars[base + 41] = s.engine_wear['ice']
    cars[base + 42] = s.engine_wear['mguk']
    cars[base + 43] = s.engine_wear['tc']

    return hdr + cars


# ─── Terminal raw input (non-blocking) ───────────────────────────────

def get_key_nonblocking():
    """Read a single keypress if available, None otherwise."""
    if select.select([sys.stdin], [], [], 0)[0]:
        ch = sys.stdin.read(1)
        if ch == '\x1b':
            # Arrow key escape sequence
            if select.select([sys.stdin], [], [], 0.05)[0]:
                ch2 = sys.stdin.read(1)
                if ch2 == '[':
                    ch3 = sys.stdin.read(1)
                    return {
                        'A': 'UP', 'B': 'DOWN',
                        'C': 'RIGHT', 'D': 'LEFT'
                    }.get(ch3, None)
            return 'ESC'
        return ch
    return None


# ─── Display ──────────────────────────────────────────────────────────

def clamp(v, lo, hi):
    return max(lo, min(hi, v))

def format_lap(ms):
    mins = ms // 60000
    secs = (ms % 60000) / 1000
    return f"{mins}:{secs:06.3f}"

PIT_LABELS = ["TRACK", "PITTING", "IN PIT"]
PIT_COLORS = [C.GREEN, C.YELLOW, C.RED]

COMPOUND_COLORS = {
    16: C.RED,      # Soft
    17: C.YELLOW,   # Medium
    18: C.WHITE,    # Hard
    7:  C.GREEN,    # Inter
    8:  C.BLUE,     # Wet
}

def color_bar(value, max_val, width=20, low_color=C.GREEN, mid_color=C.YELLOW, hi_color=C.RED):
    """Render a colored bar ████░░░░."""
    frac = clamp(value / max_val, 0, 1)
    filled = int(frac * width)
    empty = width - filled
    if frac < 0.4:
        color = low_color
    elif frac < 0.7:
        color = mid_color
    else:
        color = hi_color
    return f"{color}{'█' * filled}{C.GRAY}{'░' * empty}{C.RST}"

def color_wear_bar(value, width=10):
    """Wear bar: low=green, high=red (inverted logic: high wear is bad)."""
    return color_bar(value, 100, width, C.GREEN, C.YELLOW, C.RED)

def rpm_bar(rpm, max_rpm=15000, width=30):
    """RPM bar with F1-style color zones."""
    frac = clamp(rpm / max_rpm, 0, 1)
    filled = int(frac * width)
    segments = []
    for i in range(width):
        f = i / width
        if i < filled:
            if f < 0.6:
                segments.append(f"{C.GREEN}█")
            elif f < 0.8:
                segments.append(f"{C.YELLOW}█")
            elif f < 0.93:
                segments.append(f"{C.RED}█")
            else:
                segments.append(f"{C.MAGENTA}█")
        else:
            segments.append(f"{C.GRAY}░")
    return ''.join(segments) + C.RST

def temp_color(temp, cold=60, warm=100, hot=130):
    if temp < cold:
        return C.BLUE
    elif temp < warm:
        return C.GREEN
    elif temp < hot:
        return C.YELLOW
    return C.RED

def brake_temp_color(temp):
    if temp < 300:
        return C.BLUE
    elif temp < 600:
        return C.GREEN
    elif temp < 900:
        return C.YELLOW
    return C.RED

def print_status(s: SimState, play_mode: bool, play_sim: 'PlaySimulator'):
    """Print current state with ANSI colors."""
    sys.stdout.write('\033[2J\033[H')  # clear screen
    ers_pct = s.ers_energy_j / 4_000_000 * 100
    W = 62  # box width

    mode_str = f"{C.BG_GREEN}{C.BOLD} ▶ PLAY {C.RST}" if play_mode else f"{C.BG_BLUE}{C.BOLD} ■ MANUAL {C.RST}"
    screen_color = C.CYAN if s.screen_name == "MAIN" else C.YELLOW

    # Header
    print(f"{C.CYAN}{C.BOLD}╔{'═' * W}╗{C.RST}")
    print(f"{C.CYAN}║{C.RST}  {C.BOLD}{C.WHITE}SIMRACING DASH — Interactive Tester{C.RST}     {mode_str}    {C.CYAN}║{C.RST}")
    print(f"{C.CYAN}{C.BOLD}╠{'═' * W}╣{C.RST}")

    # Screen selector tabs
    tabs = ""
    for i, (name, _) in enumerate(SCREENS):
        if i == s.screen_idx:
            tabs += f" {C.BG_CYAN}{C.BOLD}{C.WHITE} {name} {C.RST}"
        else:
            tabs += f" {C.GRAY} {name} {C.RST}"
    print(f"{C.CYAN}║{C.RST} {tabs}        {C.CYAN}║{C.RST}")
    print(f"{C.CYAN}║{C.RST}  {C.DIM}← → cambiar pantalla    SPACE toggle modo{C.RST}        {C.CYAN}║{C.RST}")
    print(f"{C.CYAN}{C.BOLD}╠{'═' * W}╣{C.RST}")

    # Gear big display
    gear_str = 'R' if s.gear == -1 else ('N' if s.gear == 0 else str(s.gear))
    gear_color = C.RED if s.gear == -1 else (C.GRAY if s.gear == 0 else C.WHITE)
    drs_str = f"{C.GREEN}{C.BOLD}DRS{C.RST}" if s.drs else f"{C.GRAY}DRS{C.RST}"

    print(f"{C.CYAN}║{C.RST}  {C.BOLD}{C.WHITE}{s.speed:>3}{C.RST} {C.DIM}km/h{C.RST}    {gear_color}{C.BOLD}G{gear_str}{C.RST}    {C.WHITE}{s.rpm:>5}{C.RST} {C.DIM}rpm{C.RST}    {drs_str}    P{C.BOLD}{s.position}{C.RST}    {C.CYAN}║{C.RST}")

    # RPM bar
    print(f"{C.CYAN}║{C.RST}  RPM {rpm_bar(s.rpm)}              {C.CYAN}║{C.RST}")

    # Throttle & Brake bars
    thr_bar = color_bar(s.throttle, 1.0, 20, C.GREEN, C.GREEN, C.GREEN)
    brk_bar = color_bar(s.brake, 1.0, 20, C.RED, C.RED, C.RED)
    print(f"{C.CYAN}║{C.RST}  {C.GREEN}THR{C.RST} {thr_bar} {s.throttle*100:>5.1f}%              {C.CYAN}║{C.RST}")
    print(f"{C.CYAN}║{C.RST}  {C.RED}BRK{C.RST} {brk_bar} {s.brake*100:>5.1f}%              {C.CYAN}║{C.RST}")

    # Lap info
    pit_c = PIT_COLORS[s.pit_status]
    print(f"{C.CYAN}║{C.RST}  {C.DIM}Lap:{C.RST} {C.WHITE}{format_lap(s.last_lap_ms)}{C.RST}   {pit_c}{C.BOLD}{PIT_LABELS[s.pit_status]}{C.RST}                           {C.CYAN}║{C.RST}")
    print(f"{C.CYAN}{C.BOLD}╠{'═' * W}╣{C.RST}")

    # Fuel & ERS
    fuel_color = C.RED if s.fuel < 10 else (C.YELLOW if s.fuel < 20 else C.GREEN)
    ers_bar = color_bar(ers_pct, 100, 15, C.YELLOW, C.YELLOW, C.GREEN)
    ers_mode_colors = [C.GRAY, C.GREEN, C.YELLOW, C.RED]
    ers_mc = ers_mode_colors[s.ers_mode]
    print(f"{C.CYAN}║{C.RST}  {C.DIM}Fuel:{C.RST} {fuel_color}{s.fuel:.1f}L{C.RST} ({s.fuel_laps:.1f} laps)   {C.DIM}Bias:{C.RST} {C.WHITE}{s.brake_bias}%{C.RST}                 {C.CYAN}║{C.RST}")
    print(f"{C.CYAN}║{C.RST}  {C.DIM}ERS:{C.RST}  {ers_bar} {ers_pct:>3.0f}%  {ers_mc}{C.BOLD}{ERS_MODES[s.ers_mode]}{C.RST}              {C.CYAN}║{C.RST}")

    # Diff
    print(f"{C.CYAN}║{C.RST}  {C.DIM}Diff:{C.RST} ON={C.WHITE}{s.diff_on}%{C.RST}  OFF={C.WHITE}{s.diff_off}%{C.RST}                              {C.CYAN}║{C.RST}")
    print(f"{C.CYAN}{C.BOLD}╠{'═' * W}╣{C.RST}")

    # Tyres
    cc = COMPOUND_COLORS.get(s.compound_value, C.WHITE)
    print(f"{C.CYAN}║{C.RST}  {C.DIM}Tyres:{C.RST} {cc}{C.BOLD}{s.compound_name}{C.RST}  {C.DIM}Age:{C.RST} {C.WHITE}{s.tyre_age}{C.RST} laps                            {C.CYAN}║{C.RST}")
    labels = ['FL', 'FR', 'RL', 'RR']
    wear_line = "  "
    for i in range(4):
        w = s.tyre_wear[i]
        wc = C.GREEN if w < 30 else (C.YELLOW if w < 60 else C.RED)
        wear_line += f"{C.DIM}{labels[i]}:{C.RST}{wc}{w:>4.0f}%{C.RST}  "
    print(f"{C.CYAN}║{C.RST}{wear_line}                      {C.CYAN}║{C.RST}")
    print(f"{C.CYAN}{C.BOLD}╠{'═' * W}╣{C.RST}")

    # Temps compact
    ts_line = "  "
    for i in range(4):
        tc = temp_color(s.tyre_surface[i])
        ts_line += f"{C.DIM}{labels[i]}:{C.RST}{tc}{s.tyre_surface[i]:>3}°{C.RST}  "
    print(f"{C.CYAN}║{C.RST}  {C.DIM}Tyre Surf:{C.RST}{ts_line}             {C.CYAN}║{C.RST}")

    bt_line = "  "
    for i in range(4):
        bc = brake_temp_color(s.brakes_temp[i])
        bt_line += f"{C.DIM}{labels[i]}:{C.RST}{bc}{s.brakes_temp[i]:>4}°{C.RST} "
    ec = temp_color(s.engine_temp, 80, 105, 115)
    print(f"{C.CYAN}║{C.RST}  {C.DIM}Brakes:   {C.RST}{bt_line}            {C.CYAN}║{C.RST}")
    print(f"{C.CYAN}║{C.RST}  {C.DIM}Engine:{C.RST} {ec}{s.engine_temp}°C{C.RST}                                           {C.CYAN}║{C.RST}")
    print(f"{C.CYAN}{C.BOLD}╠{'═' * W}╣{C.RST}")

    # Engine wear
    w = s.engine_wear
    parts = [('MGU-H', w['mguh']), ('ES', w['es']), ('CE', w['ce']),
             ('ICE', w['ice']), ('MGU-K', w['mguk']), ('TC', w['tc']), ('GBOX', w['gearbox'])]
    ew_line = ""
    for name, val in parts:
        wc = C.GREEN if val < 30 else (C.YELLOW if val < 60 else C.RED)
        ew_line += f" {C.DIM}{name}:{C.RST}{wc}{val:>2}%{C.RST}"
    print(f"{C.CYAN}║{C.RST}{ew_line}  {C.CYAN}║{C.RST}")
    print(f"{C.CYAN}{C.BOLD}╠{'═' * W}╣{C.RST}")

    # Help
    if play_mode:
        seg = TRACK_SEGMENTS[play_sim.seg_idx]
        seg_name = "BRAKING" if seg[2] else ("DRS STRAIGHT" if seg[3] else "CORNER/ACCEL")
        seg_c = C.RED if seg[2] else (C.GREEN if seg[3] else C.YELLOW)
        print(f"{C.CYAN}║{C.RST}  {C.BG_GREEN}{C.BOLD} PLAY {C.RST} {seg_c}{C.BOLD}{seg_name}{C.RST}  Lap #{play_sim.lap_count + 1}                        {C.CYAN}║{C.RST}")
        print(f"{C.CYAN}║{C.RST}  {C.DIM}1-6 pantalla   SPACE→manual   q salir{C.RST}               {C.CYAN}║{C.RST}")
    else:
        print(f"{C.CYAN}║{C.RST}  {C.BG_BLUE}{C.BOLD} MANUAL {C.RST}  {C.DIM}↑↓ RPM  g/G gear  t/T thr  b/B brk{C.RST}      {C.CYAN}║{C.RST}")
        print(f"{C.CYAN}║{C.RST}  {C.DIM}d DRS  p pit  c compound  r random  SPACE→play{C.RST}      {C.CYAN}║{C.RST}")
    print(f"{C.CYAN}║{C.RST}  {C.BOLD}1{C.RST}{C.DIM}=MAIN {C.RST}{C.BOLD}2{C.RST}{C.DIM}=SETUP {C.RST}{C.BOLD}3{C.RST}{C.DIM}=PITS {C.RST}{C.BOLD}4{C.RST}{C.DIM}=TYRES {C.RST}{C.BOLD}5{C.RST}{C.DIM}=TEMPS {C.RST}{C.BOLD}6{C.RST}{C.DIM}=ENGINE{C.RST}    {C.CYAN}║{C.RST}")
    print(f"{C.CYAN}{C.BOLD}╚{'═' * W}╝{C.RST}")
    sys.stdout.flush()


# ─── Main ─────────────────────────────────────────────────────────────

def main():
    ip   = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_IP
    port = int(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_PORT

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

    state = SimState()
    play_mode = False
    play_sim = PlaySimulator()
    interval = 1.0 / SEND_HZ

    # Save terminal settings and switch to raw mode
    old_settings = termios.tcgetattr(sys.stdin)
    try:
        tty.setcbreak(sys.stdin.fileno())
        print_status(state, play_mode, play_sim)

        last_send = 0
        last_tick = time.monotonic()
        redraw_counter = 0
        while True:
            now = time.monotonic()
            dt = now - last_tick
            last_tick = now

            # Handle input (works in both modes)
            key = get_key_nonblocking()
            redraw = False
            if key == 'q':
                break
            elif key == ' ':
                play_mode = not play_mode
                if play_mode:
                    play_sim = PlaySimulator()
                redraw = True
            # Screen selection: 1-6
            elif key in ('1','2','3','4','5','6'):
                state.screen_idx = int(key) - 1
                redraw = True

            # Manual-only controls
            if not play_mode:
                if key == 'UP':
                    state.rpm = clamp(state.rpm + 1000, 0, 15000)
                    redraw = True
                elif key == 'DOWN':
                    state.rpm = clamp(state.rpm - 1000, 0, 15000)
                    redraw = True
                elif key == 'g':
                    state.gear = clamp(state.gear + 1, -1, 8)
                    redraw = True
                elif key == 'G':
                    state.gear = clamp(state.gear - 1, -1, 8)
                    redraw = True
                elif key == 't':
                    state.throttle = clamp(state.throttle + 0.25, 0.0, 1.0)
                    redraw = True
                elif key == 'T':
                    state.throttle = clamp(state.throttle - 0.25, 0.0, 1.0)
                    redraw = True
                elif key == 'b':
                    state.brake = clamp(state.brake + 0.25, 0.0, 1.0)
                    redraw = True
                elif key == 'B':
                    state.brake = clamp(state.brake - 0.25, 0.0, 1.0)
                    redraw = True
                elif key == 'd':
                    state.drs = not state.drs
                    redraw = True
                elif key == 'p':
                    state.pit_status = (state.pit_status + 1) % 3
                    redraw = True
                elif key == 'c':
                    state.compound_idx = (state.compound_idx + 1) % len(COMPOUNDS)
                    redraw = True
                elif key == 'r':
                    state.randomize()
                    redraw = True

            # Play mode auto-simulation
            if play_mode:
                play_sim.tick(state, dt)
                redraw_counter += 1
                # Redraw at ~10 FPS to avoid terminal flicker (every 5 ticks at 50ms sleep)
                if redraw_counter >= 5:
                    redraw_counter = 0
                    redraw = True

            if redraw:
                print_status(state, play_mode, play_sim)

            # Send packets at SEND_HZ
            # Three tiny legacy packets (~100 bytes total) every cycle carry
            # mfdPanelIndex, telemetry basics, status, and lap data.
            # Heavy F1-format packets rotate one per cycle for data the
            # legacy format can't carry (temps, wear, session type, diff).
            # NOTE: heavy telemetry (packetId 6) is excluded from rotation
            # because it also writes mfdPanelIndex and can conflict.
            if now - last_send >= interval:
                last_send = now
                state.frame += 1

                # Always: legacy packets (reliable, tiny)
                sock.sendto(make_legacy_telemetry(state), (ip, port))
                sock.sendto(make_legacy_status(state), (ip, port))
                sock.sendto(make_legacy_lap(state), (ip, port))

                # Rotate one heavy F1-format packet per cycle (~4 Hz each)
                # NO heavy telemetry here — it would overwrite mfdPanelIndex
                heavy = [
                    make_session_packet,    # session type (race detection)
                    make_telemetry_packet,  # temps (brakes, tyres, engine)
                    make_status_packet,     # compound, tyre age, diff
                    make_setup_packet,      # diff on/off
                    make_damage_packet,     # tyre wear, engine wear
                ]
                pkt_fn = heavy[state.frame % len(heavy)]
                sock.sendto(pkt_fn(state), (ip, port))

            # Small sleep to avoid busy-wait
            time.sleep(0.005)

    finally:
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old_settings)
        sock.close()
        print(f"\n{C.CYAN}Bye!{C.RST}")


if __name__ == '__main__':
    main()
