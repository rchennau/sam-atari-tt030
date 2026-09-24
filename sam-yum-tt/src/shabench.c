/* shabench — measure whether the T425 is worth using for yum's SHA-256 (tt030-rpm-pipeline, option 1,
 * operator 2026-09-16). Reports, on one buffer of KB kilobytes:
 *   1. SHA-256 rate on the 68030 (no ATW needed; runs in Hatari too)
 *   2. TT <-> T425 link throughput (op 'E', 4 KB chunks echoed)
 *   3. T425 SHA-256 including the transfer (op 'H'), and whether its digest equals the 68030's
 * Resets the T425: the Dropbear offload server is re-booted by the next SSH login.
 *
 * Usage: shabench [SHASERV.btl] [KB]     (no server -> 68030 only; fpgabios.tos must be resident otherwise)
 * Build: m68k-atari-mint-gcc -m68020-60 -O2 -o shabench.ttp sam-yum-tt/src/shabench.c sam-yum-tt/src/sha256.c
 * Link + boot code: atwboot.c.
 */
#include "sha256.h"
#include "atwboot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main(int argc, char **argv)
{
    long kb = argc > 2 ? atol(argv[2]) : 512, len = kb * 1024, i, n, ms;
    unsigned char *buf, *back, d68[32], dt4[32];
    struct sha256 s;
    clock_t t0;
    int j;

    buf = malloc(len);
    back = malloc(4096);
    if (!buf || !back) {
        printf("out of memory for %ld KB\n", kb);
        return 1;
    }
    for (i = 0; i < len; i++)
        buf[i] = (unsigned char)(i * 7 + 3);

    t0 = clock();
    sha256_init(&s);
    sha256_update(&s, buf, len);
    sha256_final(&s, d68);
    rate("68030 sha256", kb, ms_since(t0));
    printf("digest ");
    for (j = 0; j < 32; j++)
        printf("%02x", d68[j]);
    printf("\n");

    if (argc < 2)
        return 0;
    if (atw_boot(argv[1], "#200000"))
        return 1;

    t0 = clock();
    if (send_hdr('E', len))
        return 1;
    for (i = 0; i < len; i += n) {
        n = len - i > 4096 ? 4096 : len - i;
        if (link_write(buf + i, n) != n || link_read(back, n, 5000) != n || memcmp(back, buf + i, n)) {
            printf("echo failed at byte %ld\n", i);
            return 1;
        }
    }
    ms = ms_since(t0);
    rate("link echo (both ways)", kb, ms);
    rate("link one way (echo / 2)", kb, ms / 2);

    t0 = clock();
    if (send_hdr('H', len) || link_write(buf, len) != len || link_read(dt4, 32, 60000) != 32) {
        printf("T425 hash: no reply\n");
        return 1;
    }
    rate("T425 sha256 incl. transfer", kb, ms_since(t0));
    printf("T425 digest %s the 68030's (%ld short driver writes)\n",
           memcmp(d68, dt4, 32) ? "DIFFERS from" : "matches", short_writes);
    return memcmp(d68, dt4, 32) != 0;
}
