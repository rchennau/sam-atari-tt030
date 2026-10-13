/* shmab (68030): which difference breaks the SharedMem round-trip? (2026-10-12) Five steps, 64 bytes each:
 *   1 no-write : 68030 reads SharedMem, doorbell (T425 complements in place), 68030 reads — expect complement
 *   2 write    : 68030 writes a pattern first, then as step 1
 *   3 bandwidth: T425 50 x 32 KB read + write loop (shmserv op 0xFFFFFFFF)
 *   4 no-write : as 1, after the bandwidth loop
 *   5 write    : as 2, after the bandwidth loop
 * SharedMem saved and restored. Usage: shmab SHMSERV.BTL */
#include "atwboot.h"
#include <mint/osbind.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#define SHM ((volatile unsigned char *)0xFEDF4000L)
#define SHMLEN 39936L
#define N 64
static unsigned char save[SHMLEN], pat[N], before[N], after[N];
static long do_save(void) { memcpy(save, (void *)SHM, SHMLEN); return 0; }
static long do_restore(void) { memcpy((void *)SHM, save, SHMLEN); return 0; }
static void clr(void) { __asm__ volatile ("movec %%cacr,%%d0\n\tori.w #0x0800,%%d0\n\tmovec %%d0,%%cacr" ::: "d0"); }
static long do_write(void) { memcpy((void *)SHM, pat, N); return 0; }
static long do_before(void) { clr(); memcpy(before, (void *)SHM, N); return 0; }
static long do_after(void) { clr(); memcpy(after, (void *)SHM, N); return 0; }
static int step(const char *name, int write)
{
    unsigned char b[4] = { N, 0, 0, 0 }, ba[8];
    int k, bad = 0;
    if (write) Supexec(do_write);
    Supexec(do_before);
    if (link_write(b, 4) != 4 || link_read(ba, 8, 5000) != 8) { printf("%-24s no reply\n", name); return 1; }
    Supexec(do_after);
    for (k = 0; k < N; k++) if (after[k] != (unsigned char)~before[k]) bad++;
    printf("%-24s %s (%d/%d bytes wrong; 68030 before %02x%02x, T425 before %02x%02x after %02x%02x, 68030 after %02x%02x)\n",
           name, bad ? "FAIL" : "ok", bad, N, before[0], before[1], ba[0], ba[1], ba[4], ba[5], after[0], after[1]);
    return 0;
}
static long ms_n;
static unsigned char mout[16384], min_[16384];
static long do_min(void) { memcpy((void *)SHM, min_, ms_n); return 0; }
static long do_mout(void) { clr(); memcpy(mout, (void *)(SHM + SHMLEN / 2), ms_n); return 0; }
/* mailbox round-trip: 68030 writes only the input half, T425 writes only the output half */
static void mailbox(int evict)
{
    static const long sizes[6] = { 4, 64, 256, 1024, 4096, 16384 };
    long s, i, k, iters, ms, bad;
    unsigned char b[4], r8[8];
    clock_t t0;
    for (k = 0; k < 16384; k++) min_[k] = (unsigned char)(k * 7 + 3);
    printf("mailbox round-trip%s (68030 writes input half, doorbell, T425 writes complement to output half, 68030 reads)\n",
           evict ? " WITH 32 KB eviction read" : "");
    printf("size   iters  us/round-trip  KB/s per direction\n");
    for (s = 0; s < 6; s++) {
        ms_n = sizes[s];
        iters = ms_n <= 256 ? 400 : ms_n <= 4096 ? 100 : 30;
        bad = 0;
        t0 = clock();
        for (i = 0; i < iters; i++) {
            min_[0] = (unsigned char)i;              /* vary the data every round */
            Supexec(do_min);
            b[0] = ms_n; b[1] = ms_n >> 8; b[2] = evict ? 9 : 0; b[3] = evict ? 0x50 : 0x40;   /* 9: 64 << 9 = 32 KB */
            if (link_write(b, 4) != 4 || link_read(r8, 8, 5000) != 8) { printf("no reply\n"); return; }
            Supexec(do_mout);
            for (k = 0; k < ms_n; k++) if (mout[k] != (unsigned char)~min_[k]) { bad++; break; }   /* rounds, not bytes */
        }
        ms = ms_since(t0);
        printf("%5ld  %5ld  %10ld     %ld   %s\n", ms_n, iters, ms * 1000 / iters, ms ? 2 * ms_n * iters / ms : 0,
               bad ? "rounds wrong:" : "all correct");
        if (bad) printf("        %ld of %ld\n", bad, iters);
    }
}
int main(int argc, char **argv)
{
    unsigned char b[4] = { 0xff, 0xff, 0xff, 0xff }, t[8];
    int k;
    if (argc < 2) { printf("usage: shmab SHMSERV.BTL\n"); return 2; }
    for (k = 0; k < N; k++) pat[k] = (unsigned char)(k * 13 + 5);
    Supexec(do_save);
    atw_verbose = 0;
    if (atw_boot(argv[1], "#200000")) { Supexec(do_restore); return 1; }
    step("1 no-write", 0);
    step("2 write", 1);
    if (link_write(b, 4) != 4 || link_read(t, 8, 30000) != 8) printf("3 bandwidth: no reply\n"); else printf("3 bandwidth loop done\n");
    step("4 no-write after bw", 0);
    step("5 write after bw", 1);
    mailbox(0);
    mailbox(1);
    {   /* commit latency: delay between the 68030's write and the doorbell, 64-byte mailbox, 20 rounds each */
        static const long dl[7] = { 0, 1, 5, 20, 50, 100, 200 };
        long d, i, k, bad; unsigned char b[4], r8[8]; clock_t t;
        ms_n = 64;
        {   /* T425-side variants, no delay: 0 plain, 1 alias read, 2 evict first, 3 both */
            static const char *vn[4] = { "plain", "alias 0x809F4000", "evict 64 KB first", "alias + evict" };
            int v;
            printf("T425 read variant -> wrong rounds of 20 (64-byte mailbox)\n");
            for (v = 0; v < 4; v++) {
                bad = 0;
                for (i = 0; i < 20; i++) {
                    min_[0] = (unsigned char)(i + 31 * v + 99);
                    Supexec(do_min);
                    b[0] = 64; b[1] = 0; b[2] = 0; b[3] = 0x40 | (v & 1 ? 0x20 : 0) | (v & 2 ? 0x10 : 0);
                    if (link_write(b, 4) != 4 || link_read(r8, 8, 5000) != 8) { printf("no reply\n"); break; }
                    Supexec(do_mout);
                    for (k = 0; k < 64; k++) if (mout[k] != (unsigned char)~min_[k]) { bad++; break; }
                }
                printf("  %-20s %ld\n", vn[v], bad);
            }
        }
        {   /* smallest eviction read that works: 64 B .. 64 KB */
            int e;
            printf("evict read size -> wrong rounds of 20, round-trip us (64-byte mailbox)\n");
            for (e = 0; e <= 10; e += 1) {
                clock_t t1 = clock();
                bad = 0;
                for (i = 0; i < 20; i++) {
                    min_[0] = (unsigned char)(i + 13 * e + 7);
                    Supexec(do_min);
                    b[0] = 64; b[1] = 0; b[2] = (unsigned char)e; b[3] = 0x40 | 0x10;
                    if (link_write(b, 4) != 4 || link_read(r8, 8, 5000) != 8) { printf("no reply\n"); break; }
                    Supexec(do_mout);
                    for (k = 0; k < 64; k++) if (mout[k] != (unsigned char)~min_[k]) { bad++; break; }
                }
                printf("  %6ld B: %ld wrong, %ld us\n", 64L << e, bad, ms_since(t1) * 1000 / 20);
            }
        }
        printf("delay before doorbell -> wrong rounds of 20 (64-byte mailbox)\n");
        for (d = 0; d < 7; d++) {
            bad = 0;
            for (i = 0; i < 20; i++) {
                min_[0] = (unsigned char)(i + 17 * d);
                Supexec(do_min);
                t = clock() + dl[d] * CLOCKS_PER_SEC / 1000;
                while (dl[d] && clock() < t) ;
                b[0] = 64; b[1] = 0; b[2] = 0; b[3] = 0x40;
                if (link_write(b, 4) != 4 || link_read(r8, 8, 5000) != 8) { printf("no reply\n"); break; }
                Supexec(do_mout);
                for (k = 0; k < 64; k++) if (mout[k] != (unsigned char)~min_[k]) { bad++; break; }
            }
            printf("  %4ld ms: %ld\n", dl[d], bad);
        }
    }
    Supexec(do_restore);
    printf("SharedMem restored\n");
    return 0;
}
