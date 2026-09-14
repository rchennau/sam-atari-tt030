/* atwxserv — boot the X25519 server onto the ATW800/2's T425 once, at system start, then exit leaving
 * it running (kanban 1f0125295, item 2). Called from mint.cnf AFTER fpgabios.tos and BEFORE rc.dropbear.
 * Dropbear then talks to the already-running server, so no login pays the ~0.9 s boot or resets the
 * T425 (repeated resets are what disturbed the ATW display).
 *
 * Same fpgabios GEMDOS opcodes and start-up handshake as the Dropbear client (staging/DROPBEAR/atwx25519.c).
 * Usage: atwxserv [SERVER.btl]   (default /etc/dropbear/xserv.btl)
 * Build: m68k-atari-mint-gcc -m68020-60 -O2 -s -o atwxserv.ttp src/x25519bench/atwxserv.c
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

static long link_write(const unsigned char *buf, long len)
{
    long done = 0;
    clock_t end = clock() + 5 * CLOCKS_PER_SEC;
    while (done < len) {
        long n = len - done > 0x7000 ? 0x7000 : len - done;
        long r = trap_1_wll(OP_WRITE, (long)(buf + done), n);
        if (r < 0)
            return r;
        if (r > 0) {
            done += r;
            end = clock() + 5 * CLOCKS_PER_SEC;
        } else if (clock() > end)
            break;
    }
    return done;
}

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
    const char *image = argc > 1 ? argv[1] : "/etc/dropbear/xserv.btl";
    unsigned char *img, pkt[1024], rep[16];
    long imglen, len, rl;
    FILE *f;

    if ((short)trap_1_ww(OP_CHECK, 0x17) != 0x17) {
        fprintf(stderr, "atwxserv: fpgabios.tos not resident\n");
        return 1;
    }
    if (!(f = fopen(image, "rb"))) {
        perror(image);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    imglen = ftell(f);
    rewind(f);
    if (!(img = malloc(imglen)) || fread(img, 1, imglen, f) != (size_t)imglen) {
        fprintf(stderr, "atwxserv: cannot read %s\n", image);
        return 1;
    }
    fclose(f);
    trap_1_w(OP_RESET);
    len = link_write(img, imglen);
    free(img);
    if (len != imglen) {
        fprintf(stderr, "atwxserv: boot image write failed (%ld of %ld)\n", len, imglen);
        return 1;
    }
    for (;;) {                                 /* INMOS runtime start-up: GETENV, SP_ID, SP_COMMAND x2 */
        if (link_read(pkt, 2, 5000) != 2) {
            fprintf(stderr, "atwxserv: no start-up request from the T425\n");
            return 1;
        }
        if (pkt[0] == 'R' && pkt[1] == 'D') {  /* "RDY0": main() is running */
            link_read(pkt + 2, 2, 2000);
            printf("atwxserv: X25519 server running on the ATW800/2 T425\n");
            return 0;
        }
        len = pkt[0] | (pkt[1] << 8);
        if (len < 1 || len > (long)sizeof pkt || link_read(pkt, len, 2000) != len) {
            fprintf(stderr, "atwxserv: bad start-up packet\n");
            return 1;
        }
        rl = 1;
        rep[2] = 1;                            /* SP_UNIMPLEMENTED unless handled below */
        if (pkt[0] == 32 && len >= 13 && pkt[1] == 10 && !memcmp(pkt + 3, "IBOARDSIZE", 10)) {
            static const char board[] = "#200000";
            rep[2] = 0;
            rep[3] = sizeof board - 1;
            rep[4] = 0;
            memcpy(rep + 5, board, sizeof board - 1);
            rl = 3 + sizeof board - 1;
        } else if (pkt[0] == 32) {
            rep[2] = 129;                      /* SP_ERROR */
        } else if (pkt[0] == 40) {             /* SP_COMMAND */
            rep[2] = rep[3] = rep[4] = 0;
            rl = 3;
        } else if (pkt[0] == 42) {             /* SP_ID */
            rep[2] = rep[3] = rep[4] = rep[5] = rep[6] = 0;
            rl = 5;
        }
        rep[0] = (unsigned char)rl;
        rep[1] = (unsigned char)(rl >> 8);
        if (link_write(rep, rl + 2) != rl + 2) {
            fprintf(stderr, "atwxserv: start-up reply write failed\n");
            return 1;
        }
    }
}
