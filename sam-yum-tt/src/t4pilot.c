/* t4pilot — time one t4serv request on the ATW800/2 T425 with the boot and the call measured apart
 * (PMS tt030-llvm-t800-adoption FR-2/FR-5: the gate times the server's work after boot, so a .btl size
 * change cannot decide a code-generation comparison).
 *   t4pilot BTL OP LEVEL INFILE [OUTFILE]
 * Prints "boot_ms call_ms outlen adler32" on stdout; with T4PILOT_V set, atw_boot logs every start-up
 * packet (a -mhost=none server must show none before its RDY0 marker). Takes the T425 lock like t4call. */
#include "atwboot.h"
#include "t4adler.h"
#include "t4lock.h"
#include <stdio.h>
#include <stdlib.h>

static unsigned long get4(const unsigned char *b)
{
    return b[0] | ((unsigned long)b[1] << 8) | ((unsigned long)b[2] << 16) | ((unsigned long)b[3] << 24);
}

int main(int argc, char **argv)
{
    static unsigned char in[1L << 20], out[1L << 20];
    unsigned char h[4], a[4];
    const char *why;
    long n, len, boot_ms, call_ms;
    clock_t t0;
    FILE *f;

    if (argc < 5) {
        fprintf(stderr, "usage: t4pilot BTL OP LEVEL INFILE [OUTFILE]\n");
        return 2;
    }
    if (!(f = fopen(argv[4], "rb")) || (n = (long)fread(in, 1, sizeof in, f)) < 0) {
        perror(argv[4]);
        return 2;
    }
    fclose(f);
    if (t4lock_take(&why) <= 0) {
        fprintf(stderr, "t4pilot: lock: %s\n", why);
        return 1;
    }
    atw_verbose = getenv("T4PILOT_V") != NULL;
    t0 = clock();
    if (atw_boot(argv[1], "#500000")) {
        t4lock_release();
        fprintf(stderr, "t4pilot: did not boot\n");
        return 1;
    }
    boot_ms = ms_since(t0);
    t0 = clock();
    if (send_op(argv[2][0], atoi(argv[3]), n) || link_write(in, n) != n || link_read(h, 4, 6L * 3600 * 1000) != 4
        || (len = (long)get4(h)) < 0 || len > (long)sizeof out
        || link_read(a, 4, 5000) != 4 || link_read(out, len, 30000) != len) {
        t4lock_release();
        fprintf(stderr, "t4pilot: call failed\n");
        return 1;
    }
    call_ms = ms_since(t0);
    t4lock_release();
    printf("%ld %ld %ld %08lx%s\n", boot_ms, call_ms, len, t4_adler32(out, len),
           t4_adler32(out, len) == get4(a) ? "" : " DAMAGED");
    if (argc > 5 && (f = fopen(argv[5], "wb"))) {
        fwrite(out, 1, (size_t)len, f);
        fclose(f);
    }
    return 0;
}
