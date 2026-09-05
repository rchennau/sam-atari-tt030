#!/usr/bin/env bash
# ==============================================================================
# stage_floppy.sh - Format and populate 1.44MB FAT12 Floppy Image / USB Floppy
# Target Device: /dev/sdf (USB Floppy on fractal) or disk image file.
# Atari TT030 Ecosystem Integration - Phase 0
# ==============================================================================

set -euo pipefail

DEFAULT_DEVICE="/dev/sdf"
IMAGE_FILE="atari_boot_144MB.img"
VOLUME_NAME="TT030_BOOT"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(dirname "$SCRIPT_DIR")"
STAGING_DIR="${REPO_DIR}/staging"

usage() {
    cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Options:
  -d, --device DEV      Target block device (default: ${DEFAULT_DEVICE})
  -i, --image-only      Create 1.44MB FAT12 image file (${IMAGE_FILE}) only, do not write to block device
  -f, --force           Skip confirmation prompt before writing to block device
  -h, --help            Display this help message

Description:
  Formats a 1.44MB (1440 KiB) FAT12 floppy disk image or physical USB floppy drive (/dev/sdf),
  and populates it with the Atari TT030 Phase 0 boot structure:
    - AUTO/          : Boot selectors (XBOOT III, ST-Menu, Selectric)
    - DRIVERS/SCSI/  : ICD AdSCSI & AHDI drivers
    - DRIVERS/NET/   : STiNG network stack & DaynaPORT SCSI drivers
    - BLUESCSI/      : bluescsi.ini config for TT030
EOF
    exit 0
}

DEVICE="${DEFAULT_DEVICE}"
IMAGE_ONLY=false
FORCE=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        -d|--device)
            DEVICE="$2"
            shift 2
            ;;
        -i|--image-only)
            IMAGE_ONLY=true
            shift
            ;;
        -f|--force)
            FORCE=true
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

echo "=== Atari TT030 Floppy Staging Utility ==="
echo "Staging directory: ${STAGING_DIR}"
echo "Image output file: ${IMAGE_FILE}"
echo "Target device:     ${DEVICE}"

if [[ ! -d "${STAGING_DIR}" ]]; then
    echo "Error: Staging directory ${STAGING_DIR} does not exist!" >&2
    exit 1
fi

# Step 1: Create 1.44MB (1440 KiB = 2880 sectors of 512 bytes) image file
echo "[1/4] Creating blank 1.44MB floppy image file (${IMAGE_FILE})..."
dd if=/dev/zero of="${IMAGE_FILE}" bs=512 count=2880 status=none

echo "[2/4] Formatting image as FAT12 (Volume label: ${VOLUME_NAME})..."
mkfs.vfat -F 12 -n "${VOLUME_NAME}" "${IMAGE_FILE}"

echo "[3/4] Populating floppy image from staging directory..."
# We use mcopy from mtools if available, or 7z, or fatcat
if command -v mcopy &>/dev/null; then
    echo "Using mtools (mcopy) to transfer files to FAT12 image..."
    export MTOOLS_SKIP_CHECK=1
    for item in "${STAGING_DIR}"/*; do
        if [[ -e "$item" ]]; then
            mcopy -i "${IMAGE_FILE}" -s "$item" ::/
        fi
    done
elif command -v 7z &>/dev/null; then
    echo "Using 7z to populate FAT12 image..."
    (cd "${STAGING_DIR}" && 7z a "${REPO_DIR}/${IMAGE_FILE}" ./*)
else
    echo "Error: Neither mtools (mcopy) nor 7z found! Please install mtools (e.g. sudo apt install mtools) to populate FAT images without root." >&2
    exit 1
fi

echo "Floppy image ${IMAGE_FILE} built successfully."

# Step 4: Write to device if requested
if [[ "${IMAGE_ONLY}" == "true" ]]; then
    echo "[4/4] Image-only flag set. Skipping physical device write."
    echo "Done! Image saved to: ${IMAGE_FILE}"
    exit 0
fi

if [[ ! -b "${DEVICE}" ]]; then
    echo "Warning: Target block device '${DEVICE}' not found or not a block device."
    echo "Floppy disk image is ready at: ${IMAGE_FILE}"
    echo "To write manually later: sudo dd if=${IMAGE_FILE} of=${DEVICE} bs=64k status=progress"
    exit 0
fi

# Sanity check: Ensure device is not a mounted system partition
if mount | grep -q "${DEVICE}"; then
    echo "Error: Target device ${DEVICE} is currently mounted! Unmount it first." >&2
    exit 1
fi

if [[ "${FORCE}" != "true" ]]; then
    echo "--------------------------------------------------------"
    echo "WARNING: OVERWRITING DEVICE ${DEVICE} !"
    echo "All data on ${DEVICE} will be permanently erased."
    echo "--------------------------------------------------------"
    read -r -p "Are you sure you want to proceed writing to ${DEVICE}? (y/N) " confirm
    if [[ "${confirm}" != [yY] && "${confirm}" != [yY][eE][sS] ]]; then
        echo "Aborted by user."
        exit 0
    fi
fi

echo "[4/4] Writing ${IMAGE_FILE} to ${DEVICE}..."
sudo dd if="${IMAGE_FILE}" of="${DEVICE}" bs=64k status=progress conv=fsync
sync

echo "=== Staging Complete! Physical USB Floppy ${DEVICE} populated successfully. ==="
