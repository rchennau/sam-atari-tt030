#!/bin/bash
set -e

EMU_DIR="/home/sam/Projects/atari-tt030-enhancement/emulator"

echo "========================================================"
echo "  Atari TT 030 Workstation Emulation Environment Setup  "
echo "========================================================"
echo "ROM: ${EMU_DIR}/roms/tos306us.img"
echo "Config: ${EMU_DIR}/hatari.cfg"
echo "HD Root: ${EMU_DIR}/hd0"
echo "--------------------------------------------------------"

flatpak run --user org.tuxfamily.hatari//stable --configfile "${EMU_DIR}/hatari.cfg" "$@"
