#!/usr/bin/env bash
# ==============================================================================
# serial_bridge.sh - Serial-to-USB Bridge & SLIP Networking Utility for Atari TT030
# Target Device: /dev/ttyUSB0 @ 57600 baud
# Atari TT030 Ecosystem Integration - FR-9
# ==============================================================================

set -euo pipefail

DEFAULT_PORT="/dev/ttyUSB0"
DEFAULT_BAUD="57600"
DEFAULT_SLIP_LOCAL="192.168.10.1"
DEFAULT_SLIP_REMOTE="192.168.10.2"

PORT="${DEFAULT_PORT}"
BAUD="${DEFAULT_BAUD}"
LOCAL_IP="${DEFAULT_SLIP_LOCAL}"
REMOTE_IP="${DEFAULT_SLIP_REMOTE}"

usage() {
    cat <<EOF
Usage: $(basename "$0") [MODE] [OPTIONS]

Modes:
  console              Interactive serial terminal console (using picocom / minicom)
  slip                 Start SLIP network bridge interface (using slattach)
  zmodem-send FILE     Send file to Atari TT030 via ZMODEM (using sz)
  zmodem-recv [DIR]    Receive file from Atari TT030 via ZMODEM (using rz)
  -h, --help           Display this help message

Options:
  -p, --port DEV       Serial device path (default: ${DEFAULT_PORT})
  -b, --baud SPEED     Baud rate (default: ${DEFAULT_BAUD})
  --local-ip IP        Local host IP for SLIP bridge (default: ${DEFAULT_SLIP_LOCAL})
  --remote-ip IP       Atari TT030 remote IP for SLIP bridge (default: ${DEFAULT_SLIP_REMOTE})

Examples:
  $(basename "$0") console -p /dev/ttyUSB0 -b 57600
  $(basename "$0") slip --local-ip 192.168.10.1 --remote-ip 192.168.10.2
  $(basename "$0") zmodem-send firmware.img
  $(basename "$0") zmodem-recv ./downloads
EOF
    exit 0
}

MODE=""
if [[ $# -gt 0 && ! "$1" =~ ^- ]]; then
    MODE="$1"
    shift
fi

while [[ $# -gt 0 ]]; do
    case "$1" in
        -p|--port)
            PORT="$2"
            shift 2
            ;;
        -b|--baud)
            BAUD="$2"
            shift 2
            ;;
        --local-ip)
            LOCAL_IP="$2"
            shift 2
            ;;
        --remote-ip)
            REMOTE_IP="$2"
            shift 2
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

if [[ -z "${MODE}" ]]; then
    echo "Error: No mode specified." >&2
    usage
fi

check_port() {
    if [[ ! -c "${PORT}" ]]; then
        echo "Warning: Serial port device '${PORT}' does not exist or is not a character device." >&2
        echo "Ensure the USB-to-Serial adapter is connected to fractal." >&2
    fi
}

run_console() {
    check_port
    echo "=== Starting Serial Console on ${PORT} @ ${BAUD} baud ==="
    if command -v picocom &>/dev/null; then
        echo "Using picocom (Press Ctrl+A then Ctrl+X to exit)..."
        exec picocom -b "${BAUD}" "${PORT}"
    elif command -v minicom &>/dev/null; then
        echo "Using minicom..."
        exec minicom -b "${BAUD}" -D "${PORT}"
    else
        echo "Error: Neither 'picocom' nor 'minicom' is installed on this system." >&2
        exit 1
    fi
}

run_slip() {
    check_port
    echo "=== Initializing SLIP Network Bridge on ${PORT} @ ${BAUD} baud ==="
    echo "Local IP:  ${LOCAL_IP}"
    echo "Remote IP: ${REMOTE_IP}"

    if ! command -v slattach &>/dev/null; then
        echo "Error: 'slattach' utility not found (net-tools package)." >&2
        exit 1
    fi

    echo "Attaching SLIP interface via slattach..."
    sudo slattach -p slip -s "${BAUD}" "${PORT}" &
    SLATTACH_PID=$!

    sleep 2

    # Identify created sl interface (typically sl0)
    SL_DEV="sl0"
    if ip link show "${SL_DEV}" &>/dev/null; then
        echo "Configuring network interface ${SL_DEV}..."
        sudo ip addr add "${LOCAL_IP}" pointopoint "${REMOTE_IP}" dev "${SL_DEV}"
        sudo ip link set "${SL_DEV}" up
        echo "SLIP bridge active on ${SL_DEV} (${LOCAL_IP} <-> ${REMOTE_IP})."
        echo "Press Ctrl+C to stop SLIP bridge."
        wait "${SLATTACH_PID}"
    else
        echo "Error: Could not find SLIP interface ${SL_DEV} after slattach execution." >&2
        kill "${SLATTACH_PID}" 2>/dev/null || true
        exit 1
    fi
}

run_zmodem_send() {
    local file="$1"
    check_port
    if [[ ! -f "${file}" ]]; then
        echo "Error: File '${file}' does not exist." >&2
        exit 1
    fi
    if ! command -v sz &>/dev/null; then
        echo "Error: 'sz' utility not found (lrzsz package)." >&2
        exit 1
    fi
    echo "=== Sending '${file}' via ZMODEM over ${PORT} @ ${BAUD} baud ==="
    stty -F "${PORT}" "${BAUD}" raw -echo
    sz --zmodem "${file}" > "${PORT}" < "${PORT}"
    echo "Transfer complete."
}

run_zmodem_recv() {
    local dest_dir="${1:-.}"
    check_port
    mkdir -p "${dest_dir}"
    if ! command -v rz &>/dev/null; then
        echo "Error: 'rz' utility not found (lrzsz package)." >&2
        exit 1
    fi
    echo "=== Waiting for ZMODEM incoming transfer on ${PORT} @ ${BAUD} baud ==="
    stty -F "${PORT}" "${BAUD}" raw -echo
    (cd "${dest_dir}" && rz --zmodem > "${PORT}" < "${PORT}")
    echo "Transfer complete."
}

case "${MODE}" in
    console)
        run_console
        ;;
    slip)
        run_slip
        ;;
    zmodem-send)
        if [[ $# -lt 1 ]]; then
            echo "Error: zmodem-send requires a filename argument." >&2
            exit 1
        fi
        run_zmodem_send "$1"
        ;;
    zmodem-recv)
        run_zmodem_recv "${1:-.}"
        ;;
    *)
        echo "Error: Unknown mode '${MODE}'" >&2
        usage
        ;;
esac
