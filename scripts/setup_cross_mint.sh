#!/usr/bin/env bash
# Set up the 68k Atari cross-compiler on fractal (TT/TOS/FreeMiNT programs). No root.
# Thorsten Otto's Linux-hosted m68k-atari-mint GCC 15.2.0 + binutils 2.45 + mintlib/fdlibm/gemlib,
# unpacked from staging/CROSSMINT/ (checksums in SHA256SUMS) into tools/cross-mint (gitignored).
# The tarballs expect /usr; gcc is relocatable, so everything lives under tools/cross-mint/usr.
#
#   scripts/setup_cross_mint.sh     then:  export PATH=$PWD/tools/cross-mint/usr/bin:$PATH
#   m68k-atari-mint-gcc -m68020-60 -O2 -o prog.prg prog.c        # TT = 68030
set -euo pipefail
REPO=$(cd "$(dirname "$0")/.." && pwd)
SRC=$REPO/staging/CROSSMINT
X=$REPO/tools/cross-mint

(cd "$SRC" && sha256sum -c --quiet SHA256SUMS)
rm -rf "$X" && mkdir -p "$X"
for f in "$SRC"/*.tar.xz; do tar xJf "$f" -C "$X"; done

# self-test: compile for the TT's 68030 and check for a GEMDOS program header
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
printf '#include <stdio.h>\nint main(void) { printf("hello\\n"); return 0; }\n' > "$WORK/hello.c"
"$X/usr/bin/m68k-atari-mint-gcc" -m68020-60 -O2 -o "$WORK/hello.prg" "$WORK/hello.c"
if [ "$(xxd -l 2 -p "$WORK/hello.prg")" = 601a ]; then
  echo "cross-mint OK: $("$X/usr/bin/m68k-atari-mint-gcc" --version | head -1)"
else
  echo "self-test FAILED: no GEMDOS header" >&2; exit 1
fi
