/* pngenc — a raw PGM/PPM (P5/P6, maxval <= 255) to PNG, the work of tpnmtopng (tt030-t425-kernel-ports W2).
 * The same C runs on the T425 (pngserv.c) and in tpnmtopng's 68030 fallback, so the two give identical files.
 * Written instead of porting pnmtopng.c: that file drives libpng through the 1.0 API (85 direct struct
 * accesses), which the mirror's libpng 1.5.9 no longer has. What it keeps from pnmtopng + libpng defaults:
 *   8-bit gray or RGB, no interlace; libpng's adaptive filter choice (per row, the filter whose output has
 *   the smallest sum of |signed byte|, ties to the lower filter); zlib window 15, memLevel 8, Z_FILTERED;
 *   IDAT chunks of 8192 bytes. maxval < 255 is written as is (pnmtopng would add an sBIT chunk).
 * zlib 1.3.2 is the pinned copy the other T425 ports use. Measured on the real TT (2026-09-25): the files are
 * byte-identical to SpareMiNT's pnmtopng for 800x600 RGB, 800x600 gray and 320x240 RGB. */
#include <stdlib.h>
#include <string.h>
#include "zlib.h"
#include "pngenc.h"

#define PE_IDAT 8192L

static voidpf pe_zalloc(voidpf op, uInt items, uInt size)
{
    (void)op;
    return malloc((size_t)items * size);
}

static void pe_zfree(voidpf op, voidpf p)
{
    (void)op;
    free(p);
}

/* Width, height and channels of a raw P5/P6 file; returns the offset of the first pixel byte, or -1. */
static long pe_header(const unsigned char *p, long n, long *w, long *h, int *ch)
{
    long v[3], pos = 2;
    int k;
    if (n < 11 || p[0] != 'P' || (p[1] != '5' && p[1] != '6'))
        return -1;
    for (k = 0; k < 3; k++) {
        while (pos < n && (p[pos] == ' ' || p[pos] == '\t' || p[pos] == '\n' || p[pos] == '\r'))
            pos++;
        if (pos >= n || p[pos] < '0' || p[pos] > '9')
            return -1;
        for (v[k] = 0; pos < n && p[pos] >= '0' && p[pos] <= '9' && v[k] < 100000L; pos++)
            v[k] = v[k] * 10 + (p[pos] - '0');
    }
    *ch = p[1] == '6' ? 3 : 1;
    if (v[0] < 1 || v[1] < 1 || v[2] < 1 || v[2] > 255 || n - (pos + 1) != v[0] * v[1] * *ch)
        return -1;
    *w = v[0];
    *h = v[1];
    return pos + 1;
}

/* Bounds, from the filtered size raw: zlib at worst stores (5 bytes per 16 KB block + 6); the IDAT framing
 * is 12 bytes per chunk of 8192. */
static long pe_zbound(long raw)
{
    return raw + raw / 1000 + 64;
}

static long pe_frame(long zb)
{
    return 12 * (zb / PE_IDAT + 2);
}

long pe_cap(const unsigned char *pnm, long n)
{
    long w, h, zb;
    int ch;
    if (pe_header(pnm, n, &w, &h, &ch) < 0)
        return -1;
    zb = pe_zbound((w * ch + 1) * h);
    return 8 + 25 + pe_frame(zb) + zb + 12;      /* signature, IHDR, IDAT framing + stream, IEND */
}

static unsigned char *pe_put32(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)(v >> 24); p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8); p[3] = (unsigned char)v;
    return p + 4;
}

/* One chunk of type t whose data already sits at d (len bytes, d == p + 8); fills length, type and CRC. */
static unsigned char *pe_chunk(unsigned char *p, const char *t, long len)
{
    unsigned long crc;
    pe_put32(p, (unsigned long)len);
    memcpy(p + 4, t, 4);
    crc = crc32(0L, p + 4, (uInt)(len + 4));
    return pe_put32(p + 8 + len, crc);
}

/* libpng's default heuristic: try None, Sub, Up, Average, Paeth; keep the smallest sum of |signed byte|
 * (ties keep the earlier filter). best gets the filter byte + the filtered row. */
