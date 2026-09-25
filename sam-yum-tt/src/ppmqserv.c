/* ppmqserv — netpbm's ppmquant on the T425 (tt030-t425-kernel-ports W2). t4serv.c is the loop; the work is
 * ppmquant.c + libppm3.c unchanged, run through pmshim.c (the same code tppmquant runs on the 68030).
 *   op 'q'   payload = colors (4, LE) || a raw PPM file (P6, maxval <= 255); result = the quantized PPM
 *            file, written over the request (T4_IN_PLACE: input + pixel array + output would not fit the
 *            board's ~5 MB otherwise). Boot with IBOARDSIZE #500000 (measured usable: 4.94 MB). */
#include "t4serv.h"
#include "pmshim.h"

long t4_kernel(int op, int level, const unsigned char *in, long n, unsigned char *out, long cap)
{
    long colors;
    (void)level;
    if (op != 'q' || n < 15)
        return -1;
    colors = in[0] | ((long)in[1] << 8) | ((long)in[2] << 16) | ((long)in[3] << 24);
    if (colors < 2 || colors > 32767)
        return -1;
    return ps_run((int)colors, in + 4, n - 4, out, cap);
}

long t4_out_cap(int op, int level, const unsigned char *in, long n)
{
    (void)op; (void)level; (void)in; (void)n;
    return T4_IN_PLACE;
}
