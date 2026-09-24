/* atwboot — TT side of the ATW800/2 T425 link, shared by shabench and compbench (extracted from
 * shabench on 2026-09-24, the third client the ponytail note asked for). Needs fpgabios.tos resident.
 * Link + boot code first written in ../../src/x25519bench/xclient.c. */
#include "atwboot.h"
#include <mint/osbind.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

int atw_verbose = 1;

/* Start-up chatter goes to stdout for the benchmarks; tgzip turns it off (its stdout is the data). */
static void atw_log(const char *fmt, ...)
{
    va_list ap;
    if (!atw_verbose)
        return;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

#define OP_CHECK 100
#define OP_WRITE 105
#define OP_READ 106
#define OP_RESET 112

long short_writes;

/* Write all of buf. The driver's block write returns how many bytes it actually sent, which can be
 * fewer than asked (even 0): iserver's WriteLink loops on it. The first xclient assumed a full write,
 * so a start-up reply could go out partly and leave the T425 waiting mid-packet (2026-09-13).
 * Chunks stay under iserver's 0x7FFF cap; gives up after 5 s without progress. */
long link_write(const unsigned char *buf, long len)
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

#ifndef ATW_READ_CAP
#define ATW_READ_CAP 0x7000L
#endif

/* Read exactly len bytes or give up after timeout_ms; returns bytes read. Each driver call asks for at
 * most ATW_READ_CAP bytes: iserver's ReadLink refuses more than 0x7FFF (a 16-bit length in the driver).
 * Asking for ~96 KB in one call returned the right COUNT of wrong bytes and left the stream out of step
 * (measured 2026-09-24); link_write has always capped its calls, which is why writes were never hit. */
long link_read(unsigned char *buf, long len, long timeout_ms)
{
    long got = 0;
    clock_t end = clock() + timeout_ms * CLOCKS_PER_SEC / 1000;
    while (got < len) {
        long n = len - got > ATW_READ_CAP ? ATW_READ_CAP : len - got;
        long r = trap_1_wll(OP_READ, (long)(buf + got), n);
        if (r > 0)
            got += r;
        if (clock() > end)
            break;
    }
    return got;
}

long ms_since(clock_t t0)
{
    return (long)((clock() - t0) * 1000 / CLOCKS_PER_SEC);
}

void rate(const char *what, long kb, long ms)
{
    if (ms > 0)
        printf("%-28s %6ld ms  %7ld KB/s\n", what, ms, kb * 1000 / ms);
    else
        printf("%-28s %6ld ms  (below timer resolution)\n", what, ms);
}

/* Boot SERVER.btl raw and answer the runtime's start-up requests, as xclient does. */
int atw_boot(const char *path, const char *board)
{
    unsigned char *img;
    long imglen, n;
    clock_t t0;
    FILE *f;

    if ((short)trap_1_ww(OP_CHECK, 0x17) != 0x17) {
        atw_log("fpgabios not resident (opcode %d check failed)\n", OP_CHECK);
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
        atw_log("cannot read %s\n", path);
        return 1;
    }
    fclose(f);
    trap_1_w(OP_RESET);
    n = link_write(img, imglen);
    free(img);
    if (n != imglen) {
        atw_log("boot: only %ld of %ld bytes sent\n", n, imglen);
        return 1;
    }
    t0 = clock();
    for (;;) {
        unsigned char pkt[1024], rep[64];
        long len, rl;
        if (link_read(pkt, 2, 10000) != 2)
            break;                             /* quiet: start-up done */
        atw_log("start-up: packet at +%ld ms\n", (long)((clock() - t0) * 1000 / CLOCKS_PER_SEC));
        if (pkt[0] == 'R' && pkt[1] == 'D') {  /* the server's "RDYk" marker: main() runs, on link k */
            if (link_read(pkt + 2, 2, 2000) == 2)
                atw_log("start-up done: server ready, marker %.4s\n", pkt);
            break;
        }
        len = pkt[0] | (pkt[1] << 8);
        if (len < 1 || len > (long)sizeof pkt || link_read(pkt, len, 2000) != len) {
            atw_log("start-up: bad packet (length %ld)\n", len);
            return 1;
        }
        if (pkt[0] == 32 && len >= 3) {        /* SP_GETENV: slice = 2-byte length + name */
            long nl = pkt[1] | (pkt[2] << 8);
            if (nl == 10 && len >= 13 && !memcmp(pkt + 3, "IBOARDSIZE", 10)) {
                long bl = (long)strlen(board);
                rep[2] = 0;                    /* SP_SUCCESS */
                rep[3] = (unsigned char)bl;
                rep[4] = 0;
                memcpy(rep + 5, board, bl);
                rl = 3 + bl;
            } else {
                rep[2] = 129;                  /* SP_ERROR: variable not set, as iserver answers */
                rl = 1;
            }
            atw_log("start-up: SP_GETENV %.*s answered\n", (int)(nl < 40 ? nl : 40), pkt + 3);
        } else if (pkt[0] == 40) {             /* SP_COMMAND: success + empty command line (serverc.c) */
            rep[2] = 0;
            rep[3] = 0;
            rep[4] = 0;
            rl = 3;
            atw_log("start-up: SP_COMMAND answered\n");
        } else if (pkt[0] == 42) {             /* SP_ID: success + version, host, OS, board (serverc.c) */
            rep[2] = 0;
            rep[3] = rep[4] = rep[5] = rep[6] = 0;
            rl = 5;
            atw_log("start-up: SP_ID answered\n");
        } else {
            rep[2] = 1;                        /* SP_UNIMPLEMENTED */
            rl = 1;
            atw_log("start-up: SP command %d answered unimplemented\n", pkt[0]);
        }
        rep[0] = (unsigned char)rl;
        rep[1] = (unsigned char)(rl >> 8);
        if ((n = link_write(rep, rl + 2)) != rl + 2)
            atw_log("start-up: reply only %ld of %ld bytes sent\n", n, rl + 2);
    }
    atw_log("start-up done (%ld short driver writes so far)\n", short_writes);
    (void)t0;
    return 0;
}

int send_hdr(char op, long len)
{
    unsigned char h[5];
    h[0] = (unsigned char)op;
    h[1] = (unsigned char)len;
    h[2] = (unsigned char)(len >> 8);
    h[3] = (unsigned char)(len >> 16);
    h[4] = (unsigned char)(len >> 24);
    return link_write(h, 5) == 5 ? 0 : 1;
}

int send_op(char op, int level, long len)
{
    unsigned char h[6];
    h[0] = (unsigned char)op;
    h[1] = (unsigned char)level;
    h[2] = (unsigned char)len;
    h[3] = (unsigned char)(len >> 8);
    h[4] = (unsigned char)(len >> 16);
    h[5] = (unsigned char)(len >> 24);
    return link_write(h, 6) == 6 ? 0 : 1;
}
