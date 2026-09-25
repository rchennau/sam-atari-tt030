/* memserv — how much T425 memory can a server really use? (tt030-t425-kernel-ports W2, ppmquant sizing)
 * op 'M': malloc 64 KB blocks until malloc fails or 7 MB; write a pattern into every block, read it all
 * back; result = allocated bytes (4, LE) || verified bytes (4, LE). The vendor spec says 6 MB. */
#include <stdlib.h>
#include "t4serv.h"

#define BLK (64L * 1024)
#define MAXB 112                                 /* 7 MB */

static void put4(unsigned char *b, unsigned long v)
{
    b[0] = (unsigned char)v; b[1] = (unsigned char)(v >> 8); b[2] = (unsigned char)(v >> 16);
    b[3] = (unsigned char)(v >> 24);
}

long t4_kernel(int op, int level, const unsigned char *in, long n, unsigned char *out, long cap)
{
    static unsigned long *blk[MAXB];
    int k, got = 0;
    long i, ok = 0;
    (void)level; (void)in; (void)n;
    if (op != 'M' || cap < 8)
        return -1;
    while (got < MAXB && (blk[got] = (unsigned long *)malloc(BLK)) != NULL)
        got++;
    for (k = 0; k < got; k++)
        for (i = 0; i < BLK / 4; i++)
            blk[k][i] = (unsigned long)blk[k] ^ (unsigned long)i * 2654435761UL;
    for (k = 0; k < got; k++) {
        for (i = 0; i < BLK / 4; i++)
            if (blk[k][i] != ((unsigned long)blk[k] ^ (unsigned long)i * 2654435761UL))
                break;
        if (i < BLK / 4)
            break;
        ok += BLK;
    }
    for (k = 0; k < got; k++)
        free(blk[k]);
    put4(out, (unsigned long)got * BLK);
    put4(out + 4, (unsigned long)ok);
    return 8;
}

long t4_out_cap(int op, int level, const unsigned char *in, long n)
{
    (void)op; (void)level; (void)in; (void)n;
    return 16;
}
