/* pngserv — tpnmtopng's PNG encoder on the T425 (tt030-t425-kernel-ports W2). t4serv.c is the loop; the
 * work is pngenc.c + zlib 1.3.2, the same C tpnmtopng runs on the 68030.
 *   op 'p'   level = zlib level 0..9, payload = a raw P5/P6 file (maxval <= 255); result = the PNG file.
 *            Input, output (~ the pixel size) and zlib's ~260 KB all live on the board: tpnmtopng sizes it. */
#include "t4serv.h"
#include "pngenc.h"

long t4_kernel(int op, int level, const unsigned char *in, long n, unsigned char *out, long cap)
{
    if (op != 'p')
        return -1;
    return pe_encode(in, n, level, out, cap);
}

long t4_out_cap(int op, int level, const unsigned char *in, long n)
{
    if (op != 'p' || level < 0 || level > 9)
        return 0;
    return pe_cap(in, n);
}
