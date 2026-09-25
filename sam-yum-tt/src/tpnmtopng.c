/* tpnmtopng — `pnmtopng [-compression N] [pnmfile]` for raw P5/P6 (maxval <= 255) with the work on the
 * ATW800/2 T425 (tt030-t425-kernel-ports W2), and the 68030 fallback running the SAME encoder (pngenc.c +
 * zlib 1.3.2), so the output is identical either way. A twin of pnmtopng, not a replacement: -filter,
 * -interlace, -text, -transparent, -gamma, plain/16-bit input … stay with stock pnmtopng. The file is
 * byte-identical to stock's (measured on three images, perf.md).
 * The T425 takes inputs of T4P_MIN and up whose input + output + zlib fit T4P_MAX, when
 * /etc/t425/enabled/tpnmtopng exists; otherwise, or on any failure, the 68030 runs it and says why. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pngenc.h"
#include "t4call.h"

#define T4P_BTL "/usr/lib/t425/tpnmtopng.btl"
#define T4P_MIN (224L * 1024)                    /* input bytes; 320x240 RGB (225 KB) measured 0.83 */
#define T4P_MAX (4400L * 1024 - 300L * 1024)     /* input + output; zlib's level-9 state takes ~300 KB */

static unsigned char *slurp(FILE *f, long *n)
{
    long cap = 1L << 20, got = 0, r;
    unsigned char *b = (unsigned char *)malloc(cap), *nb;
    while (b && (r = (long)fread(b + got, 1, cap - got, f)) > 0) {
        got += r;
        if (got == cap) {
            cap *= 2;
            if (!(nb = (unsigned char *)realloc(b, cap)))
                free(b);
            b = nb;
        }
    }
    *n = got;
    return b;
}

int main(int argc, char **argv)
{
    int verbose = 0, level = 6, i = 1;
    long n, cap, len = -1;
    unsigned char *buf, *out;
    const char *why = "not enabled";
    FILE *f = stdin;

    if (i < argc && !strcmp(argv[i], "-v"))
        verbose = i++;
    if (i + 1 < argc && !strcmp(argv[i], "-compression")) {
        level = atoi(argv[i + 1]);
        i += 2;
    }
    if (argc - i > 1 || level < 0 || level > 9 || (i < argc && argv[i][0] == '-' && argv[i][1])) {
        fprintf(stderr, "usage: tpnmtopng [-v] [-compression 0..9] [pnmfile]   (other options: use pnmtopng)\n");
        return 2;
    }
    if (i < argc && !(f = fopen(argv[i], "rb"))) {
        perror(argv[i]);
        return 1;
    }
    buf = slurp(f, &n);
    if (!buf) {
        fprintf(stderr, "tpnmtopng: out of memory\n");
        return 1;
    }
    if ((cap = pe_cap(buf, n)) < 0 || !(out = (unsigned char *)malloc(cap))) {
        fprintf(stderr, cap < 0 ? "tpnmtopng: need a raw PGM/PPM (P5/P6, maxval <= 255, no comments); "
                                  "else use pnmtopng\n" : "tpnmtopng: out of memory\n");
        return 1;
    }
    t4call_timeout_ms = 1800000L;               /* 800x600 takes minutes on the 68030: see perf.md */
    if (t4_enabled("tpnmtopng")) {
        if (n < T4P_MIN || n + cap > T4P_MAX)
            why = "not used for this size";
        else
            len = t4call(T4P_BTL, 'p', level, buf, n, out, cap, &why);
        if (len < 0 && n >= T4P_MIN && n + cap <= T4P_MAX)
            fprintf(stderr, "tpnmtopng: T425 %s; encoding on the 68030\n", why);
    }
    if (len < 0) {                               /* the same encoder here */
        len = pe_encode(buf, n, level, out, cap);
        if (len < 0) {
            fprintf(stderr, "tpnmtopng: encoding failed\n");
            return 1;
        }
    } else if (verbose)
        fprintf(stderr, "tpnmtopng: encoded on the T425\n");
    if ((long)fwrite(out, 1, len, stdout) != len || fflush(stdout) != 0) {
        fprintf(stderr, "tpnmtopng: write error\n");
        return 1;
    }
    return 0;
}
