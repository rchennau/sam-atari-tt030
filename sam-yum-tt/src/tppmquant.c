/* tppmquant — netpbm's `ppmquant N [ppmfile]` with the work on the ATW800/2 T425 (tt030-t425-kernel-ports
 * W2), and the 68030 fallback running the SAME code (ppmquant.c + libppm3.c through pmshim.c), so the
 * output is identical either way. A twin of ppmquant, not a replacement: -fs/-floyd and -map stay with
 * stock ppmquant (-fs is time-seeded, so it is not reproducible anyway).
 *   tppmquant [-v] ncolors [ppmfile]        raw P6 (maxval <= 255, no comments) in, quantized P6 out on stdout
 * The T425 takes images of 64 KB .. 1.5 MB of pixels (in place; the board has ~5 MB usable, measured) when
 * /etc/t425/enabled/tppmquant exists; otherwise, or on any failure, the 68030 runs it and says why. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pmshim.h"
#include "t4call.h"

#define T4Q_BTL "/usr/lib/t425/tppmquant.btl"
#define T4Q_MIN (64L * 1024)                     /* pixels bytes; below this the ~1 s boot dominates */
#define T4Q_MAX (1536L * 1024)

static unsigned char *slurp(FILE *f, long *n)
{
    long cap = 1L << 20, got = 0, r;
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
    long colors, n, len = -1;
    unsigned char *buf, *out = NULL;
    const char *why = "not enabled";
    FILE *f = stdin;

    if (i < argc && !strcmp(argv[i], "-v"))
        verbose = i++;
    if (i >= argc || (colors = atol(argv[i])) < 2 || colors > 32767 || argc - i > 2) {
        fprintf(stderr, "usage: tppmquant [-v] ncolors [ppmfile]   (-fs / -map: use ppmquant)\n");
        return 2;
    }
    if (i + 1 < argc && !(f = fopen(argv[i + 1], "rb"))) {
        perror(argv[i + 1]);
        return 1;
    }
    buf = slurp(f, &n);
    if (!buf) {
        fprintf(stderr, "tppmquant: out of memory\n");
        return 1;
    }
    t4call_timeout_ms = 1800000L;               /* the kernel runs for minutes (800x600: see perf.md) */
    if (t4_enabled("tppmquant")) {
        if (n < T4Q_MIN || n > T4Q_MAX)
            why = "not used for this size";
        else if (!(out = (unsigned char *)malloc(n + 4)))
            why = "had no memory";
        else {
            buf[0] = (unsigned char)colors; buf[1] = (unsigned char)(colors >> 8);
            buf[2] = (unsigned char)(colors >> 16); buf[3] = (unsigned char)(colors >> 24);
            len = t4call(T4Q_BTL, 'q', 0, buf, n + 4, out, n + 4, &why);
        }
        if (len < 0 && n >= T4Q_MIN && n <= T4Q_MAX)
            fprintf(stderr, "tppmquant: T425 %s; quantizing on the 68030\n", why);
    }
    if (len < 0) {                               /* the same kernel here, in place */
        len = ps_run((int)colors, buf + 4, n, buf + 4, n);
        if (len < 0) {
            fprintf(stderr, "tppmquant: need a raw PPM (P6, maxval <= 255, no comments); else use ppmquant\n");
            return 1;
        }
        out = buf + 4;
    } else if (verbose)
        fprintf(stderr, "tppmquant: quantized on the T425\n");
    if ((long)fwrite(out, 1, len, stdout) != len || fflush(stdout) != 0) {
        fprintf(stderr, "tppmquant: write error\n");
        return 1;
    }
    return 0;
}
