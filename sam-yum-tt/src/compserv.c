/* compserv — the compression family on the T425 (t425-compress; tt030-t425-kernel-ports FR-2).
 * The server loop is t4serv.c; this file is the family's two hooks. Ops: see compkern.h. */
#include "t4serv.h"
#include "compkern.h"

long t4_kernel(int op, int level, const unsigned char *in, long n, unsigned char *out, long cap)
{
    return ck_compress(op, level, in, n, out, cap);
}

long t4_out_cap(long n)
{
    return n + n / 2 + 1024;
}
