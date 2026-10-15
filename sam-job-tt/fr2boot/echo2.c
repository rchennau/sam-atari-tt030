/* FR-2 relay test, TRAM 2: llvm echo on link 0 (faces TRAM 1's link 1); replies the bytes XOR 0xA5. */
#include <channel.h>
#define LOUT(k) ((Channel *)(0x80000000u + 4u * (unsigned)(k)))
#define LIN(k)  ((Channel *)(0x80000010u + 4u * (unsigned)(k)))
int main(void)
{
    static unsigned char b[4096];
    int n, i;
    for (;;) {
        ChanIn(LIN(0), &n, 4);
        if (n < 0 || n > 4096) n = 0;
        ChanIn(LIN(0), b, n);
        for (i = 0; i < n; i++) b[i] ^= 0xA5;
        ChanOut(LOUT(0), &n, 4);
        ChanOut(LOUT(0), b, n);
    }
}
