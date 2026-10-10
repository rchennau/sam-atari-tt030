#!/bin/sh
# Build a T425 port's kernel with llvm-t800 and check it before any TT time (PMS tt030-llvm-t800-adoption FR-5).
#   sam-yum-tt/t4clang.sh PORT [DIR]     PORT = lha | jpeg | ppmq | png
# 1. sam-yum-tt/t4PORT.sh prepares the sources in DIR exactly as the icc build uses them (and builds the icc images).
# 2. The port's t4 test main is built for the host (gcc) and with clang under t800-run (t4 emulator, 5 MB RAM as
#    on the ATW800/2: t800-run's default is 256 KB, friction F9); the outputs must be identical.
# 3. The server image is built with clang at -O2 and -Os -> DIR/clang/PORTserv-O2.btl, -Os.btl, each confined to
#    5 MB (NFR-2). These are the FR-5 candidates; the decision is the TT timing gate, never the emulator (F7).
set -eu
R=$(cd "$(dirname "$0")/.." && pwd)
PORT=$1
case $PORT in
lha)  D=${2:-/tmp/lh}; STD=-std=gnu89; DEFS="-DEUC -DSYSV_SYSTEM_DIR -DTZSET -DSUPPORT_LH6"
      LIB="lhaserv slide huf crcio maketree shuf dhuf maketbl larc"; TEST=lhat4test; SERV="t4serv t4adler" ;;
jpeg) D=${2:-/tmp/jp}; STD=-std=gnu89; DEFS=-DNO_GETENV
      LIB="jpegserv $(sed -n '/^LIB="/,/"$/p' "$R/sam-yum-tt/t4jpeg.sh" | tr -d '"' | sed 's/^LIB=//' | tr '\n' ' ')"
      TEST=jpegt4test; SERV="t4serv t4adler" ;;
ppmq) D=${2:-/tmp/pq}; STD=-std=gnu89; DEFS=-DPPM_PACKCOLORS
      LIB="pmshim ppmquant libppm3"; TEST=ppmqt4test; SERV="ppmqserv t4serv t4adler" ;;
png)  D=${2:-/tmp/pg}; STD=; DEFS="-DZ_SOLO -Dz_off_t=long"
      LIB="adler32 crc32 deflate trees zutil pngenc"; TEST=pngt4test; SERV="pngserv t4serv t4adler" ;;
*)    echo "usage: t4clang.sh lha|jpeg|ppmq|png [DIR]" >&2; exit 2 ;;
esac
sh "$R/sam-yum-tt/t4$PORT.sh" "$D" > /dev/null
C=$D/clang && rm -rf "$C" && mkdir -p "$C"
cp "$D"/*.c "$D"/*.h "$C"/
[ -d "$D/sys" ] && cp -r "$D/sys" "$C"/
rm -f "$C/sys/types.h"            # an icc stub; it shadows libt800's own (pid_t; friction F10)
cd "$C"
src() { for f in "$@"; do printf '%s.c ' "$f"; done; }
H=$C/host && mkdir -p "$H" && cp ./*.c ./*.h "$H"/ && rm -f "$H/dirent.h"   # host: no stubs at all
(cd "$H" && gcc -w $STD $DEFS -I. -o host_test $(src $TEST $LIB) -lm && ./host_test > host.out)
LL=$R/tools/llvm-t800/bin
PATH=$LL:$PATH t800-run -mcpu=t425 -ext -O2 -lc $STD -w --defsym=__ram_end=0x80500000 $DEFS -I. \
    $(src $TEST $LIB) -o clang_test > clang.log 2>&1 || { tail -20 clang.log; exit 1; }
grep -v '^RESULT ' clang.log | grep -v '^T4UNINIT' > clang.out    # t4's own lines (friction F6)
if ! cmp -s clang.out "$H/host.out"; then
    echo "t4clang $PORT: clang under t4 differs from the host build" >&2; diff clang.out "$H/host.out" | head >&2; exit 1
fi
for o in O2 Os; do
    PATH=$LL:$PATH t800-cc -mcpu=t425 -mboard=atw800 -mmem=5M -mhost=none -$o $STD -w $DEFS -I. \
        -o "${PORT}serv-$o" $(src $SERV $LIB) > "cc-$o.log" 2>&1 || { tail -20 "cc-$o.log"; exit 1; }
    "$LL/llvm-nm" "${PORT}serv-$o.elf" | grep -q '80500000 A __ram_end' \
        || { echo "t4clang $PORT: ${PORT}serv-$o not confined to 5 MB (NFR-2)" >&2; exit 1; }
done
echo "t4clang $PORT: clang == host ($(wc -l < clang.out) lines)"
ls -l "$C/${PORT}serv-O2.btl" "$C/${PORT}serv-Os.btl" "$D/${PORT}serv.b4h"
