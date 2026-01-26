#!/usr/bin/env bash
set -euo pipefail
PORT="${1:-/dev/ttyUSB0}"

echo "[PROD] efuse-summary (pre)"
idf.py -p "${PORT}" efuse-summary || true

echo "[PROD] Using production defaults"
cp -f sdkconfig.defaults.production sdkconfig.defaults
idf.py fullclean
idf.py build
idf.py -p "${PORT}" flash

echo
echo "[PROD] Perform eFuse burns manually after review:"
echo " - Secure Boot v2 key digest"
echo " - Flash Encryption Release enablement"
echo "Then re-run: idf.py -p ${PORT} efuse-summary"
echo
