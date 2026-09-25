/* pngt4test — pngenc under t4 vs on the host (tt030-t425-kernel-ports W2): a 200x150 gradient + pattern
 * image as P6 and P5, zlib levels 6 and 9; prints length and checksum. With an argument (host only) also
 * writes t6_<level>.png / t5_<level>.png into that directory for an independent decoder to check. */
#include <stdio.h>
#include <stdlib.h>
#include "pngenc.h"

#define W 200L
#define H 150L

int main(int argc, char **argv)
{
    static const int lv[2] = { 6, 9 };
    long hdr, n, i, x, y, r, cap, ch;
    unsigned long s;
    unsigned char *b = malloc(64 + W * H * 3), *o;
    int k, p6;
    if (!b)
        return 1;
    for (p6 = 1; p6 >= 0; p6--) {
        ch = p6 ? 3 : 1;
        hdr = sprintf((char *)b, "P%c\n%ld %ld\n255\n", p6 ? '6' : '5', W, H);
        for (y = 0; y < H; y++)
            for (x = 0; x < W; x++)
                for (i = 0; i < ch; i++)
                    b[hdr + (y * W + x) * ch + i] =
                        (unsigned char)((x * (i + 3) + y * (5 - i) + (x * y) % 17 + ((x ^ y) & 8) * 9) & 255);
        n = hdr + W * H * ch;
        for (k = 0; k < 2; k++) {
            cap = pe_cap(b, n);
            if (cap < 0 || !(o = malloc(cap)))
                return 1;
            r = pe_encode(b, n, lv[k], o, cap);
            for (s = 0, i = 0; i < r; i++)
                s = s * 31 + o[i];
            printf("P%d level %d -> %ld %08lx\n", p6 ? 6 : 5, lv[k], r, s & 0xffffffffUL);
            if (argc > 1 && r > 0) {
                char fn[256];
                FILE *f;
                sprintf(fn, "%s/t%d_%d.png", argv[1], p6 ? 6 : 5, lv[k]);
                if ((f = fopen(fn, "wb")) != NULL) {
                    fwrite(o, 1, (size_t)r, f);
                    fclose(f);
                }
            }
            free(o);
        }
    }
    return 0;
}
