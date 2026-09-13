/* x25519bench — kanban a764de13 step 1: time one X25519 scalar multiply with the SAME C code on the
 * TT's 68030 (gcc) and the ATW800/2's T425 (INMOS icc, no 64-bit type). Uses Daniel Beer's public-domain
 * c25519 (8-bit digits, 32-bit accumulators), because icc cannot build Monocypher's i64 field math.
 * Checks the result against RFC 7748 §6.1 (Alice), computed independently on fractal.
 *
 * Usage: bench [reps]      (default 1)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "c25519.h"

static const uint8_t alice_sk[32] = {
    0x77, 0x07, 0x6d, 0x0a, 0x73, 0x18, 0xa5, 0x7d, 0x3c, 0x16, 0xc1, 0x72, 0x51, 0xb2, 0x66, 0x45,
    0xdf, 0x4c, 0x2f, 0x87, 0xeb, 0xc0, 0x99, 0x2a, 0xb1, 0x77, 0xfb, 0xa5, 0x1d, 0xb9, 0x2c, 0x2a };
static const uint8_t alice_pk[32] = {
    0x85, 0x20, 0xf0, 0x09, 0x89, 0x30, 0xa7, 0x54, 0x74, 0x8b, 0x7d, 0xdc, 0xb4, 0x3e, 0xf7, 0x5a,
    0x0d, 0xbf, 0x3a, 0x0d, 0x26, 0x38, 0x1a, 0xf4, 0xeb, 0xa4, 0xa9, 0x8e, 0xaa, 0x9b, 0x4e, 0x6a };

int main(int argc, char **argv)
{
    int reps = argc > 1 ? atoi(argv[1]) : 1, i;
    uint8_t e[32], out[32];
    clock_t t0, t1;

    memcpy(e, alice_sk, 32);
    e[0] &= 248;                       /* RFC 7748 clamping */
    e[31] &= 127;
    e[31] |= 64;
    t0 = clock();
    for (i = 0; i < reps; i++)
        c25519_smult(out, c25519_base_x, e);
    t1 = clock();
    printf("x25519 c25519: %d rep(s), %ld ticks, CLOCKS_PER_SEC %ld, %ld ms per op, result %s\n",
           reps, (long)(t1 - t0), (long)CLOCKS_PER_SEC,
           (long)((double)(t1 - t0) * 1000.0 / CLOCKS_PER_SEC / reps),
           memcmp(out, alice_pk, 32) ? "WRONG" : "matches RFC 7748");
    return 0;
}
