/* FR-2 relay test, TRAM 1: receives TRAM 2's boot image on link 3 (int len + bytes) and boots it over link 1, then
 * relays: message in on link 3 -> out on link 1 -> reply from link 1, XOR 0x5A -> back on link 3. */
#include <channel.h>
#define LOUT(k) ((Channel *)(0x80000000u + 4u * (unsigned)(k)))
#define LIN(k)  ((Channel *)(0x80000010u + 4u * (unsigned)(k)))
int main(void)
{
    static unsigned char img[65536], b[4096];
    int n, i;
    ChanIn(LIN(3), &n, 4);
    if (n > 0 && n <= (int)sizeof img) { ChanIn(LIN(3), img, n); ChanOut(LOUT(1), img, n); }
    for (;;) {
        ChanIn(LIN(3), &n, 4);
        if (n < 0 || n > 4096) n = 0;
        ChanIn(LIN(3), b, n);
        ChanOut(LOUT(1), &n, 4);
        ChanOut(LOUT(1), b, n);
        ChanIn(LIN(1), &n, 4);
        ChanIn(LIN(1), b, n);
        for (i = 0; i < n; i++) b[i] ^= 0x5A;
        ChanOut(LOUT(3), &n, 4);
        ChanOut(LOUT(3), b, n);
    }
}
