#!/usr/bin/env bash
set -euo pipefail

echo "Building emulator..."
mkdir -p build

g++ -std=c++17 \
  -Itools/emulator -Itest/host/include -Iinclude \
  src/telemetry_parser.cpp src/state.cpp src/dashboard_manager.cpp \
  src/dashboards/dash_main.cpp src/dashboards/dash_tyres.cpp src/dashboards/dash_ers.cpp src/dashboards/dash_damage.cpp \
  tools/emulator/emulator.cpp -o build/emulator

echo "Built build/emulator" 
