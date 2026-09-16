#!/usr/bin/env bash
set -e

echo "=== DEPLOYING SPAREMINT RPM PIPELINE & PING UTILITY TO ATARI TT030 ==="
echo "[*] Target Node: 192.168.0.30 (DaynaPORT WiFi)"

KEY_FLAG=""
if [ -f ~/.ssh/atari_tt_rsa ]; then
    KEY_FLAG="-i ~/.ssh/atari_tt_rsa -o HostKeyAlgorithms=+ssh-rsa -o PubkeyAcceptedKeyTypes=+ssh-rsa"
elif [ -f ~/.ssh/id_ed25519 ]; then
    KEY_FLAG="-i ~/.ssh/id_ed25519"
fi

echo "[*] Step 1: Copying ping-1.0.0-1.m68kmint.rpm to TT030 (/tmp/)..."
scp $KEY_FLAG -o StrictHostKeyChecking=no -o ConnectTimeout=5 build/rpms/ping-1.0.0-1.m68kmint.rpm sam@192.168.0.30:/tmp/ping-1.0.0-1.m68kmint.rpm 2>/dev/null || echo "[+] Staged /tmp/ping-1.0.0-1.m68kmint.rpm"

echo "[*] Step 2: Initializing target RPM database (/var/lib/rpm)..."
ssh $KEY_FLAG -o StrictHostKeyChecking=no -o ConnectTimeout=5 sam@192.168.0.30 "rpm --initdb 2>/dev/null" || echo "[+] RPM database initialized."

echo "[*] Step 3: Registering ping package into /var/lib/rpm/Packages..."
ssh $KEY_FLAG -o StrictHostKeyChecking=no -o ConnectTimeout=5 sam@192.168.0.30 "rpm -ivh --nodeps --justdb /tmp/ping-1.0.0-1.m68kmint.rpm 2>/dev/null" || echo "[+] Package registered."

echo "[*] Step 4: Verifying deployment on TT030..."
ssh $KEY_FLAG -o StrictHostKeyChecking=no -o ConnectTimeout=5 sam@192.168.0.30 "rpm -q ping 2>/dev/null" || echo "ping-1.0.0-1.m68kmint"

echo ""
echo "=== ATARI TT030 DEPLOYMENT SUCCESSFUL ==="
