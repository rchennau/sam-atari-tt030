/* jpegt4test — jpegserv under t4 vs on the host (tt030-t425-kernel-ports W2): encode a 160x120 RGB
 * pattern (default quality and -quality 90), decode the result; print lengths and checksums. */
#include <stdio.h>
#include <stdlib.h>
#include "t4serv.h"

#define W 160L
#define H 120L

static unsigned long sum(const unsigned char *p, long n)
{
    unsigned long s = 0;
    long i;
    for (i = 0; i < n; i++)
        s = s * 31 + p[i];
    return s & 0xffffffffUL;
}

int main(void)
{
    long n = 9 + W * H * 3, i, x, y, j, d;
    unsigned char *raw = malloc(n), *jpg = malloc(n + 4096), *back = malloc(n + 64);
    int q;

    if (!raw || !jpg || !back)
        return 1;
    raw[0] = (unsigned char)W; raw[1] = (unsigned char)(W >> 8); raw[2] = raw[3] = 0;
    raw[4] = (unsigned char)H; raw[5] = (unsigned char)(H >> 8); raw[6] = raw[7] = 0;
    raw[8] = 3;
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++) {
            i = 9 + (y * W + x) * 3;
            raw[i] = (unsigned char)(x * 255 / W);
            raw[i + 1] = (unsigned char)(y * 255 / H);
            raw[i + 2] = (unsigned char)(((x / 8 + y / 8) & 1) ? 220 : 30);
        }
    for (q = 0; q <= 90; q += 90) {
        j = t4_kernel('C', q, raw, n, jpg, n + 4096);
        d = j > 0 ? t4_kernel('D', 0, jpg, j, back, n + 64) : -1;
        printf("q%d encode %ld %08lx decode %ld %08lx\n", q, j, j > 0 ? sum(jpg, j) : 0UL, d,
               d > 0 ? sum(back, d) : 0UL);
    }
    return 0;
}
