#!/bin/sh
# Build netpbm's giftopnm for the T425 (gifserv; tt030-t425-kernel-ports W2) with the INMOS toolset.
#   sam-yum-tt/t4gif.sh [DIR]   -> DIR/gifserv.b4h (ships as /usr/lib/t425/tgiftopnm.btl), DIR/gift4test.b4h
# DIR must be SHORT (default /tmp/gf). netpbm 9.5 from the SpareMiNT SRPM, copied, never edited:
#   giftopnm.c  main -> giftopnm_main, exit -> ps_exit, fread -> ps_fread (the GIF in memory), malloc/free -> the pmshim arena
set -eu
R=$(cd "$(dirname "$0")/.." && pwd)
D=${1:-/tmp/gf}
N=$R/tools/rpm-src/x/netpbm-9.5-1/netpbm-9.5
rm -rf "$D" && mkdir -p "$D/sys" && cd "$D"
cp "$N"/pnm/giftopnm.c "$N"/pnm/pnm.h "$N"/ppm/ppm.h "$N"/ppm/libppm.h "$N"/pgm/pgm.h "$N"/pgm/libpgm.h \
   "$N"/pbm/pbm.h "$N"/pbm/libpbm.h "$N"/pbmplus.h "$N"/shhopt/shhopt.h .
cp "$R"/sam-yum-tt/src/pmshim.[ch] "$R"/sam-yum-tt/src/gifrun.h "$R"/sam-yum-tt/src/gifserv.c \
   "$R"/sam-yum-tt/src/gift4test.c "$R"/sam-yum-tt/src/t4serv.[ch] "$R"/sam-yum-tt/src/t4adler.[ch] .
printf '/* t4gif stub */\ntypedef long off_t;\n' > sys/types.h
for h in unistd.h sys/stat.h fcntl.h; do printf '/* t4gif stub */\n' > "$h"; done
. "$R/tools/d72uni-env.sh"
ISEARCH="$D/ $D72UNI/libs/"
export ISEARCH IBOARDSIZE
cc_() { icc "$1.c" -t4 -dPPM_PACKCOLORS $2 -o "$1.t4h" > "$1.log" 2>&1 || { cat "$1.log"; exit 1; }
        grep -E '^(Serious|Error|Fatal)' "$1.log" && exit 1; true; }
# Renames in a prepended header (a long -d list overflows icc's command line under t4).
printf '%s\n' '#include <stdlib.h>' '#define malloc ps_malloc' '#define free ps_free' '#define exit ps_exit' '#define fread ps_fread' \
    '#include "pmshim.h"' > psren.h
printf '#define main giftopnm_main\n#include "psren.h"\n' | cat - giftopnm.c > s.tmp && mv s.tmp giftopnm.c
cc_ giftopnm ""
for f in pmshim gifserv gift4test t4serv t4adler; do cc_ $f ""; done
printf '%s\n' t4serv.t4h gifserv.t4h pmshim.t4h giftopnm.t4h t4adler.t4h '#include startup.lnk' > gifserv.lnk
printf '%s\n' gift4test.t4h pmshim.t4h giftopnm.t4h '#include startup.lnk' > gift4test.lnk
for m in gifserv gift4test; do
    ilink -f "$m.lnk" -t4 -h -o "$m.c4h"
    icollect "$m.c4h" -t -o "$m.b4h"
    [ -s "$m.b4h" ] || { echo "t4gif: $m.b4h not built" >&2; exit 1; }
done
ls -l "$D"/gifserv.b4h "$D"/gift4test.b4h
