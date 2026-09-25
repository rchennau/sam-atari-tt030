/* tpnmscale — netpbm's `pnmscale -xsize/-ysize/-xysize` with the work on the ATW800/2 T425
 * (tt030-t425-kernel-ports W2), and the 68030 fallback running the SAME code (pnmscale.c through
 * pmshim.c), so the output is identical either way. A twin of pnmscale, not a replacement: -xscale,
 * -yscale, -pixels and the bare scale factor parse floats and stay with stock pnmscale (pnmsrun.h).
 *   tpnmscale [-v] {-xsize|-width N} {-ysize|-height N} | -xysize X Y  [pnmfile]
 * raw P5/P6 (maxval <= 255, no comments) in, the scaled file out on stdout. The T425 takes inputs of 64 KB
 * and up whose input + output fit T4S_MAX when /etc/t425/enabled/tpnmscale exists; otherwise, or on any
 * failure, the 68030 runs it and says why. *
 * Shipped 68030-only (2026-09-25): the T425 path failed the FR-5 gate (on / off 0.80-1.18, perf.md), so
 * the RPM carries no server and no -t425-on switch; the gain is pmshim's in-memory I/O. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pnmsrun.h"
#include "t4call.h"

#define T4S_BTL "/usr/lib/t425/tpnmscale.btl"
#define T4S_MIN (64L * 1024)                     /* input bytes; below this the ~1 s boot dominates */
#define T4S_MAX (4400L * 1024)                   /* input + output on the board (~5 MB usable, measured) */

static unsigned char *slurp(FILE *f, long *n)
{
    long cap = 1L << 20, got = 0, r;
    unsigned char *b = (unsigned char *)malloc(cap + 8), *nb;
    while (b && (r = (long)fread(b + 8 + got, 1, cap - got, f)) > 0) {
        got += r;
        if (got == cap) {
            cap *= 2;
            if (!(nb = (unsigned char *)realloc(b, cap + 8)))
                free(b);
            b = nb;
        }
    }
    *n = got;
    return b;                                    /* the file starts at b + 8: room for the request head */
}

static int usage(void)
{
    fprintf(stderr, "usage: tpnmscale [-v] {-xsize|-width N} {-ysize|-height N} | -xysize X Y [pnmfile]\n"
                    "       (-xscale / -yscale / -pixels / a scale factor: use pnmscale)\n");
    return 2;
}

static void put32(unsigned char *p, long v)
{
    p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24);
}

int main(int argc, char **argv)
{
    int verbose = 0, mode = 0, i = 1;
    long x = 0, y = 0, n, cap, len = -1;
    unsigned char *buf, *out;
    const char *why = "not enabled";
    FILE *f = stdin;

    if (i < argc && !strcmp(argv[i], "-v"))
        verbose = i++;
    for (; i < argc && argv[i][0] == '-'; i++) {
        if (i + 1 >= argc)
            return usage();
        if (!strcmp(argv[i], "-xsize") || !strcmp(argv[i], "-width"))
            x = atol(argv[++i]);
        else if (!strcmp(argv[i], "-ysize") || !strcmp(argv[i], "-height"))
            y = atol(argv[++i]);
        else if (!strcmp(argv[i], "-xysize") && i + 2 < argc) {
            mode = 1;
            x = atol(argv[++i]);
            y = atol(argv[++i]);
        } else
            return usage();
    }
    if (argc - i > 1 || x < 0 || y < 0 || (x == 0 && y == 0) || (mode == 1 && (x == 0 || y == 0)))
        return usage();
    if (i < argc && !(f = fopen(argv[i], "rb"))) {
        perror(argv[i]);
        return 1;
    }
    buf = slurp(f, &n);
    if (!buf) {
        fprintf(stderr, "tpnmscale: out of memory\n");
        return 1;
    }
    if ((cap = pn_cap(mode, x, y, buf + 8, n)) < 0 || !(out = (unsigned char *)malloc(cap))) {
        fprintf(stderr, cap < 0 ? "tpnmscale: need a raw PGM/PPM (P5/P6, maxval <= 255, no comments); "
                                  "else use pnmscale\n" : "tpnmscale: out of memory\n");
        return 1;
    }
    t4call_timeout_ms = 600000L;                 /* 800x600 takes ~40 s on the 68030: see perf.md */
    if (t4_enabled("tpnmscale")) {
        if (n < T4S_MIN || n + cap > T4S_MAX)
            why = "not used for this size";
        else {
            put32(buf, x);
            put32(buf + 4, y);
            len = t4call(T4S_BTL, 's', mode, buf, n + 8, out, cap, &why);
        }
        if (len < 0 && n >= T4S_MIN && n + cap <= T4S_MAX)
            fprintf(stderr, "tpnmscale: T425 %s; scaling on the 68030\n", why);
    }
    if (len < 0) {                               /* the same kernel here */
        len = pn_run(mode, x, y, buf + 8, n, out, cap);
        if (len < 0) {
            fprintf(stderr, "tpnmscale: pnmscale refused this image or size; try pnmscale\n");
            return 1;
        }
    } else if (verbose)
        fprintf(stderr, "tpnmscale: scaled on the T425\n");
    if ((long)fwrite(out, 1, len, stdout) != len || fflush(stdout) != 0) {
        fprintf(stderr, "tpnmscale: write error\n");
        return 1;
    }
    return 0;
}
