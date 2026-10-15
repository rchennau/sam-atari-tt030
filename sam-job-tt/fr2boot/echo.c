/* FR-2 boot experiment: INMOS standalone echo worker (reduced runtime, no host). Link 1 in/out:
 * int n, then n bytes; replies n and the bytes XOR 0x5A. */
#include <channel.h>
#define LIN(k)  ((Channel *)(0x80000010L + 4 * (k)))
#define LOUT(k) ((Channel *)(0x80000000L + 4 * (k)))
int main(void)
{
    static unsigned char b[4096];
    int n, i;
    for (;;) {
        ChanIn(LIN(1), (char *)&n, 4);
        if (n < 0 || n > 4096) n = 0;
        ChanIn(LIN(1), (char *)b, n);
        for (i = 0; i < n; i++) b[i] ^= 0x5A;
        ChanOut(LOUT(1), (char *)&n, 4);
        ChanOut(LOUT(1), (char *)b, n);
    }
    return 0;
}
