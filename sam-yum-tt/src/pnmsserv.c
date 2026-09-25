/* pnmsserv — netpbm's pnmscale on the T425 (tt030-t425-kernel-ports W2). t4serv.c is the loop; the work is
 * pnmscale.c unchanged, run through pmshim.c (the same code tpnmscale runs on the 68030).
 *   op 's'   level = mode (pnmsrun.h), payload = x (4, LE) || y (4, LE) || a raw P5/P6 file (maxval <= 255);
 *            result = the scaled file. Input and output are separate (a scale can grow the image), so
 *            both must fit the board's ~5 MB together: tpnmscale sizes the request. */
#include "t4serv.h"
#include "pnmsrun.h"

static long le32(const unsigned char *p)
{
    return p[0] | ((long)p[1] << 8) | ((long)p[2] << 16) | ((long)p[3] << 24);
}

long t4_kernel(int op, int level, const unsigned char *in, long n, unsigned char *out, long cap)
{
    if (op != 's' || n < 19)
        return -1;
    return pn_run(level, le32(in), le32(in + 4), in + 8, n - 8, out, cap);
}

long t4_out_cap(int op, int level, const unsigned char *in, long n)
{
    if (op != 's' || n < 19 || (level != 0 && level != 1))
        return 0;
    return pn_cap(level, le32(in), le32(in + 4), in + 8, n - 8);
}
