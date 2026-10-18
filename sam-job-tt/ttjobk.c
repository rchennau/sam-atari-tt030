/* ttjobk — ask the running ttjobd's T425 for OP_CAPS: its payload cap and the largest block its heap can give
 * now (ttjob-ports FR-7, NFR-4/5 headroom). Prints "cap N hiwater N heap N leftover last N total N"; exit 1 if ttjobd or the op is unavailable.
 * Build: m68k-atari-mint-gcc -m68020-60 -O2 -s -Isam-job-tt -Isam-yum-tt/src -o ttjobk sam-job-tt/ttjobk.c
 *        sam-job-tt/libttjob.c sam-yum-tt/src/t4adler.c */
#include <stdio.h>
#include "libttjob.h"
#include "jobwire.h"

static unsigned long g(const unsigned char *b)
{ return b[0] | (unsigned long)b[1] << 8 | (unsigned long)b[2] << 16 | (unsigned long)b[3] << 24; }

int main(void)
{
    unsigned char out[24];
    long olen = 0;
    int id = ttjob_submit(OP_CAPS, 0, "", 0), st;
    if (id <= 0 || (st = ttjob_wait(id, out, sizeof out, &olen, 30000L)) != ST_OK || olen < 24) {
        fprintf(stderr, "ttjobk: no answer (ttjobd down, or an image without OP_CAPS)\n");
        return 1;
    }
    printf("cap %lu hiwater %lu heap %lu leftover last %lu total %lu\n", g(out), g(out + 4), g(out + 12) - g(out + 8),
           g(out + 16), g(out + 20));
    return 0;
}
