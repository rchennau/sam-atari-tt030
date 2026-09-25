/* bct4test — bc through bcshim under t4 vs on the host (tt030-t425-kernel-ports W3). One program per run
 * (bc's globals are not reset between runs; bcserv serves one request per boot): the program index is
 * compiled in with -DBCT=n. Prints result length, exit code, checksum and the head of stdout. */
#include <stdio.h>
#include "bcshim.h"

#ifndef BCT
#define BCT 0
#endif

static const char *prog[] = {
    "scale=40; 4*a(1)\n",
    "2^700\n",
    "scale=60; sqrt(2)\n",
    "define f(n) {\n if (n < 2) return (1);\n return (n * f(n - 1));\n}\nf(120)\n",
    "x = 1 +\n",
    "for (i = 0; i < 5; i++) { i^i; }\nquit\n",
};
static const int lib[] = { 1, 0, 0, 0, 0, 0 };

int main(void)
{
    static unsigned char out[65536];
    long r, i;
    unsigned long s = 0;
    r = bs_run(lib[BCT], prog[BCT], (long)strlen(prog[BCT]), out, sizeof out);
    for (i = 0; i < r; i++)
        s = s * 31 + out[i];
    printf("prog %d -> %ld exit %d %08lx | %.60s\n", BCT, r, r > 4 ? out[4] : -1, s & 0xffffffffUL,
           r > 5 ? (char *)out + 5 : "");
    return 0;
}
