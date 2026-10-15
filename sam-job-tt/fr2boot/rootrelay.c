/* FR-2 relay test (from the late-read root): T425 -> TRAM 1 (relay) -> TRAM 2 (echo), both llvm.
 * Original header: boot the INMOS echo worker onto TRAM 1 (link 1), answer its start-up iserver requests as
 * atw_boot() does (IBOARDSIZE -> "#400000"), then send the message from a separate process while main prints a 1 s
 * heartbeat for up to 20 s — the host link never stalls, and the stage shows how far the worker got. */
#include <channel.h>
#include <process.h>
#include <stdio.h>
#include <string.h>
#define LOUT(n) ((Channel *)(0x80000000u + 4u * (unsigned)(n)))
#define LIN(n)  ((Channel *)(0x80000010u + 4u * (unsigned)(n)))
static volatile int stage;
static unsigned char msg[16], back[16];
static int rn;
static void sender(void)
{
    int n = 16;
    ChanOut(LOUT(1), &n, 4); stage = 1;
    ChanOut(LOUT(1), msg, 16); stage = 2;
    ChanIn(LIN(1), &rn, 4); stage = 3;
    ChanIn(LIN(1), back, 16); stage = 4;
    ProcStop();
}
static int quiet_for(int ticks) { return ProcTimerAlt(ProcTimePlus(ProcTime(), ticks), LIN(1), NULL) != 0; }
int main(int argc, char **argv)
{
    static unsigned char img[65536], pkt[1024], rep[64];
    const char *board = "#400000";
    FILE *f = fopen(argc > 1 ? argv[1] : "relay1.btl", "rb");
    static unsigned char img2[65536];
    FILE *f2 = fopen(argc > 2 ? argv[2] : "echo2.btl", "rb");
    int sz2;
    long sz;
    int served = 0, ok = 1, i;
    if (!f) { printf("FAIL open\n"); return 1; }
    if (!f2) { printf("FAIL open TRAM 2 image\n"); return 1; }
    sz = (long)fread(img, 1, sizeof img, f);
    fclose(f);
    sz2 = (int)fread(img2, 1, sizeof img2, f2);
    fclose(f2);
    ChanOut(LOUT(1), img, (int)sz);
    ChanOut(LOUT(1), &sz2, 4);                    /* TRAM 1 boots TRAM 2 with this */
    ChanOut(LOUT(1), img2, sz2);
    printf("  TRAM 1 booted (%ld B), TRAM 2 image handed over (%d B)\n", sz, sz2); fflush(stdout);
    while (!quiet_for(15625)) {
        int len, rl;
        ChanIn(LIN(1), pkt, 2);
        len = pkt[0] | (pkt[1] << 8);
        if (len < 1 || len > (int)sizeof pkt) { printf("FAIL bad start-up packet %d\n", len); return 1; }
        ChanIn(LIN(1), pkt, len);
        if (pkt[0] == 32 && len >= 13 && !memcmp(pkt + 3, "IBOARDSIZE", 10)) {
            int bl = (int)strlen(board);
            rep[2] = 0; rep[3] = (unsigned char)bl; rep[4] = 0; memcpy(rep + 5, board, bl); rl = 3 + bl;
        } else if (pkt[0] == 32) { rep[2] = 129; rl = 1; }
        else if (pkt[0] == 40) { rep[2] = rep[3] = rep[4] = 0; rl = 3; }
        else if (pkt[0] == 42) { rep[2] = rep[3] = rep[4] = rep[5] = rep[6] = 0; rl = 5; }
        else { rep[2] = 1; rl = 1; }
        rep[0] = (unsigned char)rl; rep[1] = (unsigned char)(rl >> 8);
        ChanOut(LOUT(1), rep, rl + 2);
        printf("  start-up request %d answered (board %s)\n", pkt[0], board); fflush(stdout);
        served++;
    }
    for (i = 0; i < 16; i++) msg[i] = (unsigned char)(i * 7 + 1);
    ProcRun(ProcAlloc(sender, 4096, 0));
    for (i = 0; i < 20 && stage < 4; i++) {
        ProcTimerAlt(ProcTimePlus(ProcTime(), 15625), NULL);
        printf("  %2d s: stage %d\n", i + 1, stage); fflush(stdout);
    }
    if (stage < 4) { printf("FAIL: worker stopped at stage %d (0 header unsent, 1 header taken, 2 message taken, 3 length back)\n", stage); return 1; }
    for (i = 0; i < 16; i++) ok &= back[i] == (unsigned char)(msg[i] ^ 0x5A ^ 0xA5);   /* both hops */
    printf("%s: booted %ld bytes, served %d start-up requests, echo n=%d %s\n",
           ok && rn == 16 ? "PASS" : "FAIL", sz, served, rn, ok ? "data ok" : "data BAD");
    return 0;
}
