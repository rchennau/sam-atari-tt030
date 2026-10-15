#!/bin/sh
# fr2_mixed_link.sh — PMS tt030-ttjob-ports FR-2: can INMOS-compiled kernels live in an llvm-linked image?
# Converts the icc objects of the compression kernels (/tmp/cb/*.t4h from sam-yum-tt/t4compress.sh) to ELF with
# llvm-t800's tcoff2elf, links them with a clang-built compt4test main (ld.lld --no-relax: converted modules carry
# raw branch offsets), runs it under t4 and compares with a host gcc build. Exit 0 = byte-identical.
# 2026-10-15: IDENTICAL (z/Z/b/r ops). t800-run.sh takes no .o inputs, so a patched copy runs from a temp dir.
set -eu
R=$(cd "$(dirname "$0")/.." && pwd); P=$R/tools/llvm-t800; W=${1:-/tmp/fr2mixed}
K="adler32 crc32 deflate trees zutil inflate inftrees inffast blocksort huffman crctable randtable decompress bzcompress bzlib compkern"
[ -f /tmp/cb/compkern.t4h ] || sh "$R/sam-yum-tt/t4compress.sh" /tmp/cb > /dev/null
rm -rf "$W" && mkdir -p "$W/host" && cd "$W"
for k in $K; do "$P/bin/tcoff2elf" "/tmp/cb/$k.t4h" -o "$k.o" > /dev/null; done
cp /tmp/cb/*.c /tmp/cb/*.h host/ && cp /tmp/cb/compt4test.c /tmp/cb/*.h .
(cd host && gcc -w -DZ_SOLO -DBZ_NO_STDIO -I. -o host compt4test.c $(for k in $K; do printf '%s.c ' "$k"; done) && ./host > ../host.out)
python3 - "$P/share/t800/runtime/t800-run.sh" run-mixed.sh <<'PY'
import sys
s = open(sys.argv[1]).read()
for a, b in (('T800_RT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)',
              'T800_RT=${T800_RT:-$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)}'),
             ('\t*.c|*.cc|*.cpp|*.ll|*.s) inputs="$inputs $1" ;;',
              '\t*.c|*.cc|*.cpp|*.ll|*.s) inputs="$inputs $1" ;;\n\t*.o) preobjs="${preobjs:-} $1" ;;'),
             ('[ "$inmos" = 1 ] && ldflags="--no-relax"',
              '[ "$inmos" = 1 ] && ldflags="--no-relax"\n[ -n "${preobjs:-}" ] && ldflags="--no-relax"; objs="$objs ${preobjs:-}"')):
    assert a in s, a
    s = s.replace(a, b, 1)
open(sys.argv[2], "w").write(s)
PY
T800_RT=$P/share/t800/runtime LLVM_BIN=$P/bin T4=$P/bin/t4 MKBOOT=$P/bin/mkboot T800_T4_FLAGS='-sm 23' \
    sh run-mixed.sh -mcpu=t425 -ext -O2 -lc -w --defsym=__ram_end=0x80500000 -DZ_SOLO -DBZ_NO_STDIO -I. \
    compt4test.c $(for k in $K; do printf '%s.o ' "$k"; done) -o mixed > mixed.log 2>&1
grep -v '^RESULT \|^T4UNINIT' mixed.log > mixed.out
cmp mixed.out host.out && echo "fr2_mixed_link: IDENTICAL ($(wc -l < host.out) ops)"
