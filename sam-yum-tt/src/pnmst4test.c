/* pnmst4test — pnmscale through pmshim under t4 vs on the host (tt030-t425-kernel-ports W2): a 200x150
 * gradient + pattern image, P6 and P5, down, up and -xysize; prints length and checksum. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pnmsrun.h"

#define W 200L
#define H 150L

int main(void)
{
    static const long req[5][3] = { { 0, 77, 0 }, { 0, 0, 311 }, { 0, 123, 45 }, { 1, 90, 90 }, { 1, 400, 100 } };
    long hdr, n, i, x, y, r, cap, ch;
    unsigned long s;
    unsigned char *b = malloc(64 + W * H * 3), *o;
    int k, p6;
    if (!b)
        return 1;
    for (p6 = 0; p6 < 2; p6++) {
        ch = p6 ? 3 : 1;
        hdr = sprintf((char *)b, "P%c\n%ld %ld\n255\n", p6 ? '6' : '5', W, H);
        for (y = 0; y < H; y++)
            for (x = 0; x < W; x++)
                for (i = 0; i < ch; i++)
                    b[hdr + (y * W + x) * ch + i] = (unsigned char)((x * (i + 3) + y * (5 - i) + (x * y) % 17) & 255);
        n = hdr + W * H * ch;
        for (k = 0; k < 5; k++) {
            cap = pn_cap((int)req[k][0], req[k][1], req[k][2], b, n);
            if (cap < 0 || !(o = malloc(cap)))
                return 1;
            r = pn_run((int)req[k][0], req[k][1], req[k][2], b, n, o, cap);
            for (s = 0, i = 0; i < r; i++)
                s = s * 31 + o[i];
            printf("P%d mode %ld %ldx%ld -> %ld %08lx\n", p6 ? 6 : 5, req[k][0], req[k][1], req[k][2], r,
                   s & 0xffffffffUL);
            free(o);
        }
    }
    return 0;
}
