#!/bin/bash
# Build HighWire 0.4.8 (GPL-3, github.com/neilrackett/atarist-highwire @ 9af7034) for the TT's 68030 with the side
# toolchain, plus the MiNTnet network overlay. HighWire has no TLS of its own: HTTP_PROXY sends 'GET https://...'
# to a TLS-terminating proxy (scripts/tt_tlsproxy.py). Output: tools/src-highwire/build-hw/dist/highwire.prg and
# modules/mintnet.ovl. Install the overlay as modules/network.ovl (lower case: ext2 is case-sensitive) with the
# execute bit set (ext2 enforces it for Pexec; a copy off FAT loses it -> 'Pexec failed: -36').
. "$(dirname "$0")/../../scripts/tt_log.sh"   # log every run for SAM (operator rule 2026-10-13)
set -euo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd); S=$R/tools/src-highwire; P=$R/packaging/highwire
C=$R/tools/cflib-src; Z=$R/tools/rpm-src/sysroot/usr
export PATH=$R/tools/cross-mint/usr/bin:$PATH
[ -d $S/hw ] || git clone -q https://github.com/neilrackett/atarist-highwire.git $S/hw
git -C $S/hw checkout -q 9af7034
rm -rf $S/build-hw && cp -r $S/hw $S/build-hw
patch -d $S/build-hw -p1 < $P/inet-numeric-addr.patch
patch -d $S/build-hw -p1 < $P/makefile-no-werror.patch     # gcc 15 warnings that -Werror turns fatal
make -C $S/build-hw -j16 CPU=68030 CC="m68k-atari-mint-gcc -g -I$C -I$Z/include" LD="m68k-atari-mint-gcc -L$C/.build/m68020 -L$Z/lib" >/dev/null
make -C $S/build-hw/modules/network.src CPU=68030 CC="m68k-atari-mint-gcc -g -I$C" >/dev/null
ls -l $S/build-hw/dist/highwire.prg $S/build-hw/modules/mintnet.ovl
