#!/usr/bin/env bash
set -euo pipefail

./scripts/build_emulator.sh
./scripts/build_sender.sh

echo "All built."
