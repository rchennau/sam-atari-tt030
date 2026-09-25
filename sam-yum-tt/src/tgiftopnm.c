/* tgiftopnm — netpbm's `giftopnm [-image N]` with the work on the ATW800/2 T425 (tt030-t425-kernel-ports W2),
 * and the 68030 fallback running the SAME code (giftopnm.c through pmshim.c), so the output is identical
 * either way. A twin of giftopnm, not a replacement: -verbose / -comments print from inside the decoder and
 * stay with stock giftopnm, as does an image larger than its GIF screen.
 *   tgiftopnm [-v] [-image N] [giffile]      the PNM file (P4/P5/P6, as giftopnm picks) on stdout
 * The T425 takes images of T4G_MIN pixels and up whose GIF + decoded image + output fit T4G_MAX, when
 * /etc/t425/enabled/tgiftopnm exists; otherwise, or on any failure, the 68030 runs it and says why. *
 * Shipped 68030-only (2026-09-25): the T425 path failed the FR-5 gate (on / off 0.80-1.18, perf.md), so
 * the RPM carries no server and no -t425-on switch; the gain is pmshim's in-memory I/O. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gifrun.h"
#include "t4call.h"

#define T4G_BTL "/usr/lib/t425/tgiftopnm.btl"
#define T4G_MIN (21845L)                         /* pixels (64 KB of RGB); below this the ~1 s boot dominates */
#define T4G_MAX (4400L * 1024)                   /* bytes on the board (~5 MB usable, measured) */

static unsigned char *slurp(FILE *f, long *n)
{
    long cap = 1L << 18, got = 0, r;
    unsigned char *b = (unsigned char *)malloc(cap + 4), *nb;
    while (b && (r = (long)fread(b + 4 + got, 1, cap - got, f)) > 0) {
        got += r;
        if (got == cap) {
            cap *= 2;
            if (!(nb = (unsigned char *)realloc(b, cap + 4)))
                free(b);
            b = nb;
        }
    }
    *n = got;
    return b;                                    /* the file starts at b + 4: room for the request head */
}

int main(int argc, char **argv)
{
    int verbose = 0, i = 1;
    long image = 1, n, cap, px, len = -1;
    unsigned char *buf, *out;
    const char *why = "not enabled";
    FILE *f = stdin;

    if (i < argc && !strcmp(argv[i], "-v"))
        verbose = i++;
    if (i + 1 < argc && !strcmp(argv[i], "-image")) {
        image = atol(argv[i + 1]);
        i += 2;
    }
    if (argc - i > 1 || image < 1 || (i < argc && argv[i][0] == '-' && argv[i][1])) {
        fprintf(stderr, "usage: tgiftopnm [-v] [-image N] [giffile]   (-verbose / -comments: use giftopnm)\n");
        return 2;
    }
    if (i < argc && !(f = fopen(argv[i], "rb"))) {
        perror(argv[i]);
        return 1;
    }
    buf = slurp(f, &n);
    if (!buf) {
        fprintf(stderr, "tgiftopnm: out of memory\n");
        return 1;
    }
    if ((cap = gf_cap(buf + 4, n)) < 0 || !(out = (unsigned char *)malloc(cap))) {
        fprintf(stderr, cap < 0 ? "tgiftopnm: not a GIF file\n" : "tgiftopnm: out of memory\n");
        return 1;
    }
    px = gf_pixels(buf + 4, n);
    t4call_timeout_ms = 600000L;                 /* 800x600 takes ~40 s on the 68030: see perf.md */
    if (t4_enabled("tgiftopnm")) {
        if (px < T4G_MIN || n + px * 4 + cap > T4G_MAX)
            why = "not used for this size";
        else {
            buf[0] = (unsigned char)image; buf[1] = (unsigned char)(image >> 8);
            buf[2] = (unsigned char)(image >> 16); buf[3] = (unsigned char)(image >> 24);
            len = t4call(T4G_BTL, 'g', 0, buf, n + 4, out, cap, &why);
        }
        if (len < 0 && px >= T4G_MIN && n + px * 4 + cap <= T4G_MAX)
            fprintf(stderr, "tgiftopnm: T425 %s; decoding on the 68030\n", why);
    }
    if (len < 0) {                               /* the same kernel here */
        len = gf_run(image, buf + 4, n, out, cap);
        if (len < 0) {
            fprintf(stderr, "tgiftopnm: giftopnm refused this GIF (or the image is larger than its screen); "
                            "try giftopnm\n");
            return 1;
        }
    } else if (verbose)
        fprintf(stderr, "tgiftopnm: decoded on the T425\n");
    if ((long)fwrite(out, 1, len, stdout) != len || fflush(stdout) != 0) {
        fprintf(stderr, "tgiftopnm: write error\n");
        return 1;
    }
    return 0;
}
