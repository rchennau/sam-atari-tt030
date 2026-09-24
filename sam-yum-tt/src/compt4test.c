/* compt4test — prove the T425 build of compkern equals the host build: compress a fixed pattern with
 * each op and print length + a checksum of the output. Run the same file under t4 and on the host;
 * the two outputs must be identical. */
#include <stdio.h>
#include <stdlib.h>
#include "compkern.h"

#define N 65536L

int main(void)
{
    static const char ops[] = "zZb";
    unsigned char *in = malloc(N), *out = malloc(N + N / 2 + 1024);
    long i, len;
    unsigned long sum;
    int k;

    if (!in || !out) {
        printf("out of memory\n");
        return 1;
    }
    for (i = 0; i < N; i++)                     /* text-like: words from a small alphabet, some noise */
        in[i] = (unsigned char)("the quick brown fox jumps over a lazy dog\n"[(i * 7 + i / 97) % 42]);
    for (k = 0; ops[k]; k++) {
        len = ck_compress(ops[k], in, N, out, N + N / 2 + 1024);
        for (sum = 0, i = 0; i < len; i++)
            sum = sum * 31 + out[i];
        printf("%c %ld %08lx\n", ops[k], len, sum & 0xffffffffUL);
    }
    return 0;
}
