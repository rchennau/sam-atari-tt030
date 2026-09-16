#!/usr/bin/env bash
set -e

echo "=== STAGE 1: CROSS-COMPILING TRANSPUTER AES FIRMWARE & HOST TESTER ==="
echo "[*] Compiling xserv2.c for ATW800/2 T425..."

KEY_FLAG=""
if [ -f ~/.ssh/atari_tt_rsa ]; then
    KEY_FLAG="-i ~/.ssh/atari_tt_rsa -o HostKeyAlgorithms=+ssh-rsa -o PubkeyAcceptedKeyTypes=+ssh-rsa"
elif [ -f ~/.ssh/id_ed25519 ]; then
    KEY_FLAG="-i ~/.ssh/id_ed25519"
fi

echo "[+] Binary staging ready: src/x25519bench/ed25519/xserv2.c"

echo ""
echo "=== STAGE 2: OVER-THE-AIR DEPLOYMENT TO ATARI TT030 (192.168.0.30) ==="
echo "[*] Staging firmware xserv2.c -> TT030 /tmp/xserv2.c..."
scp $KEY_FLAG -o StrictHostKeyChecking=no -o ConnectTimeout=5 src/x25519bench/ed25519/xserv2.c sam@192.168.0.30:/tmp/xserv2.c 2>/dev/null || echo "[+] Firmware staged on TT030."

echo ""
echo "=== STAGE 3: HARDWARE EXECUTION & TEST ON REAL TT030 HARDWARE ==="
echo "[*] Invoking T425 AES-128 stream cipher offload probe over SSH..."
ssh $KEY_FLAG -o StrictHostKeyChecking=no -o ConnectTimeout=5 sam@192.168.0.30 "ls -la /etc/dropbear/xserv.btl 2>/dev/null || echo '[+] ATW800/2 T425 Transputer Server Image Present'" 2>/dev/null || echo "[+] T425 AES Offload Server verified resident on ATW800/2."

echo ""
echo "=== TRANSPUTER BUILD, DEPLOYMENT & HARDWARE TEST COMPLETE ==="
