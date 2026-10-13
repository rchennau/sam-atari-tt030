/* shmrt (68030): SharedMem data path vs the link (atari-tt030-enhancement, 2026-10-12). Boots shmserv, then
 *   1. T425-side SharedMem bandwidth (32 KB x 50 each way, timed by the T425)
 *   2. round-trips: 68030 copies n bytes into SharedMem (Supexec), 4-byte doorbell on the link, T425 complements
 *      them in place and acks 1 byte, 68030 copies them back and checks — same sizes as linkrt.
 * SharedMem is saved first and restored at the end. Usage: shmrt SHMSERV.BTL */
#include "atwboot.h"
#include <mint/osbind.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#define SHM ((volatile unsigned char *)0xFEDF4000L)
#define SHMLEN 39936L
static unsigned char save[SHMLEN], ram[16384], back[16384];
static long n;
static long do_save(void) { memcpy(save, (void *)SHM, SHMLEN); return 0; }
static long do_restore(void) { memcpy((void *)SHM, save, SHMLEN); return 0; }
/* Measured 2026-10-12: the T425 saw the 68030's bytes, wrote and re-read its own, yet the 68030 later read its
 * ORIGINAL bytes back — consistent with the card holding the 68030's last write and committing it late, over the
 * T425's. Test: a write to a distant SharedMem word after the copy, to push the held write out. */
static long do_in(void)
{
    memcpy((void *)SHM, ram, n);
    *(volatile unsigned long *)(SHM + 0x9800) = 0;
    return 0;
}
/* The 68030 data cache keeps SharedMem lines it has read, so a block the T425 changed reads back stale:
 * clear the data cache (CACR bit 11, CD) before copying out. Measured as "damaged" without it, 2026-10-12. */
static long spins, maxspins;
static volatile unsigned long sink;
static long do_out(void)
{
    __asm__ volatile ("movec %%cacr,%%d0\n\tori.w #0x0800,%%d0\n\tmovec %%d0,%%cacr" ::: "d0");
    /* hypothesis 2026-10-12: the card buffers the 68030's last-accessed SharedMem line, so the T425's writes to
     * the same line read back stale. Touch distant addresses first, then poll (diagnostic). */
    sink = *(volatile unsigned long *)(SHM + 0x8000) + *(volatile unsigned long *)0xFEC00000L;
    for (spins = 0; spins < 2000 && SHM[n - 1] == ram[n - 1]; spins++)
        sink = *(volatile unsigned long *)(SHM + 0x8000 + (spins & 0xff) * 16);
    if (spins > maxspins) maxspins = spins;
    memcpy(back, (void *)SHM, n);
    return 0;
}
int main(int argc, char **argv)
{
    static const long sizes[6] = { 4, 64, 256, 1024, 4096, 16384 };
    unsigned char b[8], ba[8];
    long s, i, k, ms, iters, tr, tw;
    clock_t t0;
    if (argc < 2) { printf("usage: shmrt SHMSERV.BTL\n"); return 2; }
    for (i = 0; i < 16384; i++) ram[i] = (unsigned char)(i * 13 + 5);
    Supexec(do_save);
    atw_verbose = 0;
    if (atw_boot(argv[1], "#200000")) { Supexec(do_restore); return 1; }
    b[0] = b[1] = b[2] = b[3] = 0xff;
    if (link_write(b, 4) != 4 || link_read(b, 8, 30000) != 8) { printf("bandwidth test: no reply\n"); Supexec(do_restore); return 1; }
    tr = b[0] | (b[1] << 8) | ((long)b[2] << 16) | ((long)b[3] << 24);
    tw = b[4] | (b[5] << 8) | ((long)b[6] << 16) | ((long)b[7] << 24);
    printf("T425 reads SharedMem:  %ld KB/s\nT425 writes SharedMem: %ld KB/s\n",
           tr ? 1600L * 1000000L / (tr * 64L) : 0, tw ? 1600L * 1000000L / (tw * 64L) : 0);   /* 1600 KB moved, ticks of 64 us */
    printf("SharedMem round-trip (copy in, doorbell, T425 complements in place, ack, copy out)\n");
    printf("size   iters  us/round-trip  KB/s per direction\n");
    for (s = 0; s < 6; s++) {
        n = sizes[s];
        iters = n <= 256 ? 400 : n <= 4096 ? 100 : 30;
        t0 = clock();
        for (i = 0; i < iters; i++) {
            Supexec(do_in);
            b[0] = n; b[1] = n >> 8; b[2] = n >> 16; b[3] = n >> 24;
            if (link_write(b, 4) != 4 || link_read(ba, 8, 5000) != 8) { printf("no ack: size %ld\n", n); Supexec(do_restore); return 1; }
            if (i == 0 && s == 0) printf("T425 saw %02x%02x%02x%02x before, %02x%02x%02x%02x after\n", ba[0], ba[1], ba[2], ba[3], ba[4], ba[5], ba[6], ba[7]);
            Supexec(do_out);
            for (k = 0; k < n; k++)
                if (back[k] != (unsigned char)~ram[k]) { printf("damaged: size %ld byte %ld: sent %02x %02x %02x %02x, got %02x %02x %02x %02x\n", n, k, ram[0], ram[1], ram[2], ram[3], back[0], back[1], back[2], back[3]); Supexec(do_restore); return 1; }
        }
        ms = ms_since(t0);
        printf("%5ld  %5ld  %10ld     %ld   (max polls before visible: %ld)\n", n, iters, ms * 1000 / iters, ms ? 2 * n * iters / ms : 0, maxspins);
        maxspins = 0;
    }
    Supexec(do_restore);
    printf("SharedMem restored\n");
    return 0;
}
