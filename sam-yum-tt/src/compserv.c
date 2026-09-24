/* compserv — T425 side of the Rev. 7 benchmark: does a build-on-miss package gain from the transputer?
 * Booted raw by compbench, as shaserv is by shabench (atwboot.c). Protocol on link 0, after "RDY0":
 *   TT -> T425  op (1) || level (1) || len (4, little-endian) || len bytes
 *   op 'E': each 4096-byte chunk echoed back at once                     (link throughput)
 *   op 'z' | 'g' | 'b': ck_compress(op, level) on the buffer, then
 *   T425 -> TT  outlen (4, little-endian; 0xffffffff = failed) || adler32(out) (4) || outlen bytes
 * The checksum tells a wrong result from a damaged transfer. It found one on 2026-09-24: a ~96 KB result
 * arrived damaged because the TT asked the driver for it in ONE read call, over the driver's 0x7FFF
 * limit — not an FPGA or T425 fault. atwboot's link_read now caps each call (A/B on the same server:
 * uncapped damaged, capped byte-identical), so this streams again; a TT-paced 4 KB workaround is gone.
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
    unsigned char hdr[6], *in, *out;
    unsigned long len, n, i;
    long olen, cap;

    ChanOut(LINK0OUT, ready, 4);
    for (;;) {
        ChanIn(LINK0IN, (char *)hdr, 6);
        len = hdr[2] | ((unsigned long)hdr[3] << 8) | ((unsigned long)hdr[4] << 16) | ((unsigned long)hdr[5] << 24);
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
        olen = in && out ? ck_compress(hdr[0], hdr[1], in, (long)len, out, cap) : -1;
        put4(olen < 0 ? 0xffffffffUL : (unsigned long)olen);
        if (olen > 0) {
            put4(adler32(adler32(0L, Z_NULL, 0), out, (uInt)olen));
            ChanOut(LINK0OUT, (char *)out, (int)olen);
        }
        free(in);
        free(out);
    }
    return 0;
}
