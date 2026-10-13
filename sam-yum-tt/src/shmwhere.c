/* shmwhere (68030): where do T425 writes to SharedMem land? Saves SharedMem, boots shmserv, asks it to write a
 * 16-byte marker at T425 0x807F4000, then scans the 68030's whole ATW800/2 window 0xFEC00000-0xFEDFFFFF
 * READ-ONLY for it — as written, with 16-bit pairs swapped, and with 32-bit words swapped — and prints the
 * A20GATE register (VTG 0xFEDFF81E, read only). Restores SharedMem. Usage: shmwhere SHMSERV.BTL */
#include "atwboot.h"
#include <mint/osbind.h>
#include <stdio.h>
#include <string.h>
#define WIN ((volatile unsigned char *)0xFEC00000L)
#define WINLEN 0x200000L
#define SHM ((volatile unsigned char *)0xFEDF4000L)
#define SHMLEN 39936L
static unsigned char save[SHMLEN], pat[3][16];
static long hits[3][8], nh[3];
static unsigned short a20;
static const unsigned char m[16] = { 'T','4','2','5','W','R','I','T', 0xde,0xad,0xbe,0xef,0x01,0x23,0x45,0x67 };
static long do_save(void) { memcpy(save, (void *)SHM, SHMLEN); return 0; }
static long do_restore(void) { memcpy((void *)SHM, save, SHMLEN); return 0; }
static long do_a20(void) { a20 = *(volatile unsigned short *)0xFEDFF81EL; return 0; }
static long do_scan(void)
{
    long i; int v;
    __asm__ volatile ("movec %%cacr,%%d0\n\tori.w #0x0800,%%d0\n\tmovec %%d0,%%cacr" ::: "d0");
    for (i = 0; i <= WINLEN - 16; i++)
        for (v = 0; v < 3; v++)
            if (WIN[i] == pat[v][0] && !memcmp((void *)(WIN + i), pat[v], 16) && nh[v] < 8)
                hits[v][nh[v]++] = i;
    return 0;
}
int main(int argc, char **argv)
{
    static const char *name[3] = { "as written", "16-bit pairs swapped", "32-bit words swapped" };
    unsigned char b[4] = { 0xfe, 0xff, 0xff, 0xff }, rb[16];
    int i, v;
    if (argc < 2) { printf("usage: shmwhere SHMSERV.BTL\n"); return 2; }
    for (i = 0; i < 16; i++) { pat[0][i] = m[i]; pat[1][i] = m[i ^ 1]; pat[2][i] = m[i ^ 3]; }
    Supexec(do_a20);
    printf("A20GATE (0xFEDFF81E) = %04x\n", a20);
    Supexec(do_save);
    atw_verbose = 0;
    if (atw_boot(argv[1], "#200000")) { Supexec(do_restore); return 1; }
    if (link_write(b, 4) != 4 || link_read(rb, 16, 5000) != 16) { printf("no reply\n"); Supexec(do_restore); return 1; }
    printf("T425 reads back at 0x807F4000: ");
    for (i = 0; i < 16; i++) printf("%02x", rb[i]);
    printf(" (%s)\n", memcmp(rb, m, 16) ? "NOT its write" : "its write");
    Supexec(do_scan);
    for (v = 0; v < 3; v++)
        for (i = 0; i < nh[v]; i++)
            printf("68030 sees it at 0x%08lx (window +0x%06lx), %s\n", 0xFEC00000L + hits[v][i], hits[v][i], name[v]);
    if (!nh[0] && !nh[1] && !nh[2]) printf("68030 does not see the marker anywhere in 0xFEC00000-0xFEDFFFFF\n");
    Supexec(do_restore);
    printf("SharedMem restored\n");
    return 0;
}
