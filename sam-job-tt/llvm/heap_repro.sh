#!/bin/bash
# heap_repro.sh [SEQ] — defect 393e4dcc check under t4: node-0 jobs in root.c's malloc/free order, then a 1.44 MB
# cjpeg 'J'. Every job must answer status 0 (before the fix SEQ 3's second lha job hung in free()). Needs the
# objects build_llvm.sh leaves in /tmp/samjob_llvm. -mmem = root's 5M + the off-heap 1.44 MB source buffer, so
# the heap is the root's size (3,745,444 B in jobllvm.elf; this image is within 300 B of it).
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd); job=$(cd "$here/.." && pwd); root=$(cd "$job/.." && pwd)
L=$root/tools/llvm-t800/bin; w=/tmp/samjob_llvm; o=$(mktemp -d); trap 'rm -rf "$o"' EXIT
SEQ=${1:-3}
OBJS=$(ls $w/*.o | grep -vE '/(root|roots|worker1|worker2|jobexec|bc_all|bc_[a-z0-9]+|lha_lhaserv)\.o$')
$L/clang -target t800 -mcpu=t425 -O2 -w -DSEQ=$SEQ -I"$job" -I"$here" -I"$root/sam-yum-tt/src" -c "$here/heap_repro.c" -o "$o/hr.o"
$L/clang -target t800 -mcpu=t425 -mboard=atw800 -mmem=5627312 -Wl,--wrap=malloc,--wrap=free,--wrap=realloc,--wrap=calloc \
    "$o/hr.o" $OBJS -o "$o/hr.elf"
$L/llvm-objcopy -O binary "$o/hr.elf" "$o/hr.bin"; $L/mkboot "$o/hr.bin" -o "$o/hr.btl"
(cd "$o" && timeout 600 "$L/t4" -s4 -sm 23 -sr -se -ss -sb hr.btl </dev/null > out 2> err) || true
grep -E '^(heap|op|RC)' "$o/out"
want=$(( SEQ == 1 ? 1 : SEQ == 2 ? 2 : 4 ))
[ "$(grep -c -- '-> status 0 ' "$o/out")" = "$want" ] && grep -qx 'RC 0' "$o/out" \
    || { echo "heap_repro: SEQ $SEQ FAILED (want $want status-0 jobs)" >&2; exit 1; }
echo "heap_repro: SEQ $SEQ ok"
