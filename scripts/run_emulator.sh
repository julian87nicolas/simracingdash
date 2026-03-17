#!/usr/bin/env bash
set -euo pipefail

echo "Running emulator (build/emulator)..."
if [ ! -x build/emulator ]; then
  echo "build/emulator not found or not executable. Run ./scripts/build_emulator.sh first." >&2
  exit 2
fi

./build/emulator
