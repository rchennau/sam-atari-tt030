#!/bin/bash
# Build tlsbench (src/tlsbench/tlsbench.c) for the TT: Mbed TLS 3.6.6 (tools/dl-mbedtls, release tarball) with the
# MiNT user config and the X25519 hook patch, plus libttjob. HighWire transputer evaluation 2026-10-17.
. "$(dirname "$0")/tt_log.sh"   # log every run for SAM (operator rule 2026-10-13)
set -euo pipefail
cd "$(dirname "$0")/.."
R=$PWD; M=tools/mbedtls-3.6.6
export PATH=$R/tools/cross-mint/usr/bin:$PATH
if [ ! -f $M/.tlsbench-patched ]; then
    rm -rf $M; tar xjf tools/dl-mbedtls/mbedtls-3.6.6.tar.bz2 -C tools
    patch -d $M -p1 < src/tlsbench/ecp-hook.patch && touch $M/.tlsbench-patched
fi
CF="-m68020-60 -O2 -I$R/src/tlsbench -DMBEDTLS_USER_CONFIG_FILE='\"mbedtls_mint_config.h\"'"
make -C $M -j16 CC=m68k-atari-mint-gcc AR=m68k-atari-mint-ar CFLAGS="$CF" lib >/dev/null
m68k-atari-mint-gcc -m68020-60 -O2 -s -Wall -I$R/src/tlsbench -I$M/include -Isam-job-tt -Isam-yum-tt/src \
    -DMBEDTLS_USER_CONFIG_FILE="\"mbedtls_mint_config.h\"" -o build/tlsbench.ttp \
    src/tlsbench/tlsbench.c sam-job-tt/libttjob.c sam-yum-tt/src/t4adler.c -L$M/library -lmbedtls -lmbedx509 -lmbedcrypto
ls -l build/tlsbench.ttp
