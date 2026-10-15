/* FR-2: the echo worker built with llvm-t800 for a TRAM (T4-class, no FPU, 4 MB), -mhost=none: no start-up
 * conversation. Link 3 (slot-1 L3 faces the T425's L1): int n, n bytes in; n and the bytes XOR 0x5A out. */
#include <channel.h>
#define LOUT(k) ((Channel *)(0x80000000u + 4u * (unsigned)(k)))
#define LIN(k)  ((Channel *)(0x80000010u + 4u * (unsigned)(k)))
int main(void)
{
    static unsigned char b[4096];
    int n, i;
    for (;;) {
        ChanIn(LIN(3), &n, 4);
        if (n < 0 || n > 4096) n = 0;
        ChanIn(LIN(3), b, n);
        for (i = 0; i < n; i++) b[i] ^= 0x5A;
        ChanOut(LOUT(3), &n, 4);
        ChanOut(LOUT(3), b, n);
    }
}
