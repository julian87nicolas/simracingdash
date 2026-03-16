#!/usr/bin/env bash
set -e
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BIN_DIR="$ROOT_DIR/tests/bin"
mkdir -p "$BIN_DIR"

echo "Compiling telemetry parser test..."
g++ -std=c++17 -I$ROOT_DIR/tests/include -I$ROOT_DIR/include -I$ROOT_DIR/src \
  $ROOT_DIR/src/telemetry_parser.cpp $ROOT_DIR/tests/test_telemetry_parser.cpp $ROOT_DIR/tests/serial_stub.cpp -o $BIN_DIR/test_telemetry_parser

echo "Compiling state manager test..."
g++ -std=c++17 -I$ROOT_DIR/tests/include -I$ROOT_DIR/include -I$ROOT_DIR/src \
  $ROOT_DIR/src/state.cpp $ROOT_DIR/tests/test_state_manager.cpp $ROOT_DIR/tests/serial_stub.cpp -o $BIN_DIR/test_state_manager

echo "Compiling dashboard render test..."
g++ -std=c++17 -I$ROOT_DIR/tests/include -I$ROOT_DIR/include -I$ROOT_DIR/src \
  $ROOT_DIR/src/dashboards/dash_main.cpp $ROOT_DIR/tests/test_dashboard.cpp $ROOT_DIR/tests/serial_stub.cpp -o $BIN_DIR/test_dashboard

echo "Running tests..."
$BIN_DIR/test_telemetry_parser
$BIN_DIR/test_state_manager
$BIN_DIR/test_dashboard

echo "All tests passed."
