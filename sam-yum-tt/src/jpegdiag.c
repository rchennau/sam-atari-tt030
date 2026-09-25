/* jpegdiag — why did jpegserv fail on the real T425? Boots SERVER, sends FILE as op 'D' (or 'C' for a
 * .raw request made by cjpeg's front end), then op 'Q': libjpeg's msg_code and the failure stage.
 *   jpegdiag SERVER.btl FILE D|C [quality]          (tt030-t425-kernel-ports W2 debugging) */
#include "atwboot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long g4(const unsigned char *b)
{
    return b[0] | ((unsigned long)b[1] << 8) | ((unsigned long)b[2] << 16) | ((unsigned long)b[3] << 24);
}

static long ask(int op, int level, const unsigned char *in, long n, unsigned char *out, long cap)
{
    unsigned char h[4], a[4];
    long len;
    if (send_op((char)op, level, n) || (n && link_write(in, n) != n) || link_read(h, 4, 120000) != 4)
        return -2;
    len = (long)g4(h);
    if (len < 0 || len > cap) {
        unsigned char more[16];
        long m = link_read(more, 16, 3000), i;
        printf("raw reply %02x %02x %02x %02x, then %ld more bytes:", h[0], h[1], h[2], h[3], m);
        for (i = 0; i < m; i++)
            printf(" %02x", more[i]);
        printf("\n");
        return -1;
    }
    if (link_read(a, 4, 5000) != 4 || link_read(out, len, 60000) != len)
        return -3;
    return len;
}

int main(int argc, char **argv)
{
    FILE *f;
    long n, r;
    unsigned char *in, *out;
    if (argc < 4 || !(f = fopen(argv[2], "rb")))
        return 2;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    rewind(f);
    in = malloc(n);
    out = malloc(3L * 1024 * 1024);
    if (!in || !out || (long)fread(in, 1, n, f) != n)
        return 1;
    atw_verbose = 0;
    if (atw_boot(argv[1], getenv("T4BOARD") ? getenv("T4BOARD") : "#400000"))
        return printf("boot failed\n"), 1;
    r = ask('Q', 0, NULL, 0, out, 64);
    printf("Q before -> %ld%s\n", r, r == 8 ? " (server answers)" : "");
    r = ask(argv[3][0], argc > 4 ? atoi(argv[4]) : 0, in, n, out, 3L * 1024 * 1024);
    printf("op %c on %ld bytes -> %ld\n", argv[3][0], n, r);
    if (r == 8 && argv[3][0] == 'M')
        printf("memory: allocated %ld bytes, verified %ld bytes\n", (long)g4(out), (long)g4(out + 4));
    if (r == 12 && !memcmp(out, "ERR", 3) && out[3] != '!')
        printf("t4serv says: %s NULL, cap %ld, len %ld\n", out[3] == 'I' ? "in" : out[3] == 'O' ? "out" : "?",
               (long)g4(out + 4), (long)g4(out + 8));
    if (r == 12 && !memcmp(out, "ERR!", 4))
        printf("server says: libjpeg msg_code %ld, stage %ld\n", (long)g4(out + 4), (long)g4(out + 8));
    r = ask('Q', 0, NULL, 0, out, 64);
    if (r == 8)
        printf("libjpeg msg_code %ld, stage %ld\n", (long)g4(out), (long)g4(out + 4));
    else
        printf("Q -> %ld\n", r);
    return 0;
}
