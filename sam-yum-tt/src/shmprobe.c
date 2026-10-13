/* shmprobe (68030): (1) save the ATW800/2 SharedMem (0xFEDF4000, 39 KB), time 68030 copies into and out of it,
 * write a 16-byte marker at its start; (2) boot shmscan on the T425 and scan its address space 1 MB at a time,
 * READ-ONLY, for the marker; (3) restore SharedMem. Usage: shmprobe SHMSCAN.BTL [MB]  (default 16 MB from
 * 0x80000000). A 1 MB step that never answers means the T425 hung reading there: the scan stops and says so. */
#include "atwboot.h"
#include <mint/osbind.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define SHM ((volatile unsigned char *)0xFEDF4000L)
#define SHMLEN 39936L
#define TESTLEN 32768L
static unsigned char save[SHMLEN], ram[TESTLEN], back[TESTLEN];
static const unsigned char mark[16] = { 'S','A','M','-','S','H','M','?', 0x5a,0xa5,0x3c,0xc3,0x12,0x34,0x56,0x78 };
static int iters, ok;
static long do_save(void) { memcpy(save, (void *)SHM, SHMLEN); return 0; }
static long do_restore(void) { memcpy((void *)SHM, save, SHMLEN); return 0; }
static long do_write(void) { int i; for (i = 0; i < iters; i++) memcpy((void *)SHM, ram, TESTLEN); return 0; }
static long do_read(void) { int i; for (i = 0; i < iters; i++) memcpy(back, (void *)SHM, TESTLEN); return 0; }
static long do_mark(void) { memcpy((void *)SHM, mark, 16); ok = !memcmp((void *)SHM, mark, 16); return 0; }
static void put4(unsigned char *b, unsigned long v) { b[0] = v; b[1] = v >> 8; b[2] = v >> 16; b[3] = v >> 24; }
static unsigned long get4(const unsigned char *b) { return b[0] | ((unsigned long)b[1] << 8) | ((unsigned long)b[2] << 16) | ((unsigned long)b[3] << 24); }
int main(int argc, char **argv)
{
    long i, mb = argc > 2 ? atol(argv[2]) : 16, ms;
    clock_t t0;
    unsigned char req[24], r[4];
    if (argc < 2) { printf("usage: shmprobe SHMSCAN.BTL [MB]\n"); return 2; }
    for (i = 0; i < TESTLEN; i++) ram[i] = (unsigned char)(i * 13 + 5);
    Supexec(do_save);
    iters = 50;
    t0 = clock(); Supexec(do_write); ms = ms_since(t0);
    printf("68030 -> SharedMem: %ld KB in %ld ms = %ld KB/s\n", TESTLEN * iters / 1024, ms, ms ? TESTLEN * iters / ms : 0);
    t0 = clock(); Supexec(do_read); ms = ms_since(t0);
    printf("SharedMem -> 68030: %ld KB in %ld ms = %ld KB/s, data %s\n", TESTLEN * iters / 1024, ms,
           ms ? TESTLEN * iters / ms : 0, memcmp(back, ram, TESTLEN) ? "DIFFERS" : "intact");
    Supexec(do_mark);
    printf("marker written at 0xFEDF4000, read back %s\n", ok ? "ok" : "WRONG");
    atw_verbose = 0;
    if (atw_boot(argv[1], "#200000")) { Supexec(do_restore); return 1; }
    for (i = 0; i < 16; i++) req[i] = (unsigned char)~mark[i];
    for (i = 0; i < mb; i++) {
        unsigned long start = 0x80000000UL + (unsigned long)i * 0x100000UL, n, k;
        put4(req + 16, start); put4(req + 20, 0x100000UL);
        if (link_write(req, 24) != 24 || link_read(r, 4, 20000) != 4) {
            printf("T425 did not answer for %08lx..+1MB: it hung reading there; stopping\n", start);
            break;
        }
        n = get4(r);
        for (k = 0; k < n; k++) {
            if (link_read(r, 4, 2000) != 4) break;
            printf("HIT at T425 %08lx (%s)\n", get4(r) & ~1UL, (get4(r) & 1) ? "16-bit pairs swapped" : "bytes as written");
        }
    }
    printf("scanned to %08lx\n", 0x80000000UL + (unsigned long)i * 0x100000UL);
    Supexec(do_restore);
    printf("SharedMem restored\n");
    return 0;
}
