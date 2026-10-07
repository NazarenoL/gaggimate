#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROGRAM="$ROOT/.pio/build/display-sim/program"
if [[ ! -x "$PROGRAM" ]]; then
    echo 'Build the web UI and display-sim before running this test.' >&2
    exit 1
fi
TEST_DIR="$(mktemp -d)"
echo "Test state and screenshots: $TEST_DIR"
cd "$TEST_DIR"
SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software "$PROGRAM" --test-beans
