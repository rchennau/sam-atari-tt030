#!/usr/bin/env bash
# ==============================================================================
# setup_emulator.sh - Install and verify Hatari / ARAnyM emulation on fractal
# Atari TT030 Ecosystem Integration - FR-10
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(dirname "$SCRIPT_DIR")"
EMU_DIR="${REPO_DIR}/emulator"
CONFIG_FILE="${EMU_DIR}/hatari.cfg"
ROM_DIR="/usr/share/hatari"
EMUTOS_ROM="/usr/share/emutos/etos512k.img"

usage() {
    cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Options:
  --install-deps       Attempt package installation via apt (hatari, aranym, mtools, lrzsz, net-tools)
  --check              Check status of emulator installations and ROM images
  -h, --help           Display this help message

Description:
  Sets up and verifies Hatari and ARAnyM emulation environments on fractal for Atari TT 030 testing.
  Prepares configuration files and checks ROM image availability.
EOF
    exit 0
}

INSTALL_DEPS=false
CHECK_ONLY=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --install-deps)
            INSTALL_DEPS=true
            shift
            ;;
        --check)
            CHECK_ONLY=true
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo "Error: Unknown argument $1" >&2
            exit 1
            ;;
    esac
done

echo "=== Atari TT030 Emulator Setup & Verification ==="

if [[ "${INSTALL_DEPS}" == "true" ]]; then
    echo "[1/3] Installing emulation & serial/network packages via apt..."
    sudo apt-get update
    sudo apt-get install -y hatari aranym mtools lrzsz net-tools picocom minicom
fi

echo "[2/3] Checking emulator binaries..."
HATARI_AVAIL=false
ARANYM_AVAIL=false

if command -v hatari &>/dev/null; then
    echo "  [OK] Hatari found: $(command -v hatari)"
    HATARI_AVAIL=true
else
    echo "  [WARN] Hatari not found in PATH."
fi

if command -v aranym &>/dev/null; then
    echo "  [OK] ARAnyM found: $(command -v aranym)"
    ARANYM_AVAIL=true
else
    echo "  [WARN] ARAnyM not found in PATH."
fi

echo "[3/3] Checking Atari TT ROM image files..."
if [[ -f "${ROM_DIR}/tos306us.img" ]]; then
    echo "  [OK] TOS 3.06 ROM found at ${ROM_DIR}/tos306us.img"
elif [[ -f "${EMUTOS_ROM}" ]]; then
    echo "  [OK] EmuTOS 512k ROM found at ${EMUTOS_ROM}"
elif [[ -f "${EMU_DIR}/tos306.img" ]]; then
    echo "  [OK] Custom TOS 3.06 ROM found at ${EMU_DIR}/tos306.img"
else
    echo "  [INFO] No default TOS 3.06 ROM found in standard paths."
    echo "         Note: EmuTOS or TOS 3.06 image should be placed in ${ROM_DIR}/ or ${EMU_DIR}/tos306.img"
fi

if [[ "${CHECK_ONLY}" == "true" ]]; then
    echo "Verification complete."
    exit 0
fi

echo "=== Emulator Setup Complete ==="
echo "To launch Hatari in Atari TT 030 mode with configured setup:"
echo "  hatari --config ${CONFIG_FILE}"
