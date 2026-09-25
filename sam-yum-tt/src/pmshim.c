/* pmshim — netpbm 9.5's I/O and utility calls, in memory, so ppmquant.c + libppm3.c run unchanged on the
 * T425 (ppmqserv.c) AND in the TT's tppmquant fallback with the SAME code (tt030-t425-kernel-ports W2).
 * Built with -DPPM_PACKCOLORS (4-byte pixels: the T425 has ~5 MB) and with main -> ppmquant_main,
 * exit -> ps_exit, qsort -> ps_qsort on both sides, so the colormap is identical: INMOS's and mintlib's
 * qsort break ties differently, and median cut sorts with ties.
 *   ps_run(ncolors, PPM file, n, out, cap)  -> the quantized PPM file in out (may be the same buffer as
 *   the input: the pixels are read into their own array first), or -1. Raw P6, maxval <= 255 only. */
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ppm.h"
#include "pmshim.h"

static const unsigned char *ps_in;
static long ps_in_len;
static unsigned char *ps_out;
static long ps_out_cap, ps_out_pos;
static jmp_buf ps_jb;

extern int ppmquant_main(int argc, char *argv[]);

/* Arena: ppmquant was written to run once and exit, so it never frees its pixel array, hash or colormap.
 * In a server that runs it per request that leaked, and the second request ran out of memory (t4,
 * 2026-09-24). ppmquant.c, libppm3.c and this file allocate through ps_malloc/ps_free (-Dmalloc=...);
 * ps_run frees whatever is left when the run ends, however it ends. */
struct ps_blk {
    struct ps_blk *prev, *next;
    double align;                                /* keep the user block aligned like malloc's */
};
static struct ps_blk ps_live = { &ps_live, &ps_live, 0.0 };

void *ps_malloc(size_t n)
{
    struct ps_blk *b = (struct ps_blk *)malloc(sizeof *b + n);
    if (b == NULL)
        return NULL;
    b->next = ps_live.next;
    b->prev = &ps_live;
    ps_live.next->prev = b;
    ps_live.next = b;
    return b + 1;
}

void ps_free(void *p)
{
    struct ps_blk *b;
    if (p == NULL)
        return;
    b = (struct ps_blk *)p - 1;
    b->prev->next = b->next;
    b->next->prev = b->prev;
    free(b);
}

static void ps_free_all(void)
{
    while (ps_live.next != &ps_live)
        ps_free(ps_live.next + 1);
}

void ps_exit(int code)
{
    longjmp(ps_jb, code == 0 ? 1 : 2);
}

/* Floyd-Steinberg only: ppmquant seeds it with srand(time(0) ^ getpid()), which is neither reproducible
 * nor allowed on the T425 (time() would be a host request over the link). tppmquant never runs -fs; these
 * make sure nothing reaches the host even if it did. */
long ps_time(long *t)
{
    (void)t;
    pm_error("-fs is not supported here");
    return 0;
}

int ps_getpid(void)
{
    return 0;
}

void ps_srand(unsigned s)
{
    (void)s;
}

int ps_rand(void)
{
    return 0;
}

void ppm_init(int *argcP, char *argv[])
{
    (void)argcP;
    (void)argv;
}

void pm_message(const char format[], ...)
{
    (void)format;                                /* quiet on both sides */
}

void pm_error(const char format[], ...)
{
    (void)format;
    longjmp(ps_jb, 2);
}

void pm_usage(const char usage[])
{
    (void)usage;
    longjmp(ps_jb, 2);
}

int pm_keymatch(char *str, char *keyword, int minchars)
{
    int len = (int)strlen(str);
    if (len < minchars)
        return 0;
    while (--len >= 0) {
        int c1 = *str++, c2 = *keyword++;
        if (c2 == '\0')
            return 0;
        if (c1 >= 'A' && c1 <= 'Z')
            c1 += 'a' - 'A';
        if (c2 >= 'A' && c2 <= 'Z')
            c2 += 'a' - 'A';
        if (c1 != c2)
            return 0;
    }
    return 1;
}

FILE *pm_openr(char *name)
{
    (void)name;
    return stdin;                                /* a token: nothing here reads a real file */
}

void pm_close(FILE *f)
{
    (void)f;
}

char *pm_allocrow(int cols, int size)
{
    char *r = (char *)ps_malloc((size_t)cols * size);
    if (r == NULL)
        pm_error("out of memory");
    return r;
}

void pm_freerow(char *itrow)
{
    ps_free(itrow);
}

void pm_freearray(char **its, int rows)
{
    (void)rows;
    if (its) {                                   /* ppm_readppm below: one block + the row pointers */
        ps_free(its[0]);
        ps_free(its);
    }
}

