#!/bin/sh
# Build bc 1.05 for the T425 (bcserv; tt030-t425-kernel-ports W3) with the INMOS toolset.
#   sam-yum-tt/t4bc.sh [DIR] [--copies]  -> DIR/bcserv.b4h (ships as /usr/lib/t425/tbc.btl), DIR/bct4test.b4h
#   --copies stops after writing the patched sources (the 68030 build and the host check compile those).
# DIR must be SHORT (default /tmp/bq). bc from the SpareMiNT SRPM, copied, never edited: each source gets
# bcren.h prepended (bcshim.h's system headers first, then stdio/signal/getenv/exit/read -> bs_*), main.c
# also main -> bc_main; scan.c casts flex's unsigned yytext for icc. config.h is written here (no configure): ANSI headers, vprintf, no readline, no
# BC_MATH_FILE (libmath is compiled in).
set -eu
R=$(cd "$(dirname "$0")/.." && pwd)
D=${1:-/tmp/bq}
B=$R/tools/rpm-src/x/bc-1.05a-2/bc-1.05
rm -rf "$D" && mkdir -p "$D/sys" && cd "$D"
cp "$B"/bc/bc.c "$B"/bc/scan.c "$B"/bc/execute.c "$B"/bc/global.c "$B"/bc/load.c "$B"/bc/main.c \
   "$B"/bc/storage.c "$B"/bc/util.c "$B"/bc/bc.h "$B"/bc/libmath.h "$B"/lib/number.c "$B"/lib/getopt.c \
   "$B"/lib/getopt1.c "$B"/h/*.h .
cp "$R"/sam-yum-tt/src/bcshim.[ch] "$R"/sam-yum-tt/src/bcserv.c "$R"/sam-yum-tt/src/bct4test.c \
   "$R"/sam-yum-tt/src/t4serv.[ch] "$R"/sam-yum-tt/src/t4adler.[ch] .
printf '%s\n' '#define STDC_HEADERS 1' '#define HAVE_VPRINTF 1' '#define HAVE_ISGRAPH 1' '#define HAVE_LIMITS_H 1' \
    '#define HAVE_STDARG_H 1' '#define HAVE_STDDEF_H 1' '#define HAVE_STDLIB_H 1' '#define HAVE_STRING_H 1' \
    '#define PACKAGE "bc"' '#define VERSION "1.05"' > config.h
printf '/* t4bc stub */\ntypedef long off_t;\n' > sys/types.h
{ echo '#include "bcshim.h"'
  for n in printf fprintf vfprintf putchar putc puts fputs fflush fwrite getchar read fileno isatty signal \
           exit getenv fopen fclose; do printf '#undef %s\n#define %s bs_%s\n' $n $n $n; done
  # bison grows its parser stack with alloca, which icc lacks; malloc leaks only on very deep parses, and
  # every process runs bc once. ponytail: an arena if tbc ever serves several programs per boot.
  printf '#undef alloca\n#define alloca malloc\n'; } > bcren.h
for f in bc scan execute global load main storage util number getopt getopt1; do
    { [ $f = main ] && echo '#define main bc_main'; echo '#include "bcren.h"'; cat $f.c; } > $f.tmp && mv $f.tmp $f.c
done
# flex's yytext is unsigned char *; icc refuses it where bc passes it as char * (6 sites in scan.l)
python3 - <<'PY2'
s = open("scan.c").read()
a = '#include "proto.h"\n'
assert a in s, "scan.c: proto.h include not found"
open("scan.c", "w").write(s.replace(a, a + '#define strcopyof(s) strcopyof((char *)(s))\n#define strlen(s) strlen((char *)(s))\n', 1))
PY2
[ "${2:-}" = --copies ] && exit 0
. "$R/tools/d72uni-env.sh"
ISEARCH="$D/ $D72UNI/libs/"
export ISEARCH IBOARDSIZE
LIB="bc scan execute global load main storage util number getopt getopt1 bcshim"
for f in $LIB t4serv t4adler bcserv bct4test; do
    icc "$f.c" -t4 -o "$f.t4h" > "$f.log" 2>&1 || { cat "$f.log"; exit 1; }
    grep -E '^(Serious|Error|Fatal)' "$f.log" && exit 1
done
for m in bcserv bct4test; do
    extra=; [ "$m" = bcserv ] && extra="t4serv.t4h t4adler.t4h"
    { echo "$m.t4h"; [ -n "$extra" ] && printf "%s\n" $extra; for f in $LIB; do echo "$f.t4h"; done; echo '#include startup.lnk'; } > "$m.lnk"
    ilink -f "$m.lnk" -t4 -h -o "$m.c4h"
    icollect "$m.c4h" -t -o "$m.b4h"
    [ -s "$m.b4h" ] || { echo "t4bc: $m.b4h not built" >&2; exit 1; }
done
ls -l "$D"/bcserv.b4h "$D"/bct4test.b4h
