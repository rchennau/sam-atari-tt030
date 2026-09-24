/* compserv — T425 side of the Rev. 7 benchmark: does a build-on-miss package gain from the transputer?
 * Booted raw by compbench, as shaserv is by shabench (atwboot.c). Protocol on link 0, after "RDY0":
 *   TT -> T425  op (1) || len (4, little-endian) || len bytes
 *   op 'E': each 4096-byte chunk echoed back at once                     (link throughput)
 *   op 'z' | 'Z' | 'b': compkern compresses the buffer, then
 *   T425 -> TT  outlen (4, little-endian; 0xffffffff = failed) || adler32(out) (4), then per 4096-byte
 *               chunk: TT -> T425 one byte ("send"), T425 -> TT the chunk.
 * Measured 2026-09-24: the T425 streaming a ~96 KB result in one go reached the TT DAMAGED (its adler32
 * right, the bytes wrong, the stream then misaligned), while the TT-paced 4 KB echo is always clean. So
 * the TT paces every chunk; the checksum still tells a wrong result from a damaged transfer.
 * Needs IBOARDSIZE #400000: bzip2 -1 wants ~1.2 MB beside the in/out buffers (atwboot answers it).
 */
#include <channel.h>
#include <stdlib.h>
#include "compkern.h"
#include "zlib.h"

#define CHUNK 4096

static void put4(unsigned long v)
{
    unsigned char b[4];
    b[0] = (unsigned char)v;
    b[1] = (unsigned char)(v >> 8);
    b[2] = (unsigned char)(v >> 16);
    b[3] = (unsigned char)(v >> 24);
    ChanOut(LINK0OUT, (char *)b, 4);
}

int main(void)
{
    static char ready[4] = { 'R', 'D', 'Y', '0' };
    static unsigned char chunk[CHUNK];
    unsigned char hdr[5], *in, *out;
    unsigned long len, n, i;
    long olen, cap;

    ChanOut(LINK0OUT, ready, 4);
    for (;;) {
        ChanIn(LINK0IN, (char *)hdr, 5);
        len = hdr[1] | ((unsigned long)hdr[2] << 8) | ((unsigned long)hdr[3] << 16) | ((unsigned long)hdr[4] << 24);
        if (hdr[0] == 'E') {
            for (; len > 0; len -= n) {
                n = len > CHUNK ? CHUNK : len;
                ChanIn(LINK0IN, (char *)chunk, (int)n);
                ChanOut(LINK0OUT, (char *)chunk, (int)n);
            }
            continue;
        }
        cap = (long)(len + len / 2 + 1024);
        in = malloc(len ? len : 1);
        out = malloc((size_t)cap);
        for (i = 0; i < len; i += n) {         /* always drain the payload, even with no memory */
            n = len - i > CHUNK ? CHUNK : len - i;
            ChanIn(LINK0IN, in ? (char *)in + i : (char *)chunk, (int)n);
        }
        olen = in && out ? ck_compress(hdr[0], in, (long)len, out, cap) : -1;
        put4(olen < 0 ? 0xffffffffUL : (unsigned long)olen);
        if (olen > 0) {
            put4(adler32(adler32(0L, Z_NULL, 0), out, (uInt)olen));
            for (i = 0; i < (unsigned long)olen; i += n) {
                n = (unsigned long)olen - i > CHUNK ? CHUNK : (unsigned long)olen - i;
                ChanIn(LINK0IN, (char *)hdr, 1);
                ChanOut(LINK0OUT, (char *)out + i, (int)n);
            }
        }
        free(in);
        free(out);
    }
    return 0;
}