static int ps_num(long *pos, long *v)
{
    long i = *pos;
    while (i < ps_in_len && (ps_in[i] == ' ' || ps_in[i] == '\t' || ps_in[i] == '\n' || ps_in[i] == '\r'))
        i++;
    if (i >= ps_in_len || ps_in[i] < '0' || ps_in[i] > '9')
        return 0;                                /* '#' comments are not supported: tppmquant refuses them */
    for (*v = 0; i < ps_in_len && ps_in[i] >= '0' && ps_in[i] <= '9'; i++)
        *v = *v * 10 + (ps_in[i] - '0');
    *pos = i;
    return 1;
}

pixel **ppm_readppm(FILE *file, int *colsP, int *rowsP, pixval *maxvalP)
{
    long pos = 2, c, r, m, x, y;
    pixel **rows, *blk;
    const unsigned char *p;
    (void)file;
    if (ps_in_len < 11 || ps_in[0] != 'P' || ps_in[1] != '6' || !ps_num(&pos, &c) || !ps_num(&pos, &r)
        || !ps_num(&pos, &m) || m < 1 || m > 255 || pos >= ps_in_len)
        pm_error("not a raw PPM with maxval <= 255");
    pos++;                                       /* exactly one whitespace after maxval */
    if (ps_in_len - pos != c * r * 3)
        pm_error("truncated PPM");
    rows = (pixel **)ps_malloc((size_t)r * sizeof(pixel *));
    blk = (pixel *)ps_malloc((size_t)(c * r) * sizeof(pixel));
    if (rows == NULL || blk == NULL)
        pm_error("out of memory");
    p = ps_in + pos;
    for (y = 0; y < r; y++) {
        rows[y] = blk + y * c;
        for (x = 0; x < c; x++, p += 3)
            PPM_ASSIGN(rows[y][x], p[0], p[1], p[2]);
    }
    *colsP = (int)c;
    *rowsP = (int)r;
    *maxvalP = (pixval)m;
    return rows;
}

void ppm_writeppminit(FILE *file, int cols, int rows, pixval maxval, int forceplain)
{
    char h[40];
    long n;
    (void)file;
    (void)forceplain;
    n = sprintf(h, "P6\n%d %d\n%d\n", cols, rows, (int)maxval);
    if (n > ps_out_cap)
        pm_error("output too small");
    memcpy(ps_out, h, (size_t)n);
    ps_out_pos = n;
}

void ppm_writeppmrow(FILE *file, pixel *pixelrow, int cols, pixval maxval, int forceplain)
{
    int x;
    (void)file;
    (void)maxval;
    (void)forceplain;
    if (ps_out_pos + (long)cols * 3 > ps_out_cap)
        pm_error("output too small");
    for (x = 0; x < cols; x++) {
        ps_out[ps_out_pos++] = (unsigned char)PPM_GETR(pixelrow[x]);
        ps_out[ps_out_pos++] = (unsigned char)PPM_GETG(pixelrow[x]);
        ps_out[ps_out_pos++] = (unsigned char)PPM_GETB(pixelrow[x]);
    }
}

/* Shell sort: deterministic, the same on both CPUs (the libc qsorts are not). Median cut calls it on at
 * most MAXCOLORS entries a few hundred times. */
void ps_qsort(void *base, size_t n, size_t size, int (*cmp)(const void *, const void *))
{
    char *b = (char *)base, *tmp = (char *)malloc(size);
    size_t gap, i, j;
    if (tmp == NULL)
        pm_error("out of memory");
    for (gap = n / 2; gap > 0; gap /= 2)
        for (i = gap; i < n; i++) {
            memcpy(tmp, b + i * size, size);
            for (j = i; j >= gap && cmp(b + (j - gap) * size, tmp) > 0; j -= gap)
                memcpy(b + j * size, b + (j - gap) * size, size);
            memcpy(b + j * size, tmp, size);
        }
    free(tmp);
}

long ps_run(int ncolors, const unsigned char *ppm, long n, unsigned char *out, long cap)
{
    char arg[16], *argv[3];
    int r;
    ps_in = ppm;
    ps_in_len = n;
    ps_out = out;
    ps_out_cap = cap;
    ps_out_pos = 0;
    sprintf(arg, "%d", ncolors);
    argv[0] = "ppmquant";
    argv[1] = arg;
    argv[2] = NULL;
    r = setjmp(ps_jb);
    if (r == 0) {
        ppmquant_main(2, argv);
        r = 1;                                   /* main returned without exit(): treat as success */
    }
    ps_free_all();
    return r == 1 ? ps_out_pos : -1;
}