static void pe_filter(const unsigned char *row, const unsigned char *prev, long len, int bpp,
                      unsigned char *best, unsigned char *tmp)
{
    unsigned long sum, bestsum = 0;
    long i;
    int f;
    for (f = 0; f < 5; f++) {
        unsigned char *o = f == 0 ? best : tmp;
        o[0] = (unsigned char)f;
        for (i = 0, sum = 0; i < len; i++) {
            int a = i >= bpp ? row[i - bpp] : 0, b = prev ? prev[i] : 0, c = i >= bpp && prev ? prev[i - bpp] : 0;
            int pr;
            switch (f) {
            case 0: pr = 0; break;
            case 1: pr = a; break;
            case 2: pr = b; break;
            case 3: pr = (a + b) >> 1; break;
            default: {
                int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
                pr = pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
            }
            }
            o[i + 1] = (unsigned char)(row[i] - pr);
            sum += o[i + 1] < 128 ? o[i + 1] : 256 - o[i + 1];
        }
        if (f == 0)
            bestsum = sum;
        else if (sum < bestsum) {
            bestsum = sum;
            memcpy(best, tmp, (size_t)len + 1);
        }
    }
}

long pe_encode(const unsigned char *pnm, long n, int level, unsigned char *out, long cap)
{
    static const unsigned char sig[8] = { 137, 'P', 'N', 'G', 13, 10, 26, 10 };
    long w, h, off, y, len, zlen, k;
    int ch, ret;
    unsigned char *p, *best, *tmp, *z;
    z_stream s;

    if ((off = pe_header(pnm, n, &w, &h, &ch)) < 0 || level < 0 || level > 9 || cap < pe_cap(pnm, n))
        return -1;
    len = w * ch;
    best = (unsigned char *)malloc((size_t)len + 1);
    tmp = (unsigned char *)malloc((size_t)len + 1);
    if (!best || !tmp) {
        free(best);
        free(tmp);
        return -1;
    }
    memcpy(out, sig, 8);
    p = out + 8;
    pe_put32(p + 8, (unsigned long)w);
    pe_put32(p + 12, (unsigned long)h);
    p[16] = 8; p[17] = (unsigned char)(ch == 3 ? 2 : 0); p[18] = p[19] = p[20] = 0;
    p = pe_chunk(p, "IHDR", 13);
    /* The zlib stream is written pe_frame() bytes past here; the IDAT chunks are then laid over it from the
     * front. Chunk i's framing is 12 * (i + 1) bytes, which the gap covers, so the write never overtakes the
     * part of the stream not yet copied. */
    z = p + pe_frame(pe_zbound((len + 1) * h));
    memset(&s, 0, sizeof s);
    s.zalloc = pe_zalloc;
    s.zfree = pe_zfree;
    if (deflateInit2(&s, level, Z_DEFLATED, 15, 8, Z_FILTERED) != Z_OK) {
        free(best);
        free(tmp);
        return -1;
    }
    s.next_out = z;
    s.avail_out = (uInt)(out + cap - z);
    for (y = 0, ret = Z_OK; y < h && ret == Z_OK; y++) {
        const unsigned char *row = pnm + off + y * len;
        pe_filter(row, y ? row - len : NULL, len, ch, best, tmp);
        s.next_in = best;
        s.avail_in = (uInt)(len + 1);
        ret = deflate(&s, Z_NO_FLUSH);
        if (s.avail_in != 0)
            ret = Z_BUF_ERROR;
    }
    if (ret == Z_OK)
        ret = deflate(&s, Z_FINISH);
    zlen = (long)s.total_out;
    deflateEnd(&s);
    free(best);
    free(tmp);
    if (ret != Z_STREAM_END)
        return -1;
    for (k = 0; k < zlen; k += PE_IDAT) {        /* IDAT chunks of 8192, like libpng's zbuf */
        long c = zlen - k < PE_IDAT ? zlen - k : PE_IDAT;
        memmove(p + 8, z + k, (size_t)c);
        p = pe_chunk(p, "IDAT", c);
    }
    return pe_chunk(p, "IEND", 0) - out;
}
