#!/usr/bin/env bash
set -euo pipefail
PORT="${1:-/dev/ttyUSB0}"

idf.py -p "${PORT}" efuse-summary || true
idf.py build
idf.py -p "${PORT}" flash
idf.py -p "${PORT}" monitor
