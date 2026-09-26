#!/bin/sh
# Build netpbm's ppmquant for the T425 (ppmqserv; tt030-t425-kernel-ports W2) with the INMOS toolset.
#   sam-yum-tt/t4ppmq.sh [DIR]   -> DIR/ppmqserv.b4h (ships as /usr/lib/t425/tppmquant.btl), DIR/ppmqt4test.b4h
# DIR must be SHORT (default /tmp/pq). netpbm 9.5 from the SpareMiNT SRPM, copied, never edited:
#   ppmquant.c  main -> ppmquant_main, exit -> ps_exit, qsort -> ps_qsort (pmshim.c: same code as tppmquant)
#   libppm3.c   the colormap/hash functions, qsort -> ps_qsort
#   PPM_PACKCOLORS  4-byte pixels (board memory is tight; limit unverified, src/t4serv.c)
set -eu
R=$(cd "$(dirname "$0")/.." && pwd)
D=${1:-/tmp/pq}
N=$R/tools/rpm-src/x/netpbm-9.5-1/netpbm-9.5
rm -rf "$D" && mkdir -p "$D/sys" && cd "$D"
cp "$N"/ppm/ppmquant.c "$N"/ppm/libppm3.c "$N"/ppm/ppm.h "$N"/ppm/ppmcmap.h "$N"/ppm/libppm.h "$N"/pgm/pgm.h \
   "$N"/pbm/pbm.h "$N"/pnm/pnm.h "$N"/pbmplus.h "$N"/shhopt/shhopt.h "$N"/pgm/libpgm.h "$N"/pbm/libpbm.h .
cp "$R"/sam-yum-tt/src/pmshim.[ch] "$R"/sam-yum-tt/src/ppmqrun.h "$R"/sam-yum-tt/src/ppmqserv.c "$R"/sam-yum-tt/src/ppmqt4test.c \
   "$R"/sam-yum-tt/src/t4serv.[ch] "$R"/sam-yum-tt/src/t4adler.[ch] .
printf '/* t4ppmq stub */\ntypedef long off_t;\n' > sys/types.h
for h in unistd.h sys/stat.h fcntl.h; do printf '/* t4ppmq stub */\n' > "$h"; done
. "$R/tools/d72uni-env.sh"
ISEARCH="$D/ $D72UNI/libs/"
export ISEARCH IBOARDSIZE
cc_() { icc "$1.c" -t4 -dPPM_PACKCOLORS $2 -o "$1.t4h" > "$1.log" 2>&1 || { cat "$1.log"; exit 1; }
        grep -E '^(Serious|Error|Fatal)' "$1.log" && exit 1; true; }
# The renames live in psren.h, prepended to the COPIES (a long -d list overflows icc's command line under
# t4). Arena (malloc/free -> pmshim.c): every request frees what ppmquant left behind.
printf '%s\n' '#include <stdlib.h>' '#include <time.h>' '#define malloc ps_malloc' '#define free ps_free' \
    '#define qsort ps_qsort' '#define exit ps_exit' '#define time ps_time' '#define getpid ps_getpid' \
    '#define srand ps_srand' '#define rand ps_rand' '#include "pmshim.h"' > psren.h
printf '#define main ppmquant_main\n#include "psren.h"\n' | cat - ppmquant.c > q.tmp && mv q.tmp ppmquant.c
printf '#include "psren.h"\n' | cat - libppm3.c > l.tmp && mv l.tmp libppm3.c
cc_ ppmquant ""
cc_ libppm3 ""
for f in pmshim ppmqserv ppmqt4test t4serv t4adler; do cc_ $f ""; done
printf '%s\n' t4serv.t4h ppmqserv.t4h pmshim.t4h ppmquant.t4h libppm3.t4h t4adler.t4h '#include startup.lnk' > ppmqserv.lnk
printf '%s\n' ppmqt4test.t4h pmshim.t4h ppmquant.t4h libppm3.t4h '#include startup.lnk' > ppmqt4test.lnk
for m in ppmqserv ppmqt4test; do
    ilink -f "$m.lnk" -t4 -h -o "$m.c4h"
    icollect "$m.c4h" -t -o "$m.b4h"
    [ -s "$m.b4h" ] || { echo "t4ppmq: $m.b4h not built" >&2; exit 1; }
done
ls -l "$D"/ppmqserv.b4h "$D"/ppmqt4test.b4h
