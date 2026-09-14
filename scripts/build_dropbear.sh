#!/usr/bin/env bash
# Cross-build Dropbear for the TT (FreeMiNT, m68k-atari-mint) and a native dropbearkey for fractal.
#
#   scripts/build_dropbear.sh      -> tools/dropbear-<ver>/{dropbear,dropbearkey,dbclient}  (m68k)
#                                     tools/dropbear-native/dropbear-<ver>/dropbearkey     (host)
#
# Options are in staging/DROPBEAR/localoptions.h: curve25519 + ed25519 only (OpenSSH 5.6's DH/RSA
# could not finish a login within 120 s on the 32 MHz 68030). MiNT-specific settings, each found on
# the TT on 2026-09-12: DROP_PRIVS 0 (no setresgid), MULTIUSER 1 (MULTIUSER 0 exits "requires a
# non-multiuser kernel"), stream forwarding off (build guard). Needs scripts/setup_cross_mint.sh first.
set -euo pipefail
REPO=$(cd "$(dirname "$0")/.." && pwd)
VER=dropbear-2026.94
SUM=e098034a843699200c8c977a991fff73159735bf795d5f72ef672c41a6b1ae81
cd "$REPO/tools"
[ -f $VER.tar.bz2 ] || curl -sfLO https://matt.ucc.asn.au/dropbear/releases/$VER.tar.bz2
echo "$SUM  $VER.tar.bz2" | sha256sum -c -

# Monocypher (32-bit-limb curve25519/ed25519) replaces Dropbear's TweetNaCl-style curve25519.c, whose
# 64-bit field math made a login take 224 s on the TT. Applied to both the m68k and the native tree.
patch_curve() {
  cp "$REPO/staging/DROPBEAR/curve25519-monocypher.c" "$1/src/curve25519.c"
  cp "$REPO"/staging/DROPBEAR/monocypher{.c,.h,-ed25519.c,-ed25519.h} "$REPO"/staging/DROPBEAR/atwx25519.c "$1/src/"
}
# Run it with -F (and background it from the shell): when Dropbear daemonises itself on MiNT, each
# connection's child exits "setsid: Bad file descriptor" (TT, 2026-09-12); with -F it works.
rm -rf $VER && tar xjf $VER.tar.bz2
cp "$REPO/staging/DROPBEAR/localoptions.h" $VER/
patch_curve $VER
( cd $VER && PATH=$REPO/tools/cross-mint/usr/bin:$PATH \
    CC=m68k-atari-mint-gcc AR=m68k-atari-mint-ar RANLIB=m68k-atari-mint-ranlib CFLAGS="-m68020-60 -O2" \
    ./configure --host=m68k-atari-mint --disable-zlib --disable-lastlog --disable-utmp --disable-utmpx \
      --disable-wtmp --disable-wtmpx --disable-pututline --disable-pututxline --enable-static --disable-harden >/dev/null \
  && PATH=$REPO/tools/cross-mint/usr/bin:$PATH make -s PROGRAMS="dropbear dropbearkey dbclient" STATIC=1 -j"$(nproc)" )

rm -rf dropbear-native && mkdir dropbear-native && tar xjf $VER.tar.bz2 -C dropbear-native
patch_curve dropbear-native/$VER
# Native dropbear too: an interop test against OpenSSH on fractal proves the Monocypher swap is correct.
( cd dropbear-native/$VER && ./configure --disable-zlib >/dev/null && make -s PROGRAMS="dropbear dropbearkey dropbearconvert" -j"$(nproc)" )
ls -l $VER/dropbear $VER/dropbearkey $VER/dbclient dropbear-native/$VER/dropbearkey
