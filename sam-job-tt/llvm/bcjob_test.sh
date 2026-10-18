#!/bin/bash
# bcjob_test.sh — op 'c' (GNU bc) in the resident image behaves as a fresh boot for every job (ttjob-ports FR-3).
# Host reference: each program in its own process (bs_run, no BS_TRACK). T4: the image's bc.o + bcjob.c under the t4
# emulator, all programs in ONE process, then LOOP rounds; outputs must match the host.
# Needs build_llvm.sh run first (it makes /tmp/samjob_llvm/bc.o and /tmp/bqj). Exit 0 = pass.
. "$(cd "$(dirname "$0")" && git rev-parse --show-toplevel)/scripts/tt_log.sh"   # log every run for SAM (operator rule 2026-10-13)
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
BQ=/tmp/bqj; W=/tmp/bcjob_test; LOOP=${LOOP:-50}
[ -f /tmp/samjob_llvm/bc.o ] || { echo "bcjob_test: run sam-job-tt/build_llvm.sh first" >&2; exit 2; }
rm -rf "$W" && mkdir -p "$W"
# host only: glibc defines __GNU_LIBRARY__, so bc's getopt.h prototype clashes with proto.h; use a patched copy
cp -r "$BQ" "$W/src" && sed -i 's/^extern int getopt (int argc, char \*const \*argv, const char \*shortopts);/extern int getopt ();/' "$W/src/getopt.h"
(cd "$W/src" && gcc -O1 -w -std=gnu89 -Dwarn=bc_warn -DHOST -I. -o "$W/host" "$here/bcjob_test.c" bc.c scan.c execute.c \
    global.c load.c main.c storage.c util.c number.c getopt.c getopt1.c bcshim.c)
for i in 0 1 2 3 4 5 6; do "$W/host" $i; done > "$W/host.out"
# t800-run takes sources only: a patched copy also links .o inputs (bc.o) and keeps bc_data/bc_bss under --gc-sections
L=$root/tools/llvm-t800
sed -e "s|^T800_RT=.*|T800_RT=$L/share/t800/runtime|" -e 's|^\t\*\.c|\t*.o) xobjs="$xobjs $1" ;;\n\t*.c|' -e 's|\$gc \$ldflags -o "\$elf" "\$crt0_o" \$startup_o \$objs|$gc -z nostart-stop-gc $ldflags -o "$elf" "$crt0_o" $startup_o $objs $xobjs|' \
    "$L/share/t800/runtime/t800-run.sh" > "$W/t800-run.sh"
grep -q 'xobjs="' "$W/t800-run.sh" && grep -q 'objs \$xobjs' "$W/t800-run.sh" || { echo "bcjob_test: t800-run patch did not apply" >&2; exit 2; }
T800_T4_FLAGS='-sm 23' T800_T4_TIMEOUT=${T800_T4_TIMEOUT:-1200} LLVM_BIN=$L/bin T4=$L/bin/t4 MKBOOT=$L/bin/mkboot \
    sh "$W/t800-run.sh" -mcpu=t425 -ext -O2 -lc -w \
    --defsym=__ram_end=0x80500000 --defsym=__stack_headroom=0x20000 -DLOOP="$LOOP" \
    "$here/bcjob_test.c" "$here/bcjob.c" /tmp/samjob_llvm/bc.o -o "$W/t4" | tr -d '\r' > "$W/t4.all"
grep '^prog ' "$W/t4.all" | head -7 > "$W/t4.out"
grep '^prog ' "$W/t4.all" | tail -1 > "$W/t4.after"
grep '^prog 2 ' "$W/host.out" > "$W/host.after"
if diff -u "$W/host.out" "$W/t4.out" && diff -u "$W/host.after" "$W/t4.after" && grep -q '^rounds ' "$W/t4.all"; then
    echo "bcjob_test: PASS (7 programs match fresh host runs; still fresh after $LOOP rounds)"
else
    echo "bcjob_test: FAIL" >&2; grep -v '^prog ' "$W/t4.all" >&2 || true; exit 1
fi
