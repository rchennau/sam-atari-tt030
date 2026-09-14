/* xclient — TT-side test for the T425 X25519 offload (kanban a764de13 step 2), before any Dropbear change.
 * Talks to the ATW800/2 link through fpgabios.tos's GEMDOS extension (opcodes from iserver's hardst.s):
 *   100 presence (arg 0x17 -> 0x17), 112 reset T425, 105 block write (buf, len), 106 block read (buf, len).
 * Boots xserv raw (reset + image bytes, as iserver's Boot() does), then times requests and checks them
 * against RFC 7748 §6.1 (Alice).
 *
 * Usage: xclient SERVER.btl [reps] [quiet-ms]      (fpgabios.tos must be resident)
 * Build: m68k-atari-mint-gcc -m68020-60 -O2 -o xclient.ttp src/x25519bench/xclient.c
 */
#include <mint/osbind.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define OP_CHECK 100
#define OP_WRITE 105
#define OP_READ 106
#define OP_RESET 112

static const unsigned char alice_sk[32] = {
    0x77, 0x07, 0x6d, 0x0a, 0x73, 0x18, 0xa5, 0x7d, 0x3c, 0x16, 0xc1, 0x72, 0x51, 0xb2, 0x66, 0x45,
    0xdf, 0x4c, 0x2f, 0x87, 0xeb, 0xc0, 0x99, 0x2a, 0xb1, 0x77, 0xfb, 0xa5, 0x1d, 0xb9, 0x2c, 0x2a };
static const unsigned char alice_pk[32] = {
    0x85, 0x20, 0xf0, 0x09, 0x89, 0x30, 0xa7, 0x54, 0x74, 0x8b, 0x7d, 0xdc, 0xb4, 0x3e, 0xf7, 0x5a,
    0x0d, 0xbf, 0x3a, 0x0d, 0x26, 0x38, 0x1a, 0xf4, 0xeb, 0xa4, 0xa9, 0x8e, 0xaa, 0x9b, 0x4e, 0x6a };

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

int main(int argc, char **argv)
{
    int reps = argc > 2 ? atoi(argv[2]) : 3, i;
    long quiet_ms = argc > 3 ? atol(argv[3]) : 10000;  /* start-up quiet window; 1.5 s proved too short? */
    unsigned char req[64], out[32], *img;
    long imglen, n;
    clock_t t0, t1;
    FILE *f;

    if (argc < 2) {
        fprintf(stderr, "usage: xclient SERVER.btl [reps]\n");
        return 2;
    }
    if ((short)trap_1_ww(OP_CHECK, 0x17) != 0x17) {
        printf("fpgabios not resident (opcode %d check failed)\n", OP_CHECK);
        return 1;
    }
    if (!(f = fopen(argv[1], "rb"))) {
        perror(argv[1]);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    imglen = ftell(f);
    rewind(f);
    img = malloc(imglen);
    if (!img || fread(img, 1, imglen, f) != (size_t)imglen) {
        printf("cannot read %s\n", argv[1]);
        return 1;
    }
    fclose(f);

    t0 = clock();
    trap_1_w(OP_RESET);
    n = link_write(img, imglen);
    t1 = clock();
    printf("boot: reset + %ld of %ld bytes in %ld ms (%ld short driver writes)\n", n, imglen,
           (long)((t1 - t0) * 1000 / CLOCKS_PER_SEC), short_writes);

    /* Even the host-less INMOS C runtime (startrd.lnk) asks its host for the environment variable
     * IBOARDSIZE before main(): measured on the TT after the first raw boot, the T425 sent
     * `0e 00 20 0a 00 "IBOARDSIZE" d2` (SP_GETENV) and waited. Answer start-up requests the way iserver
     * does (hostc.c SpGetenv, pack.h framing: 2-byte LE payload length, then payload) until the T425
     * goes quiet, then the link is ours. */
    t0 = clock();
    for (;;) {
        unsigned char pkt[1024], rep[64];
        long len, rl;
        if (link_read(pkt, 2, quiet_ms) != 2)
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


    memcpy(req, alice_sk, 32);
    memset(req + 32, 0, 32);
    req[32] = 9;                               /* base point u = 9 */
    for (i = 0; i < reps; i++) {
        t0 = clock();
        n = link_write(req, 64);
        n = n == 64 ? link_read(out, 32, 20000) : n;
        t1 = clock();
        printf("x25519 via T425: %ld ms, %s\n", (long)((t1 - t0) * 1000 / CLOCKS_PER_SEC),
               n != 32 ? "NO REPLY (got fewer than 32 bytes)" :
               memcmp(out, alice_pk, 32) ? "WRONG" : "matches RFC 7748");
        if (n != 32)
            return 1;
    }
    return 0;
}
