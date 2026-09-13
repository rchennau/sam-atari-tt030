/* ramtest — FR-1: pattern-test free TT-RAM and ST-RAM from userland under FreeMiNT.
 * Allocates with Mxalloc (mode 1 = TT-RAM only, 0 = ST-RAM only) in 1 MiB blocks, leaving a
 * reserve for the running system, then runs address-in-address and 0x55/0xAA passes on each block.
 * Usage: ramtest [reserve-KiB]     (default 2048)
 * Build: m68k-atari-mint-gcc -m68020-60 -O2 -s -o ramtest src/ramtest.c
 */
#include <mint/osbind.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BLK (1024L * 1024L)
#define MAXB 128

static long test_block(volatile unsigned long *p, long n)
{
    long i, err = 0;
    for (i = 0; i < n; i++) p[i] = (unsigned long)&p[i];
    for (i = 0; i < n; i++) if (p[i] != (unsigned long)&p[i]) err++;
    for (i = 0; i < n; i++) p[i] = 0x55555555UL;
    for (i = 0; i < n; i++) if (p[i] != 0x55555555UL) err++;
    for (i = 0; i < n; i++) p[i] = 0xAAAAAAAAUL;
    for (i = 0; i < n; i++) if (p[i] != 0xAAAAAAAAUL) err++;
    return err;
}

static void run(const char *name, int mode, long reserve)
{
    void *blk[MAXB];
    long nb = 0, err = 0, i, freeb = (long)Mxalloc(-1, mode);
    time_t t0 = time(NULL);

    while (nb < MAXB && freeb - (nb + 1) * BLK >= reserve && (blk[nb] = (void *)Mxalloc(BLK, mode)))
        nb++;
    for (i = 0; i < nb; i++) {
        long e = test_block(blk[i], BLK / 4);
        if (e) printf("%s block %ld at %p: %ld errors\n", name, i, blk[i], e);
        err += e;
    }
    printf("%s: largest free %ld KiB, tested %ld MiB, errors %ld, %ld s\n",
           name, freeb / 1024, nb, err, (long)(time(NULL) - t0));
    fflush(stdout);
    for (i = 0; i < nb; i++) Mfree(blk[i]);
}

int main(int argc, char **argv)
{
    long reserve = (argc > 1 ? atol(argv[1]) : 2048) * 1024L;
    run("TT-RAM", 1, reserve);
    run("ST-RAM", 0, reserve);
    return 0;
}
