/* gifserv — netpbm's giftopnm on the T425 (tt030-t425-kernel-ports W2). t4serv.c is the loop; the work is
 * giftopnm.c unchanged, run through pmshim.c (the same code tgiftopnm runs on the 68030).
 *   op 'g'   payload = image number (4, LE; 0 or 1 = the first) || a GIF file; result = the PNM file
 *            (P4/P5/P6, as giftopnm chooses from the colormap). The decoded image (4 bytes a pixel) and the
 *            output both live on the board: tgiftopnm sizes the request to its ~5 MB. */
#include "t4serv.h"
#include "gifrun.h"

long t4_kernel(int op, int level, const unsigned char *in, long n, unsigned char *out, long cap)
{
    (void)level;
    if (op != 'g' || n < 17)
        return -1;
    return gf_run(in[0] | ((long)in[1] << 8) | ((long)in[2] << 16) | ((long)in[3] << 24), in + 4, n - 4, out, cap);
}

long t4_out_cap(int op, int level, const unsigned char *in, long n)
{
    (void)level;
    if (op != 'g' || n < 17)
        return 0;
    return gf_cap(in + 4, n - 4);
}
