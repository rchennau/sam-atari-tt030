/* compbench — Rev. 7 of tt030-rpm-pipeline (operator 2026-09-24: "packages that have to be built or ported
 * should use the transputer by default" — measure first). Same compkern C on both CPUs, same input:
 *   1. 68030: deflate -1, deflate -6, bzip2 -1 on the first KB of FILE
 *   2. TT <-> T425 link throughput (op 'E')
 *   3. T425: each op including sending the input and receiving the output; output must equal the 68030's
 * Resets the T425: the Dropbear offload server is re-booted by the next SSH login.
 *
 * Usage: compbench COMPSERV.BTL|- FILE [KB]      ("-" = 68030 only; fpgabios.tos must be resident otherwise)
 * Build: sam-yum-tt/t4compress.sh (T425 side, prints the sources' patched dir), then Makefile target `comp`.
 * Decision rule (plan Rev. 7): default builds to the T425 only where T425 incl. transfer clearly beats the 68030.
 */
#include "atwboot.h"
#include "compkern.h"
#include "zlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char OPS[] = "zzb";
static const int LEVEL[] = { 1, 6, 1 };
static const char *NAMES[] = { "deflate -1", "deflate -6", "bzip2 -1" };

int main(int argc, char **argv)
{
    long kb = argc > 3 ? atol(argv[3]) : 256, len, cap, i, n, ms, olen[3], ms68[3], t4len;
    unsigned char *buf, *back, *out[3], *t4out, h[4], a[4];
    unsigned long sum, want;
    unsigned char *ipay, *idec;
    long iplen, ms68i;
    clock_t t0;
    FILE *f;
    int k, bad = 0;
    char what[48];

    if (argc < 3) {
        printf("usage: compbench COMPSERV.BTL|- FILE [KB]\n");
        return 2;
    }
    len = kb * 1024;
    cap = len + len / 2 + 1024;
    buf = malloc(len);
    back = malloc(4096);
    t4out = malloc(cap);
    if (!buf || !back || !t4out || !(f = fopen(argv[2], "rb"))) {
        printf("cannot read %s / out of memory\n", argv[2]);
        return 1;
    }
    len = (long)fread(buf, 1, len, f);
    fclose(f);
    kb = len / 1024;
    printf("input %s, %ld KB\n", argv[2], kb);

    for (k = 0; OPS[k]; k++) {
        if (!(out[k] = malloc(cap)))
            return 1;
        t0 = clock();
        olen[k] = ck_compress(OPS[k], LEVEL[k], buf, len, out[k], cap);
        ms68[k] = ms_since(t0);
        sprintf(what, "68030 %s", NAMES[k]);
        rate(what, kb, ms68[k]);
        printf("  -> %ld bytes (%ld%%)\n", olen[k], len ? olen[k] * 100 / len : 0);
    }
    /* inflate (unzip / gunzip's kernel): raw-deflate the input once, then time op 'i' */
    {
        long r = ck_compress('r', 6, buf, len, t4out, cap);
        if (r < 2 || !(ipay = malloc(r + 3)) || !(idec = malloc(len + 64)))
            return 1;
        ipay[0] = (unsigned char)len;
        ipay[1] = (unsigned char)(len >> 8);
        ipay[2] = (unsigned char)(len >> 16);
        ipay[3] = (unsigned char)(len >> 24);
        memcpy(ipay + 4, t4out + 1, r - 1);
        iplen = r + 3;
        t0 = clock();
        n = ck_compress('i', 0, ipay, iplen, idec, len + 64);
        ms68i = ms_since(t0);
        rate("68030 inflate", kb, ms68i);
        printf("  -> %s (%ld bytes in)\n", n == len && !memcmp(idec, buf, len) ? "matches the input" : "WRONG",
               iplen);
    }
    if (!strcmp(argv[1], "-"))
        return 0;
    if (atw_boot(argv[1], "#400000"))
        return 1;

    t0 = clock();
    if (send_op('E', 0, len))
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

    for (k = 0; OPS[k]; k++) {
        t0 = clock();
        if (send_op(OPS[k], LEVEL[k], len) || link_write(buf, len) != len || link_read(h, 4, 300000) != 4) {
            printf("T425 %s: no reply\n", NAMES[k]);
            return 1;
        }
        t4len = h[0] | ((long)h[1] << 8) | ((long)h[2] << 16) | ((long)h[3] << 24);
        if (t4len < 0 || t4len > cap || link_read(a, 4, 5000) != 4) {
            printf("T425 %s: failed (length %ld)\n", NAMES[k], t4len);
            return 1;
        }
        if (link_read(t4out, t4len, 60000) != t4len) {
            printf("T425 %s: short read\n", NAMES[k]);
            return 1;
        }
        ms = ms_since(t0);
        sum = a[0] | ((unsigned long)a[1] << 8) | ((unsigned long)a[2] << 16) | ((unsigned long)a[3] << 24);
        want = adler32(adler32(0L, Z_NULL, 0), out[k], (uInt)olen[k]);
        if (sum != want || t4len != olen[k])
            printf("  T425 result %s (its adler32 %08lx, 68030 %08lx, lengths %ld/%ld)\n",
                   sum == want ? "right" : "WRONG", sum, want, t4len, olen[k]);
        else if (adler32(adler32(0L, Z_NULL, 0), t4out, (uInt)t4len) != sum)
            printf("  T425 result right, DAMAGED on the link\n");
        sprintf(what, "T425 %s incl. transfer", NAMES[k]);
        rate(what, kb, ms);
        n = t4len == olen[k] && !memcmp(t4out, out[k], t4len);
        bad |= !n;
        printf("  -> output %s the 68030's; T425/68030 time %ld.%02ld\n", n ? "matches" : "DIFFERS from",
               ms68[k] ? ms / ms68[k] : 0, ms68[k] ? ms * 100 / ms68[k] % 100 : 0);
    }
    t0 = clock();
    if (send_op('i', 0, iplen) || link_write(ipay, iplen) != iplen || link_read(h, 4, 300000) != 4) {
        printf("T425 inflate: no reply\n");
        return 1;
    }
    t4len = h[0] | ((long)h[1] << 8) | ((long)h[2] << 16) | ((long)h[3] << 24);
    if (t4len != len || link_read(a, 4, 5000) != 4 || link_read(t4out, t4len, 60000) != t4len) {
        printf("T425 inflate: failed (length %ld)\n", t4len);
        return 1;
    }
    ms = ms_since(t0);
    rate("T425 inflate incl. transfer", kb, ms);
    n = !memcmp(t4out, buf, len);
    bad |= !n;
    printf("  -> output %s the input; T425/68030 time %ld.%02ld\n", n ? "matches" : "DIFFERS from",
           ms68i ? ms / ms68i : 0, ms68i ? ms * 100 / ms68i % 100 : 0);
    printf("(%ld short driver writes)\n", short_writes);
    return bad;
}
