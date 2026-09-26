/* t4serv — the generic T425 side of every kernel family (tt030-t425-kernel-ports FR-2). Booted raw by
 * t4call() on the TT. After "RDY0" on link 0, per request:
 *   TT -> T425  op (1) || level (1) || len (4, little-endian) || len bytes
 *   op 'E': each 4096-byte chunk echoed back at once (link throughput, every server has it)
 *   other : t4_kernel(op, level) on the buffer, then
 *   T425 -> TT  outlen (4, LE; 0xffffffff = failed) || adler32(out) (4) || outlen bytes
 * The adler32 (t4adler.c, no zlib needed) tells a wrong result from a damaged transfer.
 * A family links this with its t4_kernel()
 * and t4_out_cap() (compserv.c). t4call() boots it with IBOARDSIZE #500000.
 * UNVERIFIED LIMIT (defect 2026-09-25): memserv.c "measured" 4.94 MB usable by filling T425 memory up to 7 MB,
 * and that run's pattern was later found at the start of the ATW800/2 video RAM: the HDMI desktop was black
 * until a power cycle. How T425 memory maps onto the display is not understood. What IS measured: normal ports
 * (tgzip, tppmquant ~4.2 MB) run at this size without touching the display (camera + atwvid, 2026-09-25).
 * memserv.c is retired; never fill or probe board memory beyond what a port itself allocates.
 * RULE for every family: nothing linked into a server may call the host runtime (getenv, fopen, printf,
 * time, exit...). On the T425 those are requests over link 0 to an iserver; after boot the TT answers
 * none, so the request reads as a garbage reply and the server hangs. The t4 emulator IS a host server
 * and answers them, so an emulator test cannot catch this (libjpeg's getenv("JPEGMEM"), 2026-09-24).
 * History: this loop was compserv.c (Rev. 7 benchmark), extracted 2026-09-24.
 */
#include <channel.h>
#include <stdlib.h>
#include "t4serv.h"
#include "t4adler.h"

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
        in = malloc(len ? len : 1);
        for (i = 0; i < len; i += n) {         /* always drain the payload, even with no memory */
            n = len - i > CHUNK ? CHUNK : len - i;
            ChanIn(LINK0IN, in ? (char *)in + i : (char *)chunk, (int)n);
        }
        /* sized after the payload is in: a decoder's output size is inside it (lhaserv op 'd') */
        cap = in ? t4_out_cap(hdr[0], hdr[1], in, (long)len) : 0;
        if (cap == T4_IN_PLACE) {
            out = in;
            cap = (long)len;
        } else
            out = cap > 0 ? malloc((size_t)cap) : NULL;
        olen = in && out ? t4_kernel(hdr[0], hdr[1], in, (long)len, out, cap) : -1;
        put4(olen < 0 ? 0xffffffffUL : (unsigned long)olen);
        if (olen > 0) {
            put4(t4_adler32(out, olen));
            ChanOut(LINK0OUT, (char *)out, (int)olen);
        }
        if (out && out != in)                  /* free(NULL) is not safe to assume on this runtime */
            free(out);
        if (in)
            free(in);
    }
    return 0;
}
