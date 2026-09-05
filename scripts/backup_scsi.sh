#!/usr/bin/env bash
# ==============================================================================
# backup_scsi.sh - Raw Sector Backup Utility for Atari TT030 Internal SCSI Drive
# Target: ~10GB Internal SCSI Drive before writing or partitioning
# Atari TT030 Ecosystem Integration - Phase 0
# ==============================================================================

set -euo pipefail

DEFAULT_OUTPUT_DIR="./backups"
TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
DEFAULT_IMAGE_NAME="tt030_scsi_raw_${TIMESTAMP}.img"
BLOCK_SIZE="64k"

usage() {
    cat <<EOF
Usage: $(basename "$0") -d DEVICE [OPTIONS]

Options:
  -d, --device DEV       Source SCSI block device (e.g., /dev/sdX) (REQUIRED)
  -o, --output PATH      Output image path or directory (default: ${DEFAULT_OUTPUT_DIR}/${DEFAULT_IMAGE_NAME})
  -b, --block-size BS    Transfer block size for dd/ddrescue (default: ${BLOCK_SIZE})
  -z, --compress         Compress output using zstd/gzip on the fly (.img.zst or .img.gz)
  -f, --force            Skip interactive confirmation prompt
  -h, --help             Display this help message

Description:
  Performs a raw sector-by-sector disk image backup of the Atari TT030 internal SCSI drive.
  Includes safety checks against system partitions, free space verification,
  checksum generation (SHA-256), and Atari AHDI/ICD sector 0 header verification.
EOF
    exit 0
}

DEVICE=""
OUTPUT_ARG=""
COMPRESS=false
FORCE=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        -d|--device)
            DEVICE="$2"
            shift 2
            ;;
        -o|--output)
            OUTPUT_ARG="$2"
            shift 2
            ;;
        -b|--block-size)
            BLOCK_SIZE="$2"
            shift 2
            ;;
        -z|--compress)
            COMPRESS=true
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

if [[ -z "${DEVICE}" ]]; then
    echo "Error: Source device (-d/--device) is required!" >&2
    usage
fi

if [[ ! -b "${DEVICE}" ]]; then
    echo "Error: Target device '${DEVICE}' is not a valid block device!" >&2
    exit 1
fi

# Sanity Check 1: Ensure target device is not a mounted system partition
if mount | grep -q "${DEVICE}"; then
    echo "ERROR: Device ${DEVICE} (or partition) is currently mounted on the system!" >&2
    echo "Backing up an active mounted system disk can cause corruption. Unmount first." >&2
    exit 1
fi

# Sanity Check 2: Get disk size
DISK_BYTES="$(sudo blockdev --getsize64 "${DEVICE}" 2>/dev/null || lsblk -bno SIZE "${DEVICE}" | head -n1 || echo 0)"
DISK_GB="$(awk "BEGIN {printf \"%.2f\", ${DISK_BYTES}/1073741824}")"

if [[ "${DISK_BYTES}" -eq 0 ]]; then
    echo "Warning: Could not determine exact disk size for ${DEVICE}."
fi

# Determine Output Path
if [[ -z "${OUTPUT_ARG}" ]]; then
    mkdir -p "${DEFAULT_OUTPUT_DIR}"
    OUTPUT_FILE="${DEFAULT_OUTPUT_DIR}/${DEFAULT_IMAGE_NAME}"
elif [[ -d "${OUTPUT_ARG}" ]]; then
    OUTPUT_FILE="${OUTPUT_ARG}/${DEFAULT_IMAGE_NAME}"
else
    OUTPUT_FILE="${OUTPUT_ARG}"
fi

if [[ "${COMPRESS}" == "true" ]]; then
    if command -v zstd &>/dev/null; then
        OUTPUT_FILE="${OUTPUT_FILE}.zst"
    else
        OUTPUT_FILE="${OUTPUT_FILE}.gz"
    fi
fi

OUT_DIR="$(dirname "${OUTPUT_FILE}")"
mkdir -p "${OUT_DIR}"

