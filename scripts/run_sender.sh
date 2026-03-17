#!/usr/bin/env bash
set -euo pipefail

ADDR=${1:-127.0.0.1:20777}

echo "Running telemetry sender -> ${ADDR}"
if [ ! -x build/telemetry_sender ]; then
  echo "build/telemetry_sender not found. Run ./scripts/build_sender.sh first." >&2
  exit 2
fi

./build/telemetry_sender -addr "${ADDR}"
