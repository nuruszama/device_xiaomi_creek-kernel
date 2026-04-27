#!/bin/bash
echo "[+] Running Creek Hardware Seasoning..."

set -e

BASE="$(dirname "$0")/scripts"

echo "[*] Running Creek patch pipeline..."

bash "$BASE/01_qcom_defs.sh"
# bash "$BASE/02_qcom_defaults.sh"
# bash "$BASE/03_display_patch.sh"
# bash "$BASE/04_audio_patch.sh"

echo "[+] All patches applied successfully"

echo "[+] Seasoning complete."
