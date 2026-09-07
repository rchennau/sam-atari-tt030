#!/bin/bash
set -e

EMU_DIR="/home/sam/Projects/atari-tt030-enhancement/emulator"

# Kill any existing background instances under current user
pkill -9 -f hatari 2>/dev/null || true

# Grant flatpak user sandbox permissions for host display & filesystem
flatpak override --user --filesystem=host --socket=x11 --socket=wayland --socket=fallback-x11 org.tuxfamily.hatari 2>/dev/null || true

echo "========================================================"
echo "  Atari TT 030 Workstation Emulation Environment Setup  "
echo "========================================================"
echo "ROM: ${EMU_DIR}/roms/tos306us.img"
echo "Config: ${EMU_DIR}/hatari.cfg"
echo "HD Root: ${EMU_DIR}/hd0"
echo "DISPLAY: ${DISPLAY:-:1}"
echo "--------------------------------------------------------"

export DISPLAY="${DISPLAY:-:1}"
flatpak run --user org.tuxfamily.hatari --configfile "${EMU_DIR}/hatari.cfg" "$@"
