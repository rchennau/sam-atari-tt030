/* bcserv — GNU bc 1.05 on the T425 (tt030-t425-kernel-ports W3). t4serv.c is the loop; the work is bc's own
 * sources through bcshim.c (the same code tbc runs on the 68030). One request per boot: t4call boots a
 * fresh server in every tbc process, so bc's globals start clean.
 *   op 'c'   level = 1 for -l (math library); payload = the program text; result = bs_run's layout
 *            (stdout length 4 LE, exit code 1, stdout, stderr). Output up to BCS_OUT bytes. */
#include "t4serv.h"
#include "bcshim.h"

#define BCS_OUT (1024L * 1024)

long t4_kernel(int op, int level, const unsigned char *in, long n, unsigned char *out, long cap)
{
    if (op != 'c')
        return -1;
    return bs_run(level == 1, (const char *)in, n, out, cap);
}

long t4_out_cap(int op, int level, const unsigned char *in, long n)
{
    (void)level; (void)in; (void)n;
    return op == 'c' ? BCS_OUT : 0;
}
