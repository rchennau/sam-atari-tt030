#!/bin/sh
# Build lha's encoder for the T425 (lhaserv; tt030-t425-kernel-ports W1) with the INMOS toolset.
#   sam-yum-tt/t4lha.sh [DIR]   -> DIR/lhaserv.b4h  (ships as /usr/lib/t425/lha.btl)
# DIR must be SHORT (ilink's ISEARCH limit; default /tmp/lh). Copies, never edits, the SpareMiNT source:
#   lha.h      one inserted line after <stdio.h>: #include "lhamemio.h" (memory streams, lhamemio.h)
#   lha_macro.h drop `extern int errno;` (icc's errno is a macro; its redeclaration is a type clash)
#   stubs      sys/time.h, sys/dir.h, dirent.h, sys/file.h, sys/stat.h (unused by the encoder)
# The same -D switches as the TT build (no SUPPORT_LH7: it changes the default method and lh5's window),
# minus the incremental indicator (no terminal here).
set -eu
R=$(cd "$(dirname "$0")/.." && pwd)
D=${1:-/tmp/lh}
L=$R/tools/rpm-src/x/lha-1.14i-1/lha-114i/src
rm -rf "$D" && mkdir -p "$D/sys" && cd "$D"
cp "$L"/slide.c "$L"/huf.c "$L"/crcio.c "$L"/maketree.c "$L"/shuf.c "$L"/dhuf.c "$L"/maketbl.c \
   "$L"/larc.c "$L"/lha.h "$L"/lha_macro.h "$L"/lhdir.h .
cp "$R"/sam-yum-tt/src/lhaserv.c "$R"/sam-yum-tt/src/lhat4test.c "$R"/sam-yum-tt/src/lhamemio.h "$R"/sam-yum-tt/src/t4serv.[ch] \
   "$R"/sam-yum-tt/src/t4adler.[ch] .
python3 - <<'PY'
s = open("lha.h", encoding="latin-1").read()
a = "#include <stdio.h>\n"
assert a in s, "lha.h: <stdio.h> include not found"
open("lha.h", "w", encoding="latin-1").write(s.replace(a, a + '#include "lhamemio.h"\n', 1))
import re
m = open("lha_macro.h", encoding="latin-1").read()
m2 = re.sub(r"(?m)^\s*extern\s+int\s+errno\s*;\s*$", "/* t4lha: errno comes from <errno.h> */", m)
assert m2 != m, "lha_macro.h: extern int errno not found"
open("lha_macro.h", "w", encoding="latin-1").write(m2)
PY
for h in sys/time.h sys/dir.h dirent.h sys/file.h sys/stat.h; do printf '/* t4lha stub */\n' > "$h"; done
printf '/* t4lha stub */\n#include <time.h>\ntypedef long off_t;\ntypedef unsigned short mode_t;\ntypedef int uid_t;\ntypedef int gid_t;\n' > sys/types.h
. "$R/tools/d72uni-env.sh"
ISEARCH="$D/ $D72UNI/libs/"
export ISEARCH IBOARDSIZE
DEFS="-dEUC -dSYSV_SYSTEM_DIR -dTZSET -dSUPPORT_LH6"   # = the TT build (scripts/build_t425_lha.sh)
for f in slide huf crcio maketree shuf dhuf maketbl larc lhaserv lhat4test t4serv t4adler; do
    icc "$f.c" -t4 $DEFS -o "$f.t4h" > "$f.log" 2>&1 || { cat "$f.log"; exit 1; }
    grep -E '^(Serious|Error|Fatal)' "$f.log" && exit 1
done
printf '%s\n' t4serv.t4h lhaserv.t4h slide.t4h huf.t4h crcio.t4h maketree.t4h shuf.t4h dhuf.t4h maketbl.t4h larc.t4h t4adler.t4h '#include startup.lnk' > lhaserv.lnk
ilink -f lhaserv.lnk -t4 -h -o lhaserv.c4h
icollect lhaserv.c4h -t -o lhaserv.b4h
[ -s lhaserv.b4h ] || { echo "t4lha: lhaserv.b4h not built" >&2; exit 1; }
# the same kernel with a test main instead of the server loop (checked against a host build)
printf '%s\n' lhat4test.t4h lhaserv.t4h slide.t4h huf.t4h crcio.t4h maketree.t4h shuf.t4h dhuf.t4h maketbl.t4h larc.t4h '#include startup.lnk' > lhat4test.lnk
ilink -f lhat4test.lnk -t4 -h -o lhat4test.c4h
icollect lhat4test.c4h -t -o lhat4test.b4h
ls -l "$D"/lhaserv.b4h "$D"/lhat4test.b4h
