/* pmshim — netpbm 9.5's I/O and utility calls, in memory, so a netpbm program's own .c runs unchanged on
 * the T425 AND in its TT twin's fallback with the SAME code (tt030-t425-kernel-ports W2: ppmquant.c +
 * libppm3.c for tppmquant, pnmscale.c for tpnmscale). Built with -DPPM_PACKCOLORS (4-byte pixels: the T425
 * has ~5 MB) and with main -> <prog>_main, exit -> ps_exit, qsort -> ps_qsort on both sides: INMOS's and
 * mintlib's qsort break ties differently, and median cut sorts with ties.
 *   ps_run(main, argc, argv, file, n, out, cap)  -> the program's stdout in out, or -1. Input is raw P5 or
 *   P6, maxval <= 255, no comments. ppm_readppm reads the whole image into its own array first, so for it
 *   out may be the input buffer; the row readers need out to be separate. */
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pnm.h"
#include "pmshim.h"

static const unsigned char *ps_in;
static long ps_in_len;
static unsigned char *ps_out;
/* ppmqserv sets it: ps_out and ps_in are one block with ps_out lower (root image op 'q', defect 4ab13d11), so the pixel
 * array is built at ps_out + PS_GAP over the gap and the input as it is read (each pixel's sizeof(pixel) bytes land
 * behind the next unread 3) and the output, written over it later, stays behind it (PS_GAP > the P6 header). ppm_readppm checks
 * the room itself and mallocs when it is not there (direct ppmqserv: ps_out == ps_in). */
int ps_overlay;
#define PS_GAP 64
static long ps_out_cap, ps_out_pos;
static long ps_in_pos;                            /* the row readers' place in the input */
static jmp_buf ps_jb;
xelval pnm_pbmmaxval = 1;

/* Arena: ppmquant was written to run once and exit, so it never frees its pixel array, hash or colormap.
 * In a server that runs it per request that leaked, and the second request ran out of memory (t4,
 * 2026-09-24). ppmquant.c, libppm3.c and this file allocate through ps_malloc/ps_free (-Dmalloc=...);
 * ps_run frees whatever is left when the run ends, however it ends. */
#ifdef PS_JOBHEAP
/* Linked into the resident job image (sam-job-tt/build_llvm.sh), whose jobheap.c already frees each job's leftovers:
 * a second 16-byte header per block cost tppmquant 512x512 ~0.5 MB of its 32767 histogram nodes (status 6). */
/* Blocks <= PS_NODE (ppmquant's 12-byte colorhist nodes, ~32k of them) come from slabs with a free list: jobheap's
 * 16 B header plus the runtime's own on each node left 800x600 at the heap edge (defect 4ab13d11). ps_run's
 * ps_free_all returns the slabs. ponytail: one size class, linear slab lookup in ps_free (<= PS_SLABS compares). */
#define PS_NODE 16
#define PS_PER_SLAB 4096                          /* 64 KB slabs */
#define PS_SLABS 64
static char *ps_slab[PS_SLABS];
static int ps_nslab;
static long ps_slab_used;
static void *ps_freelist;

void *ps_malloc(size_t n)
{
    void *p;
    if (n > PS_NODE)
        return malloc(n);
    if (ps_freelist) {
        p = ps_freelist;
        ps_freelist = *(void **)p;
        return p;
    }
    if (ps_nslab == 0 || ps_slab_used == PS_PER_SLAB) {
        if (ps_nslab == PS_SLABS || (ps_slab[ps_nslab] = (char *)malloc(PS_NODE * PS_PER_SLAB)) == NULL)
            return malloc(n);
        ps_nslab++;
        ps_slab_used = 0;
    }
    return ps_slab[ps_nslab - 1] + PS_NODE * ps_slab_used++;
}

void ps_free(void *p)
{
    int i;
    for (i = 0; p && i < ps_nslab; i++)
        if ((char *)p >= ps_slab[i] && (char *)p < ps_slab[i] + PS_NODE * PS_PER_SLAB) {
            *(void **)p = ps_freelist;
            ps_freelist = p;
            return;
        }
    free(p);
}

static void ps_free_all(void)
{
    while (ps_nslab > 0)
        free(ps_slab[--ps_nslab]);
    ps_slab_used = 0;
    ps_freelist = NULL;
}
#else
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
#endif

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
        if (!(ps_overlay && (unsigned char *)its[0] == ps_out + PS_GAP))
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

/* The header of a raw P5/P6 file; returns its magic digit and leaves ps_in_pos at the first pixel byte. */
static int ps_header(long *c, long *r, long *m)
{
    long pos = 2;
    if (ps_in_len < 11 || ps_in[0] != 'P' || (ps_in[1] != '5' && ps_in[1] != '6') || !ps_num(&pos, c)
        || !ps_num(&pos, r) || !ps_num(&pos, m) || *c < 1 || *r < 1 || *m < 1 || *m > 255 || pos >= ps_in_len)
        pm_error("not a raw PGM/PPM with maxval <= 255");
    ps_in_pos = pos + 1;                         /* exactly one whitespace after maxval */
    if (ps_in_len - ps_in_pos != *c * *r * (ps_in[1] == '6' ? 3 : 1))
        pm_error("truncated or oversized image");
    return ps_in[1];
}

