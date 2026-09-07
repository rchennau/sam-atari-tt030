#!/bin/bash
# Setup Hatari TAP network interface bridge for STiNG emulation on fractal
set -e

echo "========================================================"
echo "  Hatari TAP Network Interface Setup (STiNG Emulation)  "
echo "========================================================"

# Check if tap0 interface exists
if ip link show tap0 >/dev/null 2>&1; then
    echo "[OK] tap0 network interface already present."
else
    echo "[INFO] Creating tap0 interface for user $USER..."
    sudo ip tuntap add mode tap user "$USER" name tap0
    sudo ip addr add 192.168.0.254/24 dev tap0
    sudo ip link set tap0 up
    echo "[OK] tap0 created and configured (IP: 192.168.0.254)."
fi

# Enable IP forwarding on fractal
sudo sysctl -w net.ipv4.ip_forward=1 >/dev/null

echo "--------------------------------------------------------"
echo "Launch Hatari with DaynaPORT TAP network bridging:"
echo "flatpak run --user org.tuxfamily.hatari --net-type tap --net-interface tap0 --configfile /home/sam/Projects/atari-tt030-enhancement/emulator/hatari.cfg"
echo "========================================================"
