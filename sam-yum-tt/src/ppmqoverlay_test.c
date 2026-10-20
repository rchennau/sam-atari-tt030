/* ppmqoverlay_test — host check for pmshim's ps_overlay (defect 4ab13d11): `ppmquant N` run in one block laid out as
 * the root image lays out op 'q' (out, gap, input) must give the same bytes as separate buffers, without mallocing the
 * pixel array; with too small a gap it must fall back to malloc and still be right.
 *   sam-yum-tt/t4ppmq.sh /tmp/pq; cd /tmp/pq && gcc -std=gnu89 -DPPM_PACKCOLORS -w -I. -Wl,--wrap=malloc \
 *     -o ovt <repo>/sam-yum-tt/src/ppmqoverlay_test.c pmshim.c ppmquant.c libppm3.c -lm && ./ovt photo.ppm 256 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ppm.h"
#include "ppmqrun.h"

static size_t biggest;
void *__real_malloc(size_t n);
void *__wrap_malloc(size_t n) { if (n > biggest) biggest = n; return __real_malloc(n); }

int main(int argc, char **argv)
{
    FILE *f = fopen(argv[1], "rb");
    long n, npix, gap, ref_len, len;
    size_t pixbytes;
    int colors = atoi(argv[2]), fails = 0, pass;
    unsigned char *ppm, *ref, *blk;
    if (!f) return 2;
    fseek(f, 0, SEEK_END); n = ftell(f); rewind(f);
    ppm = malloc(n); ref = malloc(n);
    if (fread(ppm, 1, n, f) != (size_t)n) return 2;
    npix = n / 3;                                    /* >= the pixel count, as root's QGAP assumes */
    pixbytes = (size_t)(n - 64) / 3 * sizeof(pixel);   /* <= the pixel array (the P6 header is < 64 B) */

    ps_overlay = 0; biggest = 0;
    ref_len = pq_run(colors, ppm, n, ref, n);
    printf("separate: %ld B out, biggest malloc %lu\n", ref_len, (unsigned long)biggest);

    for (pass = 0; pass < 2; pass++) {               /* 0: room for the overlay; 1: too little, must fall back */
        gap = 64 + npix * (long)(sizeof(pixel) - 3) + (pass ? -1000 : 8);
        blk = malloc(16 + gap + n);
        memcpy(blk + 16 + gap, ppm, n);
        ps_overlay = 1; biggest = 0;
        len = pq_run(colors, blk + 16 + gap, n, blk + 16, n);
        printf("%s: %ld B out, biggest malloc %lu, %s\n", pass ? "short gap" : "overlay", len, (unsigned long)biggest,
               len == ref_len && !memcmp(blk + 16, ref, len) ? "same bytes" : "DIFFERENT");
        if (len != ref_len || memcmp(blk + 16, ref, len)) fails++;
        if (!pass && biggest >= pixbytes) { printf("overlay not used\n"); fails++; }
        if (pass && biggest < pixbytes) { printf("short gap did not fall back\n"); fails++; }
        free(blk);
    }
    printf(fails ? "FAIL\n" : "PASS\n");
    return fails != 0;
}
