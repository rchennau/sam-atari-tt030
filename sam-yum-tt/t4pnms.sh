#!/bin/sh
# Build netpbm's pnmscale for the T425 (pnmsserv; tt030-t425-kernel-ports W2) with the INMOS toolset.
#   sam-yum-tt/t4pnms.sh [DIR]   -> DIR/pnmsserv.b4h (ships as /usr/lib/t425/tpnmscale.btl), DIR/pnmst4test.b4h
# DIR must be SHORT (default /tmp/ps). netpbm 9.5 from the SpareMiNT SRPM, copied, never edited:
#   pnmscale.c  main -> pnmscale_main, exit -> ps_exit, malloc/free -> the pmshim arena
set -eu
R=$(cd "$(dirname "$0")/.." && pwd)
D=${1:-/tmp/ps}
N=$R/tools/rpm-src/x/netpbm-9.5-1/netpbm-9.5
rm -rf "$D" && mkdir -p "$D/sys" && cd "$D"
cp "$N"/pnm/pnmscale.c "$N"/pnm/pnm.h "$N"/ppm/ppm.h "$N"/ppm/libppm.h "$N"/pgm/pgm.h "$N"/pgm/libpgm.h \
   "$N"/pbm/pbm.h "$N"/pbm/libpbm.h "$N"/pbmplus.h "$N"/shhopt/shhopt.h .
cp "$R"/sam-yum-tt/src/pmshim.[ch] "$R"/sam-yum-tt/src/pnmsrun.h "$R"/sam-yum-tt/src/pnmsserv.c \
   "$R"/sam-yum-tt/src/pnmst4test.c "$R"/sam-yum-tt/src/t4serv.[ch] "$R"/sam-yum-tt/src/t4adler.[ch] .
printf '/* t4pnms stub */\ntypedef long off_t;\n' > sys/types.h
for h in unistd.h sys/stat.h fcntl.h; do printf '/* t4pnms stub */\n' > "$h"; done
. "$R/tools/d72uni-env.sh"
ISEARCH="$D/ $D72UNI/libs/"
export ISEARCH IBOARDSIZE
cc_() { icc "$1.c" -t4 -dPPM_PACKCOLORS $2 -o "$1.t4h" > "$1.log" 2>&1 || { cat "$1.log"; exit 1; }
        grep -E '^(Serious|Error|Fatal)' "$1.log" && exit 1; true; }
# Renames in a prepended header (a long -d list overflows icc's command line under t4).
printf '%s\n' '#include <stdlib.h>' '#define malloc ps_malloc' '#define free ps_free' '#define exit ps_exit' \
    '#include "pmshim.h"' > psren.h
printf '#define main pnmscale_main\n#include "psren.h"\n' | cat - pnmscale.c > s.tmp && mv s.tmp pnmscale.c
cc_ pnmscale ""
for f in pmshim pnmsserv pnmst4test t4serv t4adler; do cc_ $f ""; done
printf '%s\n' t4serv.t4h pnmsserv.t4h pmshim.t4h pnmscale.t4h t4adler.t4h '#include startup.lnk' > pnmsserv.lnk
printf '%s\n' pnmst4test.t4h pmshim.t4h pnmscale.t4h '#include startup.lnk' > pnmst4test.lnk
for m in pnmsserv pnmst4test; do
    ilink -f "$m.lnk" -t4 -h -o "$m.c4h"
    icollect "$m.c4h" -t -o "$m.b4h"
    [ -s "$m.b4h" ] || { echo "t4pnms: $m.b4h not built" >&2; exit 1; }
done
ls -l "$D"/pnmsserv.b4h "$D"/pnmst4test.b4h
