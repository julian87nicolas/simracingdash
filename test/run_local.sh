#!/usr/bin/env bash
set -e
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

echo "Run tests (prefer PlatformIO native, fallback to g++)"

if [ "$SKIP_PLATFORMIO" = "1" ] || [ "$1" = "--no-pio" ]; then
  echo "SKIP_PLATFORMIO set; skipping PlatformIO and using g++ fallback"
else
  if command -v platformio >/dev/null 2>&1 || command -v pio >/dev/null 2>&1; then
    PIO=$(command -v platformio || command -v pio)
    echo "Found PlatformIO at: $PIO"
    echo "Ensuring native platform is installed..."
    $PIO platform install native --no-check || $PIO platform install native || true
    echo "Listing installed platforms:"
    $PIO platform list || true
    echo "Running PlatformIO tests (native)..."
    $PIO test -e native -v
    exit 0
  fi
fi

echo "PlatformIO not found. Falling back to g++ host tests..."
BIN_DIR="$ROOT_DIR/test/host/bin"
mkdir -p "$BIN_DIR"

echo "Compiling telemetry parser test..."
g++ -std=c++17 -I$ROOT_DIR/test/host/include -I$ROOT_DIR/include -I$ROOT_DIR/src -I$ROOT_DIR/test/stubs/ArduinoStub/src -I$ROOT_DIR/lib/TFT_eSPI/src -I$ROOT_DIR/test/stubs/ESP8266Stub/src \
  $ROOT_DIR/src/telemetry_parser.cpp $ROOT_DIR/test/host/test_telemetry_parser.cpp $ROOT_DIR/test/host/serial_stub.cpp -o $BIN_DIR/test_telemetry_parser

echo "Compiling state manager test..."
g++ -std=c++17 -I$ROOT_DIR/test/host/include -I$ROOT_DIR/include -I$ROOT_DIR/src -I$ROOT_DIR/test/stubs/ArduinoStub/src -I$ROOT_DIR/lib/TFT_eSPI/src -I$ROOT_DIR/test/stubs/ESP8266Stub/src \
  $ROOT_DIR/src/state.cpp $ROOT_DIR/test/host/test_state_manager.cpp $ROOT_DIR/test/host/serial_stub.cpp -o $BIN_DIR/test_state_manager

echo "Compiling dashboard render test..."
g++ -std=c++17 -I$ROOT_DIR/test/host/include -I$ROOT_DIR/include -I$ROOT_DIR/src -I$ROOT_DIR/test/stubs/ArduinoStub/src -I$ROOT_DIR/lib/TFT_eSPI/src -I$ROOT_DIR/test/stubs/ESP8266Stub/src \
  $ROOT_DIR/src/dashboards/dash_main.cpp $ROOT_DIR/test/host/test_dashboard.cpp $ROOT_DIR/test/host/serial_stub.cpp -o $BIN_DIR/test_dashboard

echo "Running tests..."
$BIN_DIR/test_telemetry_parser
$BIN_DIR/test_state_manager
$BIN_DIR/test_dashboard

echo "All host g++ tests passed."
