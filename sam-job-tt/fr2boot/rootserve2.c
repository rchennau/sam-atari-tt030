/* FR-2: llvm T425 root boots the INMOS echo worker onto TRAM 1 over link 1, answers its start-up iserver requests the
 * way atw_boot() does for the T425 (SP_GETENV IBOARDSIZE -> "#400000", the TRAM's 4 MB; SP_COMMAND; SP_ID; else
 * unimplemented) until link 1 is quiet for 1 s, then sends one 16-byte message and checks the XOR-0x5A echo. */
#include <channel.h>
#include <process.h>
#include <stdio.h>
#include <string.h>
#define LOUT(n) ((Channel *)(0x80000000u + 4u * (unsigned)(n)))
#define LIN(n)  ((Channel *)(0x80000010u + 4u * (unsigned)(n)))
static int quiet_for(int ticks) { return ProcTimerAlt(ProcTimePlus(ProcTime(), ticks), LIN(1), NULL) != 0; }
int main(int argc, char **argv)
{
    static unsigned char img[65536], pkt[1024], rep[64], msg[16], back[16];
    const char *board = "#400000";
    FILE *f = fopen(argc > 1 ? argv[1] : "echoa.btl", "rb");
    long sz;
    int served = 0, n = 16, rn = 0, ok = 1, i;
    if (!f) { printf("FAIL open\n"); return 1; }
    sz = (long)fread(img, 1, sizeof img, f);
    fclose(f);
    ChanOut(LOUT(1), img, (int)sz);
    while (!quiet_for(15625)) {                         /* 15625 ticks of 64 us = 1 s */
        int len, rl;
        printf("  link 1 data: reading a start-up packet\n"); fflush(stdout);
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
        printf("  start-up request %d answered\n", pkt[0]); fflush(stdout);
        served++;
    }
    for (i = 0; i < 16; i++) msg[i] = (unsigned char)(i * 7 + 1);
    printf("  link 1 quiet: sending the message\n"); fflush(stdout);
    ChanOut(LOUT(1), &n, 4);
    ChanOut(LOUT(1), msg, 16);
    printf("  message sent; waiting for the echo\n"); fflush(stdout);
    for (i = 0; i < 10 && quiet_for(15625); i++) { printf("  ...%d s\n", i + 1); fflush(stdout); }
    if (i == 10) { printf("FAIL: no echo in 10 s\n"); return 1; }
    ChanIn(LIN(1), &rn, 4);
    ChanIn(LIN(1), back, 16);
    for (i = 0; i < 16; i++) ok &= back[i] == (unsigned char)(msg[i] ^ 0x5A);
    printf("%s: booted %ld bytes, served %d start-up requests, echo n=%d %s\n",
           ok && rn == 16 ? "PASS" : "FAIL", sz, served, rn, ok ? "data ok" : "data BAD");
    return 0;
}
