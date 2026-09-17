/* shaserv — T425 side of the yum-offload measurement (tt030-rpm-pipeline, option 1, 2026-09-16).
 * Booted raw by shabench exactly as xserv is (see ../../src/x25519bench/xserv.c for the start-up story).
 * Protocol on link 0, after "RDY0":
 *   TT -> T425  op (1) || len (4, little-endian)
 *   op 'E': len bytes in 4096-byte chunks, each echoed back at once   (link throughput)
 *   op 'H': len bytes in 4096-byte chunks, then T425 -> TT 32-byte SHA-256 (T425 hash rate)
 * Build: see ../../src/x25519bench/BUILD.txt, with shaserv.c + sha256.c and shaserv.lnk.
 */
#include <channel.h>
#include "sha256.h"

#define CHUNK 4096

int main(void)
{
    static char ready[4] = { 'R', 'D', 'Y', '0' };
    static unsigned char buf[CHUNK];
    unsigned char hdr[5], out[32];
    unsigned long len, n;
    struct sha256 s;

    ChanOut(LINK0OUT, ready, 4);
    for (;;) {
        ChanIn(LINK0IN, (char *)hdr, 5);
        len = hdr[1] | ((unsigned long)hdr[2] << 8) | ((unsigned long)hdr[3] << 16) | ((unsigned long)hdr[4] << 24);
        sha256_init(&s);
        while (len > 0) {
            n = len > CHUNK ? CHUNK : len;
            ChanIn(LINK0IN, (char *)buf, (int)n);
            if (hdr[0] == 'E')
                ChanOut(LINK0OUT, (char *)buf, (int)n);
            else
                sha256_update(&s, buf, n);
            len -= n;
        }
        if (hdr[0] == 'H') {
            sha256_final(&s, out);
            ChanOut(LINK0OUT, (char *)out, 32);
        }
    }
    return 0;
}
