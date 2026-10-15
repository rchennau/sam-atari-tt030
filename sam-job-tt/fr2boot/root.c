/* FR-2 boot experiment, llvm side: on the ATW800/2 T425, boot the INMOS echo worker (echoa.btl, built -ta) onto TRAM 1
 * over link 1 (T425 L1 -> slot-1 L3), then send one 16-byte message and check the XOR-0x5A echo. Prints PASS/FAIL. */
#include <channel.h>
#include <stdio.h>
#include <stdlib.h>
#define LOUT(n) ((Channel *)(0x80000000u + 4u * (unsigned)(n)))
#define LIN(n)  ((Channel *)(0x80000010u + 4u * (unsigned)(n)))
int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "echoa.btl";
    static unsigned char img[65536], msg[16], rep[16];
    FILE *f = fopen(path, "rb");
    long sz;
    int n = 16, rn = 0, i, ok = 1;
    if (!f) { printf("FAIL open %s\n", path); return 1; }
    sz = (long)fread(img, 1, sizeof img, f);
    fclose(f);
    ChanOut(LOUT(1), img, (int)sz);          /* hard link: the boot stream just flows */
    for (i = 0; i < 16; i++) msg[i] = (unsigned char)(i * 7 + 1);
    ChanOut(LOUT(1), &n, 4);
    ChanOut(LOUT(1), msg, 16);
    ChanIn(LIN(1), &rn, 4);
    ChanIn(LIN(1), rep, 16);
    for (i = 0; i < 16; i++) ok &= rep[i] == (unsigned char)(msg[i] ^ 0x5A);
    printf("%s booted %ld bytes; echo n=%d %s\n", ok && rn == 16 ? "PASS" : "FAIL", sz, rn, ok ? "data ok" : "data BAD");
    return 0;
}
