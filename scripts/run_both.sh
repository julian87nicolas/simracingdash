#!/usr/bin/env bash
set -euo pipefail

ADDR=${1:-127.0.0.1:20777}

# Build sender and run it (emulator removed)
./scripts/build_all.sh

echo "Running telemetry sender -> ${ADDR}"
./build/telemetry_sender -addr "${ADDR}"
