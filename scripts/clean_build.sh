#!/usr/bin/env bash
# Clean PlatformIO build and cache directories
set -euo pipefail
echo "Removing .pio and .pioenvs to force a clean rebuild..."
rm -rf .pio .pioenvs .cache
echo "Done. Now run: platformio run -e nodemcuv2 -v"
