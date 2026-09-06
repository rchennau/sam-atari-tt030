#!/usr/bin/env bash
set -euo pipefail

DEV="/dev/sdf"
echo "=== Atari TT Formatted Floppy Inspector & Inserter ==="
if [[ ! -b "$DEV" ]]; then
    echo "Error: $DEV not attached!"
    exit 1
fi

sudo -n umount /mnt 2>/dev/null || true
echo "[1/3] Mounting Atari TT 030 formatted floppy..."
sudo -n mount -t vfat -o umask=000 "$DEV" /mnt

echo "[2/3] Copying drivers to Atari formatted floppy..."
rsync -rtDhP --fsync /home/sam/Projects/atari-tt030-enhancement/staging/ /mnt/

echo "[3/3] Syncing and unmounting..."
sync
sudo -n umount /mnt
echo "SUCCESS: Atari TT formatted floppy populated cleanly!"
