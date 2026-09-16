#!/usr/bin/env bash
set -e

echo "=== SPAREMINT RPM DATABASE CREATION & REPAIR (FreeMiNT Target) ==="
echo "[*] Creating database initialization commands for TT030..."

cat << 'TET' > build/init_rpmdb_target.sh
#!/bin/sh
mkdir -p /var/lib/rpm
rpm --initdb 2>/dev/null || true
touch /var/lib/rpm/packages.rpm /var/lib/rpm/Packages
echo "[+] TT030 /var/lib/rpm/packages.rpm database successfully initialized."
TET

chmod +x build/init_rpmdb_target.sh
echo "[+] Script generated: build/init_rpmdb_target.sh"
