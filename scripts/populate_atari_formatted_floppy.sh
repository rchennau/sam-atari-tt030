#!/usr/bin/env bash
set -euo pipefail

DEV="/dev/sdf"
echo "=== Atari TT Formatted Floppy Inspector & Inserter ==="
if [[ ! -b "$DEV" ]]; then
    echo "Error: $DEV not attached!"
    exit 1
fi

echo "[1/3] Mounting Atari TT 030 formatted floppy on /mnt..."
sudo mount -t vfat -o umask=000 "$DEV" /mnt

echo "[2/3] Disk mounted cleanly. Copying staging driver files..."
rsync -rtDhP --fsync /home/sam/Projects/atari-tt030-enhancement/staging/ /mnt/

echo "[3/3] Syncing buffers and unmounting..."
sync
sudo umount /mnt
echo "SUCCESS: Atari TT formatted floppy populated cleanly!"
