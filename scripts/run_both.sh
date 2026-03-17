#!/usr/bin/env bash
set -euo pipefail

ADDR=${1:-127.0.0.1:20777}

# Start emulator in background and sender in foreground; cleanup on exit
./scripts/build_all.sh

./build/emulator &
EMU_PID=$!
trap 'kill ${EMU_PID} 2>/dev/null || true; exit' INT TERM EXIT
sleep 1
./build/telemetry_sender -addr "${ADDR}"
# sender will run until killed; after it exits, stop emulator
kill ${EMU_PID} || true
