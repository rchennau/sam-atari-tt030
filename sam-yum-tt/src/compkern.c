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

static long deflate_buf(int level, const unsigned char *in, long n, unsigned char *out, long cap)
{
    z_stream s;
    int r;
    long len;

    s.zalloc = z_alloc;
    s.zfree = z_free;
    s.opaque = 0;
    if (deflateInit(&s, level) != Z_OK)
        return -1;
    s.next_in = (Bytef *)in;
    s.avail_in = (uInt)n;
    s.next_out = out;
    s.avail_out = (uInt)cap;
    r = deflate(&s, Z_FINISH);
    len = (long)s.total_out;
    deflateEnd(&s);
    return r == Z_STREAM_END ? len : -1;
}

long ck_compress(int op, const unsigned char *in, long n, unsigned char *out, long cap)
{
    unsigned int len = (unsigned int)cap;

    if (op == 'z')
        return deflate_buf(1, in, n, out, cap);
    if (op == 'Z')
        return deflate_buf(6, in, n, out, cap);
    if (op == 'b')
        return BZ2_bzBuffToBuffCompress((char *)out, &len, (char *)in, (unsigned int)n, 1, 0, 0) == BZ_OK
            ? (long)len : -1;
    return -1;
}
