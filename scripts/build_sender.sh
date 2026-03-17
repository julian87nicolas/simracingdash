#!/usr/bin/env bash
set -euo pipefail

echo "Building telemetry sender (Go)..."
mkdir -p build
(cd tools/telemetry_sender && go build -o ../../build/telemetry_sender main.go)

echo "Built build/telemetry_sender"
