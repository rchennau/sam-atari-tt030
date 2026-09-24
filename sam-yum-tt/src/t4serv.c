/* t4serv — the generic T425 side of every kernel family (tt030-t425-kernel-ports FR-2). Booted raw by
 * t4call() on the TT. After "RDY0" on link 0, per request:
 *   TT -> T425  op (1) || level (1) || len (4, little-endian) || len bytes
 *   op 'E': each 4096-byte chunk echoed back at once (link throughput, every server has it)
 *   other : t4_kernel(op, level) on the buffer, then
 *   T425 -> TT  outlen (4, LE; 0xffffffff = failed) || adler32(out) (4) || outlen bytes
 * The adler32 tells a wrong result from a damaged transfer. A family links this with its t4_kernel()
 * and t4_out_cap() (compserv.c). Needs IBOARDSIZE #400000, which t4call() answers at boot.
 * History: this loop was compserv.c (Rev. 7 benchmark), extracted 2026-09-24.
 */
#include <channel.h>
#include <stdlib.h>
#include "t4serv.h"
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
        cap = t4_out_cap((long)len);
        in = malloc(len ? len : 1);
        out = malloc((size_t)cap);
        for (i = 0; i < len; i += n) {         /* always drain the payload, even with no memory */
            n = len - i > CHUNK ? CHUNK : len - i;
            ChanIn(LINK0IN, in ? (char *)in + i : (char *)chunk, (int)n);
        }
        olen = in && out ? t4_kernel(hdr[0], hdr[1], in, (long)len, out, cap) : -1;
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
