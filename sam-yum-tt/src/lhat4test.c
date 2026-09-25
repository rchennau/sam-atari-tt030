/* lhat4test — lha kernel (lhaserv.c) under t4 vs on the host: same input, the output length, crc and a
 * checksum must match (tt030-t425-kernel-ports W1). Input: 64 KB of text-like pattern. */
#include <stdio.h>
#include <stdlib.h>
#include "t4serv.h"

#define N 65536L

int main(void)
{
    static const char ops[] = "56";
    unsigned char *in = malloc(N), *out = malloc(t4_out_cap(N));
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
        len = t4_kernel(ops[k], 0, in, N, out, t4_out_cap(N));
        for (sum = 0, i = 0; i < len; i++)
            sum = sum * 31 + out[i];
        printf("lh%c %ld crc %02x%02x %08lx\n", ops[k], len, len > 1 ? out[1] : 0, len > 0 ? out[0] : 0,
               sum & 0xffffffffUL);
    }
    return 0;
}
