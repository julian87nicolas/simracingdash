#!/usr/bin/env bash
set -euo pipefail

# Usage: ./scripts/run_sender.sh [IP] [PORT]
# or:    ./scripts/run_sender.sh IP:PORT

if [ $# -eq 0 ]; then
  ADDR=127.0.0.1
  PORT=20777
elif [ $# -eq 1 ]; then
  # allow IP:PORT or just IP
  if [[ "$1" == *":"* ]]; then
    ADDR=${1%%:*}
    PORT=${1##*:}
  else
    ADDR=$1
    PORT=20777
  fi
else
  ADDR=$1
  PORT=$2
fi

echo "Running telemetry sender -> ${ADDR}:${PORT}"
if [ ! -x build/telemetry_sender ]; then
  echo "build/telemetry_sender not found. Run ./scripts/build_sender.sh first." >&2
  exit 2
fi

./build/telemetry_sender -addr "${ADDR}:${PORT}"
