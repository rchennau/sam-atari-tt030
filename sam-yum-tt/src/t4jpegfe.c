/* t4jpegfe — the TT side of cjpeg/djpeg on the T425 (tt030-t425-kernel-ports W2). #included by cjpeg.c
 * and djpeg.c when built with -DT425 (scripts/build_t425_libjpeg.sh). Only plain invocations go to the
 * T425, the ones jpegserv reproduces exactly:
 *   djpeg [-outfile F] FILE                         -> PPM/PGM (djpeg's default output)
 *   cjpeg [-quality N] [-outfile F] FILE            <- a raw PPM/PGM (P6/P5, maxval 255, no comments)
 * Anything else, or any failure before output, returns -1 and the stock code path runs. A failure is
 * said once on stderr. The package offloads only with /etc/t425/enabled/libjpeg (libjpeg-t425-on). */
#include "t4call.h"
#include <ctype.h>
#include <sys/stat.h>

#define T4J_BTL "/usr/lib/t425/libjpeg.btl"
#define T4J_MAX (2560L * 1024)                   /* raw rows <= 2.5 MB: in + out within the 4 MB board */
#define T4J_MIN (320L * 240 * 3)                 /* raw rows; measured 2026-09-24: 160x120 djpeg 1.5 -> 2.6 s,
                                                    320x240 4.65 -> 4.16 s, 800x600 29.7 -> 15.4 s */

static void t4j_note(const char *prog, const char *why)
{
    static int said;
    if (!said++)
        fprintf(stderr, "%s: T425 %s; using the 68030\n", prog, why);
}

/* argv: [-quality N] [-outfile F] FILE, in any order; -1 if anything else is there. */
static int t4j_args(int argc, char **argv, int allow_quality, int *quality, const char **outname,
                    const char **inname)
{
    int i;
    *quality = 0;
    *outname = NULL;
    *inname = NULL;
    for (i = 1; i < argc; i++) {
        if (allow_quality && !strcmp(argv[i], "-quality") && i + 1 < argc) {
            *quality = atoi(argv[++i]);
            if (*quality < 1 || *quality > 100)
                return -1;
        } else if (!strcmp(argv[i], "-outfile") && i + 1 < argc)
            *outname = argv[++i];
        else if (argv[i][0] != '-' && *inname == NULL)
            *inname = argv[i];
        else
            return -1;
    }
    return *inname ? 0 : -1;
}

static unsigned char *t4j_slurp(const char *name, long *n)
{
    struct stat st;
    FILE *f;
    unsigned char *p;
    if (stat(name, &st) != 0 || st.st_size <= 0 || st.st_size > T4J_MAX + 64 || !(f = fopen(name, "rb")))
        return NULL;
    p = (unsigned char *)malloc(st.st_size);
    if (p && (long)fread(p, 1, st.st_size, f) != (long)st.st_size) {
        free(p);
        p = NULL;
    }
    fclose(f);
    *n = (long)st.st_size;
    return p;
}

/* Raw size (width * height * components) from a JPEG's first SOF marker; 0 if none. */
static long t4j_sof(const unsigned char *p, long n)
{
    long i = 2;
    while (i + 9 < n) {
        if (p[i] != 0xFF) {
            i++;
            continue;
        }
        if (p[i + 1] >= 0xC0 && p[i + 1] <= 0xCF && p[i + 1] != 0xC4 && p[i + 1] != 0xC8 && p[i + 1] != 0xCC)
            return (long)((p[i + 5] << 8) | p[i + 6]) * ((p[i + 7] << 8) | p[i + 8]) * p[i + 9];
        if (p[i + 1] == 0xD8 || p[i + 1] == 0x01 || (p[i + 1] >= 0xD0 && p[i + 1] <= 0xD7)) {
            i += 2;
            continue;
        }
        i += 2 + ((p[i + 2] << 8) | p[i + 3]);
    }
    return 0;
}

static FILE *t4j_out(const char *outname)
{
    return outname ? fopen(outname, "wb") : stdout;
}

static unsigned long t4j_get4(const unsigned char *b)
{
    return b[0] | ((unsigned long)b[1] << 8) | ((unsigned long)b[2] << 16) | ((unsigned long)b[3] << 24);
}

