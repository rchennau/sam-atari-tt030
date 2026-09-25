/* compserv — the compression family on the T425 (t425-compress; tt030-t425-kernel-ports FR-2).
 * The server loop is t4serv.c; this file is the family's two hooks. Ops: see compkern.h. */
#include "t4serv.h"
#include "compkern.h"

long t4_kernel(int op, int level, const unsigned char *in, long n, unsigned char *out, long cap)
{
    return ck_compress(op, level, in, n, out, cap);
}

long t4_out_cap(int op, int level, const unsigned char *in, long n)
{
    (void)level;
    if (op == 'i')                               /* inflate: the original size leads the payload */
        return n >= 4 ? (in[0] | ((long)in[1] << 8) | ((long)in[2] << 16) | ((long)in[3] << 24)) + 64 : 0;
    return n + n / 2 + 1024;
}
