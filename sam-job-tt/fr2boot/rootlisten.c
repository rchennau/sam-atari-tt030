/* FR-2: after booting the INMOS echo worker onto TRAM 1 (link 1), listen 2 s for anything the TRAM sends first and
 * print it — the "INMOS runtime talks on its boot link" test. Sends nothing after the boot. */
#include <channel.h>
#include <process.h>
#include <stdio.h>
#define LOUT(n) ((Channel *)(0x80000000u + 4u * (unsigned)(n)))
#define LIN(n)  ((Channel *)(0x80000010u + 4u * (unsigned)(n)))
int main(int argc, char **argv)
{
    static unsigned char img[65536], got[256];
    FILE *f = fopen(argc > 1 ? argv[1] : "echoa.btl", "rb");
    long sz;
    int n = 0;
    if (!f) { printf("FAIL open\n"); return 1; }
    sz = (long)fread(img, 1, sizeof img, f);
    fclose(f);
    ChanOut(LOUT(1), img, (int)sz);
    printf("2 boot sent (%ld bytes)\n", sz); fflush(stdout);
    while (n < (int)sizeof got && ProcTimerAlt(ProcTimePlus(ProcTime(), 31250), LIN(1), NULL) == 0)
        got[n++] = (unsigned char)ChanInChar(LIN(1));     /* 31250 low-priority ticks of 64 us = 2 s */
    printf("3 TRAM sent %d bytes first:", n);
    for (int i = 0; i < n && i < 32; i++) printf(" %02x", got[i]);
    printf("\n"); fflush(stdout);
    return 0;
}
