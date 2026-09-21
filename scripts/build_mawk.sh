#!/usr/bin/env bash
# Cross-build mawk for the TT and package it as an m68kmint RPM (tt030-rpm-pipeline FR-6, first recipe).
# The TT has no awk at all (2026-09-21) and the yum client (FR-3) is sh+awk. mawk: autotools, ships its
# own generated parser (no yacc on fractal), smallest/fastest awk. Signature checked 2026-09-21 against
# Thomas Dickey's key (invisible-island.net/public/); the pin below is that verified tarball.
#
#   scripts/build_mawk.sh   -> build/mawk-<ver>-1.m68kmint.rpm   (needs scripts/setup_cross_mint.sh)
# ponytail: a dedicated script, not the generic build_recipe.sh — write that when recipe #2 exists.
set -euo pipefail
REPO=$(cd "$(dirname "$0")/.." && pwd)
V=1.3.4-20260302
SUM=e2c08a77d0a84a01f9be454d1ca3872d4f103f9ada683d075198b0c6e965633d
cd "$REPO/tools"
[ -f mawk-$V.tgz ] || curl -sfLO https://invisible-island.net/archives/mawk/mawk-$V.tgz
echo "$SUM  mawk-$V.tgz" | sha256sum -c -
rm -rf mawk-$V && tar xzf mawk-$V.tgz
( cd mawk-$V && PATH=$REPO/tools/cross-mint/usr/bin:$PATH CC=m68k-atari-mint-gcc CFLAGS="-m68020-60 -O2" \
    ./configure --host=m68k-atari-mint --prefix=/usr >/dev/null \
  && PATH=$REPO/tools/cross-mint/usr/bin:$PATH make -s -j"$(nproc)" mawk )
PATH=$REPO/tools/cross-mint/usr/bin:$PATH m68k-atari-mint-strip mawk-$V/mawk

mkdir -p "$REPO/build"
python3 - "$REPO" "$V" <<'EOF'
import sys
sys.path.insert(0, sys.argv[1] + "/scripts")
import rpm_header as R
repo, v = sys.argv[1], sys.argv[2]
ver, rel = v.split("-")
out = f"{repo}/build/mawk-{ver}-{rel}.m68kmint.rpm"
open(out, "wb").write(R.write_rpm("mawk", ver, rel, [
    ("/usr/bin/mawk", 0o100755, open(f"{repo}/tools/mawk-{v}/mawk", "rb").read()),
    ("/usr/bin/awk", 0o160777, b"mawk"),
], summary="mawk " + v + " (sam cross-build)", provides=["awk", "/usr/bin/awk"]))
print(out)
EOF
