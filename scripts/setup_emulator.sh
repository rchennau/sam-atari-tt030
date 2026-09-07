#!/bin/bash
set -e

EMU_DIR="/home/sam/Projects/atari-tt030-enhancement/emulator"

# Kill any existing background instances under current user
pkill -9 -f hatari 2>/dev/null || true

# Grant flatpak user sandbox permissions for host display & filesystem
flatpak override --user --filesystem=host --socket=x11 --socket=wayland --socket=fallback-x11 org.tuxfamily.hatari 2>/dev/null || true

# Setup tap0 network interface if not present
if ip link show tap0 >/dev/null 2>&1; then
    NET_ARGS="--net-type tap --net-interface tap0"
else
    NET_ARGS=""
fi

echo "========================================================"
echo "  Atari TT 030 Workstation Emulation Environment Setup  "
echo "========================================================"
echo "ROM: ${EMU_DIR}/roms/tos104.img"
echo "Config: ${EMU_DIR}/hatari.cfg"
echo "HD Root: ${EMU_DIR}/hd0"
echo "Network: ${NET_ARGS:-None (Run setup_hatari_tap.sh for TAP)}"
echo "DISPLAY: ${DISPLAY:-:0}"
echo "--------------------------------------------------------"

export DISPLAY="${DISPLAY:-:0}"
flatpak run --user org.tuxfamily.hatari ${NET_ARGS} --configfile "${EMU_DIR}/hatari.cfg" "$@"
