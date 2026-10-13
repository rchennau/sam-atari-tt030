/* shmscan (T425): find where the T425 sees the ATW800/2 SharedMem (TT 0xFEDF4000, Programmer's Manual p.5).
 * READS ONLY — T425 writes into card memory blanked the display on 2026-09-25 (inf-card). Booted raw by
 * shmprobe (atw_boot); after "RDY0" on link 0, per request: 68030 sends the 16-byte marker COMPLEMENTED (so this
 * program's own copy never matches), then start (4) and length (4), little-endian; reply: count (4) and up to
 * 16 hit addresses (4 each). A hit is the marker as bytes (kind 0) or with each 16-bit pair swapped (kind 1),
 * checked at every 4 KB step; kind is in bit 0 of the reported address. No host runtime calls after boot. */
#include <channel.h>
static unsigned char nmark[16];
static unsigned long get4(void)
{
    unsigned char b[4];
    ChanIn(LINK0IN, (char *)b, 4);
    return b[0] | ((unsigned long)b[1] << 8) | ((unsigned long)b[2] << 16) | ((unsigned long)b[3] << 24);
}
static void put4(unsigned long v)
{
    unsigned char b[4];
    b[0] = (unsigned char)v; b[1] = (unsigned char)(v >> 8); b[2] = (unsigned char)(v >> 16); b[3] = (unsigned char)(v >> 24);
    ChanOut(LINK0OUT, (char *)b, 4);
}
int main(void)
{
    static char ready[4] = { 'R', 'D', 'Y', '0' };
    unsigned long start, len, a, hits[16];
    int nh, k, ok0, ok1;
    ChanOut(LINK0OUT, ready, 4);
    for (;;) {
        ChanIn(LINK0IN, (char *)nmark, 16);
        start = get4(); len = get4();
        nh = 0;
        for (a = start; a - start < len; a += 4096) {
            volatile unsigned char *p = (volatile unsigned char *)a;
            ok0 = ok1 = 1;
            for (k = 0; k < 16 && (ok0 || ok1); k++) {
                unsigned char m = (unsigned char)~nmark[k], v = p[k];
                if (v != m) ok0 = 0;
                if (p[k ^ 1] != m) ok1 = 0;
            }
            if ((ok0 || ok1) && nh < 16)
                hits[nh++] = a | (ok0 ? 0 : 1);
        }
        put4((unsigned long)nh);
        for (k = 0; k < nh; k++)
            put4(hits[k]);
    }
}
