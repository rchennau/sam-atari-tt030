/* shabench — measure whether the T425 is worth using for yum's SHA-256 (tt030-rpm-pipeline, option 1,
 * operator 2026-09-16). Reports, on one buffer of KB kilobytes:
 *   1. SHA-256 rate on the 68030 (no ATW needed; runs in Hatari too)
 *   2. TT <-> T425 link throughput (op 'E', 4 KB chunks echoed)
 *   3. T425 SHA-256 including the transfer (op 'H'), and whether its digest equals the 68030's
 * Resets the T425: the Dropbear offload server is re-booted by the next SSH login.
 *
 * Usage: shabench [SHASERV.btl] [KB]     (no server -> 68030 only; fpgabios.tos must be resident otherwise)
 * Build: m68k-atari-mint-gcc -m68020-60 -O2 -o shabench.ttp sam-yum-tt/src/shabench.c sam-yum-tt/src/sha256.c
 * Link + boot code below is copied from ../../src/x25519bench/xclient.c (ponytail: extract atwboot.c if a third
 * client needs it).
 */
#include "sha256.h"
#include <mint/osbind.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define OP_CHECK 100
#define OP_WRITE 105
#define OP_READ 106
#define OP_RESET 112

static long short_writes;

/* Write all of buf. The driver's block write returns how many bytes it actually sent, which can be
 * fewer than asked (even 0): iserver's WriteLink loops on it. The first xclient assumed a full write,
 * so a start-up reply could go out partly and leave the T425 waiting mid-packet (2026-09-13).
 * Chunks stay under iserver's 0x7FFF cap; gives up after 5 s without progress. */
static long link_write(const unsigned char *buf, long len)
{
    long done = 0;
    clock_t end = clock() + 5 * CLOCKS_PER_SEC;
    while (done < len) {
        long n = len - done > 0x7000 ? 0x7000 : len - done;
        long r = trap_1_wll(OP_WRITE, (long)(buf + done), n);
        if (r < 0)
            return r;
        if (r < n)
            short_writes++;
        if (r > 0) {
            done += r;
            end = clock() + 5 * CLOCKS_PER_SEC;
        } else if (clock() > end)
            break;
    }
    return done;
}

/* Read exactly len bytes or give up after timeout_ms; returns bytes read. */
static long link_read(unsigned char *buf, long len, long timeout_ms)
{
    long got = 0;
    clock_t end = clock() + timeout_ms * CLOCKS_PER_SEC / 1000;
    while (got < len) {
        long r = trap_1_wll(OP_READ, (long)(buf + got), len - got);
        if (r > 0)
            got += r;
        if (clock() > end)
            break;
    }
    return got;
}

static long ms_since(clock_t t0)
{
    return (long)((clock() - t0) * 1000 / CLOCKS_PER_SEC);
}

static void rate(const char *what, long kb, long ms)
{
    if (ms > 0)
        printf("%-28s %6ld ms  %7ld KB/s\n", what, ms, kb * 1000 / ms);
    else
        printf("%-28s %6ld ms  (below timer resolution)\n", what, ms);
}

/* Boot SERVER.btl raw and answer the runtime's start-up requests, as xclient does. */
static int boot(const char *path)
{
    unsigned char *img;
    long imglen, n;
    clock_t t0;
    FILE *f;

    if ((short)trap_1_ww(OP_CHECK, 0x17) != 0x17) {
        printf("fpgabios not resident (opcode %d check failed)\n", OP_CHECK);
        return 1;
    }
    if (!(f = fopen(path, "rb"))) {
        perror(path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    imglen = ftell(f);
    rewind(f);
    img = malloc(imglen);
    if (!img || fread(img, 1, imglen, f) != (size_t)imglen) {
        printf("cannot read %s\n", path);
        return 1;
    }
    fclose(f);
    trap_1_w(OP_RESET);
    n = link_write(img, imglen);
    free(img);
    if (n != imglen) {
        printf("boot: only %ld of %ld bytes sent\n", n, imglen);
        return 1;
    }
    t0 = clock();
    for (;;) {
        unsigned char pkt[1024], rep[64];
        long len, rl;
        if (link_read(pkt, 2, 10000) != 2)
            break;                             /* quiet: start-up done */
        printf("start-up: packet at +%ld ms\n", (long)((clock() - t0) * 1000 / CLOCKS_PER_SEC));
        if (pkt[0] == 'R' && pkt[1] == 'D') {  /* the server's "RDYk" marker: main() runs, on link k */
            if (link_read(pkt + 2, 2, 2000) == 2)
                printf("start-up done: server ready, marker %.4s\n", pkt);
            break;
        }
        len = pkt[0] | (pkt[1] << 8);
        if (len < 1 || len > (long)sizeof pkt || link_read(pkt, len, 2000) != len) {
            printf("start-up: bad packet (length %ld)\n", len);
            return 1;
        }
        if (pkt[0] == 32 && len >= 3) {        /* SP_GETENV: slice = 2-byte length + name */
            long nl = pkt[1] | (pkt[2] << 8);
            static const char board[] = "#200000";   /* 2 MB, the vendor profile's IBOARDSIZE */
            if (nl == 10 && len >= 13 && !memcmp(pkt + 3, "IBOARDSIZE", 10)) {
                rep[2] = 0;                    /* SP_SUCCESS */
                rep[3] = sizeof board - 1;
                rep[4] = 0;
                memcpy(rep + 5, board, sizeof board - 1);
                rl = 3 + sizeof board - 1;
            } else {
                rep[2] = 129;                  /* SP_ERROR: variable not set, as iserver answers */
                rl = 1;
            }
            printf("start-up: SP_GETENV %.*s answered\n", (int)(nl < 40 ? nl : 40), pkt + 3);
        } else if (pkt[0] == 40) {             /* SP_COMMAND: success + empty command line (serverc.c) */
            rep[2] = 0;
            rep[3] = 0;
            rep[4] = 0;
            rl = 3;
            printf("start-up: SP_COMMAND answered\n");
        } else if (pkt[0] == 42) {             /* SP_ID: success + version, host, OS, board (serverc.c) */
            rep[2] = 0;
            rep[3] = rep[4] = rep[5] = rep[6] = 0;
            rl = 5;
            printf("start-up: SP_ID answered\n");
        } else {
            rep[2] = 1;                        /* SP_UNIMPLEMENTED */
            rl = 1;
            printf("start-up: SP command %d answered unimplemented\n", pkt[0]);
        }
        rep[0] = (unsigned char)rl;
        rep[1] = (unsigned char)(rl >> 8);
        if ((n = link_write(rep, rl + 2)) != rl + 2)
            printf("start-up: reply only %ld of %ld bytes sent\n", n, rl + 2);
    }
    printf("start-up done (%ld short driver writes so far)\n", short_writes);
    (void)t0;
    return 0;
}

static int send_hdr(char op, long len)
{
    unsigned char h[5];
    h[0] = (unsigned char)op;
    h[1] = (unsigned char)len;
    h[2] = (unsigned char)(len >> 8);
    h[3] = (unsigned char)(len >> 16);
    h[4] = (unsigned char)(len >> 24);
    return link_write(h, 5) == 5 ? 0 : 1;
}

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
    if (boot(argv[1]))
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