#ifdef T4J_DJPEG
static int t4_djpeg(int argc, char **argv)
{
    int q;
    const char *outname, *inname, *why = "failed";
    unsigned char *in = NULL, *out = NULL;
    long n = 0, len = -1, cap;
    FILE *o;

    if (!t4_enabled("libjpeg") || t4j_args(argc, argv, 0, &q, &outname, &inname) != 0
        || !(in = t4j_slurp(inname, &n)))
        return -1;
    if (t4j_sof(in, n) < T4J_MIN || t4j_sof(in, n) > T4J_MAX) {
        free(in);                                /* small: the ~1 s boot loses; huge: no room */
        return -1;
    }
    cap = T4J_MAX + 9 + 64;
    if (!(out = (unsigned char *)malloc(cap)))
        why = "had no memory";
    else
        len = t4call(T4J_BTL, 'D', 0, in, n, out, cap, &why);
    if (len > 9 && (o = t4j_out(outname)) != NULL) {
        unsigned long w = t4j_get4(out), h = t4j_get4(out + 4);
        int comp = out[8];
        fprintf(o, comp == 1 ? "P5\n%ld %ld\n%d\n" : "P6\n%ld %ld\n%d\n", (long)w, (long)h, 255);
        if ((long)fwrite(out + 9, 1, len - 9, o) != len - 9 || (outname && fclose(o) != 0)) {
            fprintf(stderr, "%s: write error\n", argv[0]);
            exit(EXIT_FAILURE);                  /* output half-written: not a case for the fallback */
        }
        free(in);
        free(out);
        return 0;
    }
    t4j_note(argv[0], len > 9 ? "could not open the output" : why);
    free(in);
    free(out);
    return -1;
}
#endif

#ifdef T4J_CJPEG
/* A raw PPM/PGM header "P6|P5 <ws> W <ws> H <ws> 255 <one ws>" without comments; offset of the data. */
static long t4j_ppm(const unsigned char *p, long n, unsigned long *w, unsigned long *h, int *comp)
{
    long i = 2, v[3];
    int k;
    if (n < 11 || p[0] != 'P' || (p[1] != '6' && p[1] != '5'))
        return -1;
    *comp = p[1] == '6' ? 3 : 1;
    for (k = 0; k < 3; k++) {
        if (i >= n || !isspace(p[i]))
            return -1;
        while (i < n && isspace(p[i]))
            i++;
        if (i >= n || !isdigit(p[i]))
            return -1;                           /* a '#' comment: let rdppm handle it */
        for (v[k] = 0; i < n && isdigit(p[i]); i++)
            v[k] = v[k] * 10 + (p[i] - '0');
    }
    if (v[2] != 255 || i >= n || !isspace(p[i]))
        return -1;
    *w = (unsigned long)v[0];
    *h = (unsigned long)v[1];
    i++;
    return (long)(n - i) == (long)(*w * *h * *comp) ? i : -1;
}

static int t4_cjpeg(int argc, char **argv)
{
    int q, comp;
    const char *outname, *inname, *why = "failed";
    unsigned char *in = NULL, *out = NULL;
    unsigned long w, h;
    long n = 0, off, len = -1, raw;
    FILE *o;

    if (!t4_enabled("libjpeg") || t4j_args(argc, argv, 1, &q, &outname, &inname) != 0
        || !(in = t4j_slurp(inname, &n)))
        return -1;
    if ((off = t4j_ppm(in, n, &w, &h, &comp)) < 0 || n - off < T4J_MIN) {
        free(in);
        return -1;                               /* not a plain PPM: the stock readers take it */
    }
    raw = n - off;        /* reuse the file buffer: a raw PPM header is >= 11 bytes, the request head 9 */
    memmove(in + 9, in + off, raw);
    in[0] = (unsigned char)w; in[1] = (unsigned char)(w >> 8); in[2] = (unsigned char)(w >> 16);
    in[3] = (unsigned char)(w >> 24);
    in[4] = (unsigned char)h; in[5] = (unsigned char)(h >> 8); in[6] = (unsigned char)(h >> 16);
    in[7] = (unsigned char)(h >> 24);
    in[8] = (unsigned char)comp;
    if (!(out = (unsigned char *)malloc(raw + 4096)))
        why = "had no memory";
    else
        len = t4call(T4J_BTL, 'C', q, in, raw + 9, out, raw + 4096, &why);
    if (len > 0 && (o = t4j_out(outname)) != NULL) {
        if ((long)fwrite(out, 1, len, o) != len || (outname && fclose(o) != 0)) {
            fprintf(stderr, "%s: write error\n", argv[0]);
            exit(EXIT_FAILURE);
        }
        free(in);
        free(out);
        return 0;
    }
    t4j_note(argv[0], len > 0 ? "could not open the output" : why);
    free(in);
    free(out);
    return -1;
}
#endif
