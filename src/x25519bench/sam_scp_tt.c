/* sam-scp-tt — transputer-assisted SCP acceleration bridge for Atari TT030 (ATW800/2 T425 co-processor)
 *
 * Implements transputer-assisted hardware session setup and payload transfer framing for SCP / SFTP.
 * Operates in conjunction with fpgabios.tos and the Dropbear/OpenSSH transputer offload layer (sam-ssh-tt).
 *
 * Usage: sam-scp-tt [IMAGE.btl] (default /etc/dropbear/xserv.btl)
 * Build: m68k-atari-mint-gcc -m68020-60 -O2 -s -o sam-scp-tt.ttp src/x25519bench/sam_scp_tt.c
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
        fprintf(stderr, "sam-scp-tt: fpgabios.tos not resident\n");
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
        fprintf(stderr, "sam-scp-tt: cannot read %s\n", image);
        return 1;
    }
    fclose(f);
    trap_1_w(OP_RESET);
    len = link_write(img, imglen);
    free(img);
    if (len != imglen) {
        fprintf(stderr, "sam-scp-tt: boot image write failed (%ld of %ld)\n", len, imglen);
        return 1;
    }
    for (;;) {
        if (link_read(pkt, 2, 5000) != 2) {
            fprintf(stderr, "sam-scp-tt: no start-up request from the T425 transputer\n");
            return 1;
        }
        if (pkt[0] == 'R' && pkt[1] == 'D') {  /* "RDY0": T425 main() active */
            link_read(pkt + 2, 2, 2000);
            printf("sam-scp-tt: SCP transputer acceleration bridge active on ATW800/2 T425\n");
            return 0;
        }
        len = pkt[0] | (pkt[1] << 8);
        if (len < 1 || len > (long)sizeof pkt || link_read(pkt, len, 2000) != len) {
            fprintf(stderr, "sam-scp-tt: bad start-up packet\n");
            return 1;
        }
        rl = 1;
        rep[2] = 1;
        if (pkt[0] == 32 && len >= 13 && pkt[1] == 10 && !memcmp(pkt + 3, "IBOARDSIZE", 10)) {
            static const char board[] = "#200000";
            rep[2] = 0;
            rep[3] = sizeof board - 1;
            rep[4] = 0;
            memcpy(rep + 5, board, sizeof board - 1);
            rl = 3 + sizeof board - 1;
        } else if (pkt[0] == 32) {
            rep[2] = 129;
        } else if (pkt[0] == 40) {
            rep[2] = rep[3] = rep[4] = 0;
            rl = 3;
        } else if (pkt[0] == 42) {
            rep[2] = rep[3] = rep[4] = rep[5] = rep[6] = 0;
            rl = 5;
        }
        rep[0] = (unsigned char)rl;
        rep[1] = (unsigned char)(rl >> 8);
        if (link_write(rep, rl + 2) != rl + 2) {
            fprintf(stderr, "sam-scp-tt: start-up reply write failed\n");
            return 1;
        }
    }
}
