/* compkern — see compkern.h. zlib is built Z_SOLO (the INMOS toolset has no <sys/types.h>), so it
 * gets its allocator from here; bzip2 is built BZ_NO_STDIO, so bz_internal_error lives here too. */
#include <stdlib.h>
#include "zlib.h"
#include "bzlib.h"
#include "compkern.h"

static voidpf z_alloc(voidpf opaque, uInt items, uInt size)
{
    (void)opaque;
    return malloc((size_t)items * size);
}

static void z_free(voidpf opaque, voidpf p)
{
    (void)opaque;
    free(p);
}

void bz_internal_error(int code)
{
    (void)code;
    abort();
}

static long deflate_buf(int level, int bits, const unsigned char *in, long n, unsigned char *out, long cap,
                        int *data_type)
{
    z_stream s;
    int r;
    long len;

    s.zalloc = z_alloc;
    s.zfree = z_free;
    s.opaque = 0;
    if (deflateInit2(&s, level, Z_DEFLATED, bits, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        return -1;
    s.next_in = (Bytef *)in;
    s.avail_in = (uInt)n;
    s.next_out = out;
    s.avail_out = (uInt)cap;
    r = deflate(&s, Z_FINISH);
    len = (long)s.total_out;
    if (data_type)
        *data_type = s.data_type;
    deflateEnd(&s);
    return r == Z_STREAM_END ? len : -1;
}

/* Raw inflate (the inverse of op 'r'): payload = original size (4, LE) || raw deflate stream. */
static long inflate_raw(const unsigned char *in, long n, unsigned char *out, long cap)
{
    z_stream s;
    long want;
    int r;

    if (n < 4)
        return -1;
    want = in[0] | ((long)in[1] << 8) | ((long)in[2] << 16) | ((long)in[3] << 24);
    if (want > cap)
        return -1;
    s.zalloc = z_alloc;
    s.zfree = z_free;
    s.opaque = 0;
    s.next_in = (Bytef *)in + 4;
    s.avail_in = (uInt)(n - 4);
    if (inflateInit2(&s, -15) != Z_OK)
        return -1;
    s.next_out = out;
    s.avail_out = (uInt)cap;
    r = inflate(&s, Z_FINISH);
    inflateEnd(&s);
    return r == Z_STREAM_END && (long)s.total_out == want ? want : -1;
}

long ck_compress(int op, int level, const unsigned char *in, long n, unsigned char *out, long cap)
{
    unsigned int len = (unsigned int)cap;

    if (op == 'i')
        return inflate_raw(in, n, out, cap);
    if (level < 1 || level > 9)
        return -1;
    if (op == 'z')
        return deflate_buf(level, 15, in, n, out, cap, NULL);
    if (op == 'g')                               /* 15 + 16: gzip header and trailer instead of zlib's */
        return deflate_buf(level, 31, in, n, out, cap, NULL);
    if (op == 'r') {                             /* zip's zlib path: raw, memLevel 8, default strategy */
        int type = 2;                            /* Z_UNKNOWN */
        long len = cap < 2 ? -1 : deflate_buf(level, -15, in, n, out + 1, cap - 1, &type);
        if (len < 0)
            return -1;
        out[0] = (unsigned char)type;            /* zip stores ASCII/BINARY from zlib's data_type */
        return len + 1;
    }
    if (op == 'b')
        return BZ2_bzBuffToBuffCompress((char *)out, &len, (char *)in, (unsigned int)n, level, 0, 0) == BZ_OK
            ? (long)len : -1;
    return -1;
}
