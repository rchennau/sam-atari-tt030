/* ppmqt4test — ppmquant through pmshim under t4 vs on the host (tt030-t425-kernel-ports W2): a 200x150
 * gradient + pattern image quantized to 64 and 256 colors, in place; prints length and checksum. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ppmqrun.h"

#define W 200L
#define H 150L

int main(void)
{
    static const int nc[2] = { 64, 256 };
    long hdr, n, i, x, y, r;
    unsigned long s;
    unsigned char *b = malloc(64 + W * H * 3);
    int k;
    if (!b)
        return 1;
    for (k = 0; k < 2; k++) {
        hdr = sprintf((char *)b, "P6\n%ld %ld\n255\n", W, H);
        for (y = 0; y < H; y++)
            for (x = 0; x < W; x++) {
                i = hdr + (y * W + x) * 3;
                b[i] = (unsigned char)(x * 255 / W);
                b[i + 1] = (unsigned char)(y * 255 / H);
                b[i + 2] = (unsigned char)((x * 7 + y * 13 + (x * y) % 17) & 255);
            }
        n = hdr + W * H * 3;
        r = pq_run(nc[k], b, n, b, n);
        for (s = 0, i = 0; i < r; i++)
            s = s * 31 + b[i];
        printf("colors %d -> %ld %08lx\n", nc[k], r, s & 0xffffffffUL);
    }
    return 0;
}
