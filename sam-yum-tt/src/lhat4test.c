/* lhat4test — lha kernel (lhaserv.c) under t4 vs on the host: same input, the output length, crc and a
 * checksum must match (tt030-t425-kernel-ports W1). Input: 64 KB of text-like pattern. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "t4serv.h"

#define N 65536L

int main(void)
{
    static const char ops[] = "56";
    unsigned char *in = malloc(N), *out = malloc(N + 1024), *pk = malloc(N + 1024), *back = malloc(N + 1024);
    long i, len;
    unsigned long sum;
    int k;

    if (!in || !out) {
        printf("out of memory\n");
        return 1;
    }
    for (i = 0; i < N; i++)
        in[i] = (unsigned char)("the quick brown fox jumps over a lazy dog\n"[(i * 7 + i / 97) % 42]);
    for (k = 0; ops[k]; k++) {
        len = t4_kernel(ops[k], 0, in, N, out, N + 1024);
        for (sum = 0, i = 0; i < len; i++)
            sum = sum * 31 + out[i];
        printf("lh%c %ld crc %02x%02x %08lx\n", ops[k], len, len > 1 ? out[1] : 0, len > 0 ? out[0] : 0,
               sum & 0xffffffffUL);
    }
    /* decode round trip: op 'd' on lh5 / lh6 output must give back the input and the same crc */
    for (k = 0; ops[k]; k++) {
        long plen = t4_kernel(ops[k], 0, in, N, out, N + 1024), blen;
        pk[0] = (unsigned char)N;
        pk[1] = (unsigned char)(N >> 8);
        pk[2] = (unsigned char)(N >> 16);
        pk[3] = (unsigned char)(N >> 24);
        memcpy(pk + 4, out + 2, (size_t)(plen - 2));
        blen = t4_kernel('d', ops[k] - '0', pk, plen + 2, back, N + 1024);
        printf("lh%c decode %ld %s crc %s\n", ops[k], blen,
               blen == N + 2 && !memcmp(back + 2, in, N) ? "data-ok" : "DATA-BAD",
               blen > 1 && back[0] == out[0] && back[1] == out[1] ? "same" : "DIFFERS");
    }
    return 0;
}