# Sanity Check 3: Destination Free Space
DEST_AVAIL_BYTES="$(df -B1 "${OUT_DIR}" | tail -n1 | awk '{print $4}')"
if [[ "${DISK_BYTES}" -gt 0 && "${COMPRESS}" == "false" ]]; then
    if [[ "${DEST_AVAIL_BYTES}" -lt "${DISK_BYTES}" ]]; then
        echo "ERROR: Insufficient free space in destination directory '${OUT_DIR}'!" >&2
        echo "Required: ~${DISK_GB} GB, Available: $(awk "BEGIN {printf \"%.2f\", ${DEST_AVAIL_BYTES}/1073741824}") GB" >&2
        exit 1
    fi
fi

echo "=== Atari TT030 Raw SCSI Disk Backup ==="
echo "Source Device:   ${DEVICE} (${DISK_GB} GB)"
echo "Destination:     ${OUTPUT_FILE}"
echo "Block Size:      ${BLOCK_SIZE}"
echo "Compression:     ${COMPRESS}"
echo "----------------------------------------"

if [[ "${FORCE}" != "true" ]]; then
    read -r -p "Begin raw sector read from ${DEVICE}? (y/N) " confirm
    if [[ "${confirm}" != [yY] && "${confirm}" != [yY][eE][sS] ]]; then
        echo "Backup canceled by user."
        exit 0
    fi
fi

echo "[1/3] Imaging device raw sectors..."
START_TIME="$(date +%s)"

if command -v ddrescue &>/dev/null && [[ "${COMPRESS}" == "false" ]]; then
    echo "Using GNU ddrescue for resilient sector extraction..."
    LOG_FILE="${OUTPUT_FILE}.map"
    sudo ddrescue -b 512 -d -v "${DEVICE}" "${OUTPUT_FILE}" "${LOG_FILE}"
elif [[ "${COMPRESS}" == "true" ]]; then
    if command -v zstd &>/dev/null; then
        echo "Piping dd raw stream to zstd compression..."
        sudo dd if="${DEVICE}" bs="${BLOCK_SIZE}" status=progress conv=noerror,sync | zstd -c -T0 > "${OUTPUT_FILE}"
    else
        echo "Piping dd raw stream to gzip compression..."
        sudo dd if="${DEVICE}" bs="${BLOCK_SIZE}" status=progress conv=noerror,sync | gzip -c > "${OUTPUT_FILE}"
    fi
else
    echo "Using dd with block size ${BLOCK_SIZE}..."
    sudo dd if="${DEVICE}" of="${OUTPUT_FILE}" bs="${BLOCK_SIZE}" status=progress conv=noerror,sync
fi

END_TIME="$(date +%s)"
ELAPSED=$((END_TIME - START_TIME))
echo "Imaging completed in ${ELAPSED} seconds."

# Step 2: Generate SHA-256 Checksum
echo "[2/3] Generating SHA-256 integrity checksum..."
CHECKSUM_FILE="${OUTPUT_FILE}.sha256"
sha256sum "${OUTPUT_FILE}" | tee "${CHECKSUM_FILE}"

# Step 3: Atari Sector 0 Root Sector Header Verification
echo "[3/3] Inspecting Sector 0 Partition Table Header..."
SECTOR0_HEX="$(mktemp)"
trap 'rm -f "${SECTOR0_HEX}"' EXIT

if [[ "${COMPRESS}" == "true" ]]; then
    if [[ "${OUTPUT_FILE}" == *.zst ]]; then
        zstd -dc "${OUTPUT_FILE}" | head -c 512 | xxd > "${SECTOR0_HEX}" || true
    else
        gzip -dc "${OUTPUT_FILE}" | head -c 512 | xxd > "${SECTOR0_HEX}" || true
    fi
else
    head -c 512 "${OUTPUT_FILE}" | xxd > "${SECTOR0_HEX}" || true
fi

echo "Sector 0 Hex Dump Preview (first 128 bytes):"
head -n 8 "${SECTOR0_HEX}"

if grep -qi -E 'AHDI|ICD|XGM|GEM|BGM|TOS' "${SECTOR0_HEX}"; then
    echo "SUCCESS: Atari partition signature detected in Sector 0."
else
    echo "NOTICE: No standard AHDI/ICD ASCII signature detected in sector 0. (Raw image preserved)."
fi

echo "=== Backup Complete! Raw SCSI Image saved to: ${OUTPUT_FILE} ==="