pixel **ppm_readppm(FILE *file, int *colsP, int *rowsP, pixval *maxvalP)
{
    long c, r, m, x, y;
    pixel **rows, *blk;
    const unsigned char *p;
    (void)file;
    if (ps_header(&c, &r, &m) != '6')
        pm_error("not a raw PPM");
    rows = (pixel **)ps_malloc((size_t)r * sizeof(pixel *));
    p = ps_in + ps_in_pos;
    if (ps_overlay && ps_out + PS_GAP + c * r * ((long)sizeof(pixel) - 3) <= p && ps_out + PS_GAP + c * r * (long)sizeof(pixel) <= ps_in + ps_in_len)
        blk = (pixel *)(ps_out + PS_GAP);
    else
        blk = (pixel *)ps_malloc((size_t)(c * r) * sizeof(pixel));
    if (rows == NULL || blk == NULL)
        pm_error("out of memory");
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

void pnm_init(int *argcP, char *argv[])
{
    (void)argcP;
    (void)argv;
}

void pnm_readpnminit(FILE *file, int *colsP, int *rowsP, xelval *maxvalP, int *formatP)
{
    long c, r, m;
    (void)file;
    *formatP = ps_header(&c, &r, &m) == '6' ? RPPM_FORMAT : RPGM_FORMAT;
    *colsP = (int)c;
    *rowsP = (int)r;
    *maxvalP = (xelval)m;
}

void pnm_readpnmrow(FILE *file, xel *xelrow, int cols, xelval maxval, int format)
{
    int x, ppm = PNM_FORMAT_TYPE(format) == PPM_TYPE;
    const unsigned char *p = ps_in + ps_in_pos;
    (void)file;
    (void)maxval;
    if (ps_in_pos + (long)cols * (ppm ? 3 : 1) > ps_in_len)
        pm_error("truncated image");
    for (x = 0; x < cols; x++, p += ppm ? 3 : 1)
        if (ppm)
            PPM_ASSIGN(xelrow[x], p[0], p[1], p[2]);
        else
            PNM_ASSIGN1(xelrow[x], p[0]);
    ps_in_pos = p - ps_in;
}

static int ps_out_kind;                          /* '4', '5' or '6': the P-number being written */

void pnm_writepnminit(FILE *file, int cols, int rows, xelval maxval, int format, int forceplain)
{
    char h[40];
    long n;
    int t = PNM_FORMAT_TYPE(format);
    (void)file;
    (void)forceplain;
    ps_out_kind = t == PPM_TYPE ? '6' : t == PGM_TYPE ? '5' : '4';
    if (ps_out_kind == '4')
        n = sprintf(h, "P4\n%d %d\n", cols, rows);
    else
        n = sprintf(h, "P%c\n%d %d\n%d\n", ps_out_kind, cols, rows, (int)maxval);
    if (n > ps_out_cap)
        pm_error("output too small");
    memcpy(ps_out, h, (size_t)n);
    ps_out_pos = n;
}

void pnm_writepnmrow(FILE *file, xel *xelrow, int cols, xelval maxval, int format, int forceplain)
{
    int x;
    (void)format;
    if (ps_out_kind == '6') {
        ppm_writeppmrow(file, xelrow, cols, maxval, forceplain);
        return;
    }
    if (ps_out_pos + cols > ps_out_cap)
        pm_error("output too small");
    if (ps_out_kind == '5') {
        for (x = 0; x < cols; x++)
            ps_out[ps_out_pos++] = (unsigned char)PNM_GET1(xelrow[x]);
        return;
    }
    for (x = 0; x < cols; x += 8) {              /* P4, as libpnm: black (1) where the value is 0 */
        int k, byte = 0;
        for (k = 0; k < 8; k++)
            byte = (byte << 1) | (x + k < cols && PNM_GET1(xelrow[x + k]) == 0);
        ps_out[ps_out_pos++] = (unsigned char)byte;
    }
}

void pnm_writepnm(FILE *file, xel **xels, int cols, int rows, xelval maxval, int format, int forceplain)
{
    int y;
    pnm_writepnminit(file, cols, rows, maxval, format, forceplain);
    for (y = 0; y < rows; y++)
        pnm_writepnmrow(file, xels[y], cols, maxval, format, forceplain);
}

/* A readable, not-streamed input (a GIF for giftopnm): fread from the request buffer. */
size_t ps_fread(void *buf, size_t size, size_t n, FILE *file)
{
    long want = (long)(size * n), have = ps_in_len - ps_in_pos;
    (void)file;
    if (size == 0)
        return 0;
    if (want > have)
        want = have - have % (long)size;
    memcpy(buf, ps_in + ps_in_pos, (size_t)want);
    ps_in_pos += want;
    return (size_t)want / size;
}

char **pm_allocarray(int cols, int rows, int size)
{
    char **its = (char **)ps_malloc((size_t)rows * sizeof(char *));
    int i;
    if (its == NULL || (its[0] = (char *)ps_malloc((size_t)rows * cols * size)) == NULL)
        pm_error("out of memory allocating an array");
    for (i = 1; i < rows; i++)
        its[i] = its[0] + (long)i * cols * size;
    return its;
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

long ps_run(int (*prog_main)(int, char **), int argc, char **argv, const unsigned char *in, long n,
            unsigned char *out, long cap)
{
    int r;
    ps_in = in;
    ps_in_len = n;
    ps_in_pos = 0;
    ps_out = out;
    ps_out_cap = cap;
    ps_out_pos = 0;
    r = setjmp(ps_jb);
    if (r == 0) {
        prog_main(argc, argv);
        r = 1;                                   /* main returned without exit(): treat as success */
    }
    ps_free_all();
    return r == 1 ? ps_out_pos : -1;
}
