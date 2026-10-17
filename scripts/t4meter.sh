#!/bin/bash
# t4meter.sh PORT OP LEVEL PAYLOAD [SAVE] — peak heap and stack of a T425 kernel, measured in the t4 EMULATOR
# (PMS tt030-ttjob-ports FR-1 / NFR-4). Builds the port's llvm kernel with sam-yum-tt/src/t4meter.c in place of
# t4serv.c, runs it once on PAYLOAD and prints t4meter's line. Emulator only: see t4meter.c for why it must never
# run on the card. PORT = bc | lha | jpeg | ppmq | png. Sources come from the same scripts the shipped images use
# (t4clang.sh for lha/jpeg/ppmq/png, t4bc.sh for bc); a prepared tree is reused.
. "$(dirname "$0")/tt_log.sh"
set -euo pipefail
R=$(cd "$(dirname "$0")/.." && pwd)
PORT=$1 OP=$2 LEVEL=$3 PAY=$(readlink -f "$4") SAVE=${5:-}
LL=$R/tools/llvm-t800/bin
case $PORT in
bc)   C=/tmp/bqm; STD=-std=gnu89; DEFS=-Dwarn=bc_warn
      SRC="bcserv t4meter t4adler bc scan execute global load main storage util number getopt getopt1 bcshim"
      [ -f "$C/bc.c" ] || { rm -rf "$C"; sh "$R/sam-yum-tt/t4bc.sh" "$C" --copies > /dev/null; rm -rf "$C/sys"; } ;;
lha)  C=/tmp/lh/clang; STD=-std=gnu89; DEFS="-DEUC -DSYSV_SYSTEM_DIR -DTZSET -DSUPPORT_LH6"
      SRC="t4meter t4adler lhaserv slide huf crcio maketree shuf dhuf maketbl larc" ;;
jpeg) C=/tmp/jp/clang; STD=-std=gnu89; DEFS=-DNO_GETENV
      SRC="t4meter t4adler jpegserv $(sed -n '/^LIB="/,/"$/p' "$R/sam-yum-tt/t4jpeg.sh" | tr -d '"' | sed 's/^LIB=//' | tr '\n' ' ')" ;;
ppmq) C=/tmp/pq/clang; STD=-std=gnu89; DEFS=-DPPM_PACKCOLORS
      SRC="ppmqserv t4meter t4adler pmshim ppmquant libppm3" ;;
png)  C=/tmp/pg/clang; STD=; DEFS="-DZ_SOLO -Dz_off_t=long"
      SRC="pngserv t4meter t4adler adler32 crc32 deflate trees zutil pngenc" ;;
*)    echo "usage: t4meter.sh bc|lha|jpeg|ppmq|png OP LEVEL PAYLOAD" >&2; exit 2 ;;
esac
[ "$PORT" = bc ] || [ -d "$C" ] || sh "$R/sam-yum-tt/t4clang.sh" "$PORT" > /dev/null
cp "$R/sam-yum-tt/src/t4meter.c" "$C"/
cd "$C"
PATH=$LL:$PATH t800-cc -mcpu=t425 -mboard=atw800 -mmem=5M -Os $STD -w $DEFS -I. -o meter \
    $(for f in $SRC; do printf '%s.c ' "$f"; done) > meter-cc.log 2>&1 || { tail -20 meter-cc.log; exit 1; }
printf '%s %s %s %s\n' "$OP" "$LEVEL" "$PAY" "$SAVE" > meter.in
timeout 3600 "$LL/t4" -sm 23 -ss -se -sb meter.btl 2>&1 | tr -d '\r' | grep -E '^meter|EMU|rror' || true
