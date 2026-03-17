#!/usr/bin/env bash
set -euo pipefail

# Build only the telemetry sender (emulator removed)
./scripts/build_sender.sh

echo "All built."
