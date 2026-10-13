/* linkrt — 68030 <-> T425 link round-trip benchmark (atari-tt030-enhancement, operator handoff 2026-10-12 §7
 * step 1). Boots a t4serv-family server (any: every one answers op 'E') and times N echo round-trips per size
 * through the TT link driver, the hop every job that starts on the TT pays first. Same sizes as the TRAM
 * benchmark (sam-tram-tt/tramlink). Resets the T425: the Dropbear offload server re-boots on the next SSH login.
 * Usage: linkrt [SERVER.BTL]   (default /usr/lib/t425/compserv.btl; fpgabios.tos must be resident)
 * Two passes: header and payload as separate driver writes, then merged into one write (one trap fewer).
 */
#include "atwboot.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

int main(int argc, char **argv)
{
    static const long sizes[6] = { 4, 64, 256, 1024, 4096, 16384 };
    static unsigned char buf[16390], back[16384];
    long s, i, n, ms, iters, o, c, merged;
    clock_t t0;
    for (i = 6; i < (long)sizeof buf; i++)
        buf[i] = (unsigned char)(i * 7 + 1);
    atw_verbose = 0;
    if (atw_boot(argc > 1 ? argv[1] : "/usr/lib/t425/compserv.btl", "#400000"))
        return 1;
  for (merged = 0; merged <= 1; merged++) {
    printf("%s\nsize   iters  us/round-trip  KB/s per direction\n",
           merged ? "header merged into the first chunk's write" : "header and payload as separate writes");
    for (s = 0; s < 6; s++) {
        n = sizes[s];
        iters = n <= 256 ? 400 : n <= 4096 ? 100 : 30;
        t0 = clock();
        for (i = 0; i < iters; i++) {
            unsigned char *p = buf + 6;
            if (merged) {                  /* op || level || len (LE) in front of the payload, one write */
                buf[0] = 'E'; buf[1] = 0; buf[2] = n; buf[3] = n >> 8; buf[4] = n >> 16; buf[5] = n >> 24;
            } else if (send_op('E', 0, n))
                return 1;
            /* t4serv echoes each 4096-byte chunk at once and the link is unbuffered: interleave or deadlock */
            for (o = 0; o < n; o += c) {
                long h = merged && o == 0 ? 6 : 0;
                c = n - o > 4096 ? 4096 : n - o;
                if (link_write(p + o - h, c + h) != c + h || link_read(back + o, c, 5000) != c) {
                    printf("echo failed: size %ld iteration %ld\n", n, i);
                    return 1;
                }
            }
            if (memcmp(back, p, n)) {
                printf("echo damaged: size %ld iteration %ld\n", n, i);
                return 1;
            }
        }
        ms = ms_since(t0);
        /* both directions counted, as in tramlink: the transfer rate while a direction is busy */
        printf("%5ld  %5ld  %10ld     %ld\n", n, iters, ms * 1000 / iters, ms ? 2 * n * iters / ms : 0);
    }
  }
    printf("short driver writes: %ld\n", short_writes);
    return 0;
}
