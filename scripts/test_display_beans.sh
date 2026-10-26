#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SIM_ENV="${DISPLAY_SIM_ENV:-display-sim}"
PROGRAM="$ROOT/.pio/build/$SIM_ENV/program"
if [[ ! -x "$PROGRAM" ]]; then
    echo "Build the web UI and $SIM_ENV before running this test." >&2
    exit 1
fi
TEST_DIR="${DISPLAY_SIM_TEST_DIR:-$(mktemp -d)}"
mkdir -p "$TEST_DIR"
echo "Test state and screenshots: $TEST_DIR"
cd "$TEST_DIR"
SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software "$PROGRAM" --test-beans
