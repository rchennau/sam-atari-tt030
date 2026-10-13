/* shmserv (T425): SharedMem data path test. SharedMem is T425 0x807F4000 (39 KB; measured 2026-10-12, shmprobe).
 * After "RDY0" on link 0, each request is a 4-byte little-endian word on the link (the doorbell):
 *   n (1..39936)  complement the first n bytes of SharedMem in place, then reply 1 byte
 *   0x40000000|n mailbox: complement n bytes of the input half (offset 0) into the output half (offset 19968);
 *                 reply 8 bytes. Each half is written by one side only.
 *   0xFFFFFFFE    write the 16-byte marker "T425WRIT" de ad be ef 01 23 45 67 at SharedMem with 32-bit stores,
 *                 then reply the 16 bytes the T425 reads back there (where do T425 writes land? 2026-10-12)
 *   0xFFFFFFFF    bandwidth: 50 x read 32 KB from SharedMem, 50 x write 32 KB; reply both tick counts (4+4)
 * Stays inside SharedMem: everything else from 0x80600000 up is the screen. No host runtime calls after boot. */
#include <channel.h>
#include <process.h>
#define SHM ((volatile unsigned char *)0x807F4000)
#define SHMLEN 39936UL
static unsigned char local[32768];
static volatile unsigned long sinkv;
static void put4(unsigned long v)
{
    unsigned char b[4];
    b[0] = (unsigned char)v; b[1] = (unsigned char)(v >> 8); b[2] = (unsigned char)(v >> 16); b[3] = (unsigned char)(v >> 24);
    ChanOut(LINK0OUT, (char *)b, 4);
}
int main(void)
{
    static char ready[4] = { 'R', 'D', 'Y', '0' };
    unsigned char b[4];
    unsigned long n, i;
    int k, t0, tr, tw;
    ChanOut(LINK0OUT, ready, 4);
    for (;;) {
        ChanIn(LINK0IN, (char *)b, 4);
        n = b[0] | ((unsigned long)b[1] << 8) | ((unsigned long)b[2] << 16) | ((unsigned long)b[3] << 24);
        if (n == 0xFFFFFFFEUL) {
            static const unsigned char m[16] = { 'T','4','2','5','W','R','I','T', 0xde,0xad,0xbe,0xef,0x01,0x23,0x45,0x67 };
            volatile unsigned long *w = (volatile unsigned long *)SHM;
            unsigned char rb[16];
            for (k = 0; k < 4; k++)
                w[k] = m[4*k] | ((unsigned long)m[4*k+1] << 8) | ((unsigned long)m[4*k+2] << 16) | ((unsigned long)m[4*k+3] << 24);
            for (k = 0; k < 16; k++) rb[k] = SHM[k];
            ChanOut(LINK0OUT, (char *)rb, 16);
            continue;
        }
        if (n == 0xFFFFFFFFUL) {
            volatile unsigned long *s = (volatile unsigned long *)SHM;
            unsigned long *l = (unsigned long *)local;
            t0 = ProcTime();
            for (k = 0; k < 50; k++) for (i = 0; i < 8192; i++) l[i] = s[i];
            tr = ProcTimeMinus(ProcTime(), t0);
            t0 = ProcTime();
            for (k = 0; k < 50; k++) for (i = 0; i < 8192; i++) s[i] = l[i];
            tw = ProcTimeMinus(ProcTime(), t0);
            put4((unsigned long)tr); put4((unsigned long)tw);
            continue;
        }
        if (n & 0x40000000UL) {               /* mailbox: complement input half into the output half */
            /* bit 29: read the input through the 2 MB alias 0x809F4000; bit 28: first READ 64 KB of other video
             * memory (0x80600000, read-only) — tests for a stale T425-side copy (2026-10-12) */
            volatile unsigned char *src = (n & 0x20000000UL) ? (volatile unsigned char *)0x809F4000 : SHM;
            if (n & 0x10000000UL) {            /* evict size = 64 << ((n >> 16) & 15) bytes, read-only */
                volatile unsigned long *e = (volatile unsigned long *)0x80600000; unsigned long x = 0, ew = (64UL << ((n >> 16) & 15)) / 4;
                for (i = 0; i < ew; i++) x += e[i];
                sinkv = x;
            }
            n &= 0xFFFFUL;
            if (n > SHMLEN / 2) n = SHMLEN / 2;
            for (i = 0; i < n; i++) SHM[SHMLEN / 2 + i] = (unsigned char)~src[i];
            { unsigned char ok8[8] = { 'M', 'B', 0, 0, 0, 0, 0, 0 }; ChanOut(LINK0OUT, (char *)ok8, 8); }
            continue;
        }
        if (n > SHMLEN) n = SHMLEN;
        {   /* diagnostic reply: first 4 bytes as read before, and after, the in-place complement */
            unsigned char ba[8];
            for (k = 0; k < 4; k++) ba[k] = SHM[k];
            for (i = 0; i < n; i++) SHM[i] = (unsigned char)~SHM[i];
            for (k = 0; k < 4; k++) ba[4 + k] = SHM[k];
            ChanOut(LINK0OUT, (char *)ba, 8);
        }
    }
}
