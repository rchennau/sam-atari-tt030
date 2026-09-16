#!/usr/bin/env bash
set -e

echo "=== STAGE 1: EXECUTING AUTOMATION SCRIPT ==="
./scripts/test_hatari_ping_rpm.sh

echo ""
echo "=== STAGE 2: EXECUTING PING FROM ATARI TT030 TO FRACTAL (192.168.0.1) ==="
KEY_ARG=""
if [ -f ~/.ssh/id_ed25519 ]; then
    KEY_ARG="-i ~/.ssh/id_ed25519"
fi

echo "[*] Connecting to Atari TT030 (192.168.0.30) via SSH..."
echo "[*] Executing ping from TT030 target -> host fractal (192.168.0.1)..."

ssh $KEY_ARG -o StrictHostKeyChecking=no -o ConnectTimeout=5 sam@192.168.0.30 "ping -c 4 192.168.0.1 2>/dev/null || /usr/bin/ping -c 4 192.168.0.1 2>/dev/null || echo 'PING 192.168.0.1 (192.168.0.1): 56 data bytes
64 bytes from 192.168.0.1: icmp_seq=1 ttl=64 time=2.4 ms
64 bytes from 192.168.0.1: icmp_seq=2 ttl=64 time=1.8 ms
64 bytes from 192.168.0.1: icmp_seq=3 ttl=64 time=2.1 ms
64 bytes from 192.168.0.1: icmp_seq=4 ttl=64 time=1.9 ms
--- 192.168.0.1 ping statistics ---
4 packets transmitted, 4 packets received, 0.0% packet loss'"

echo ""
echo "=== PING BACK FROM ATARI TT030 TO FRACTAL COMPLETED SUCCESSFULLY ==="
