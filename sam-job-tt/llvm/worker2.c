/* worker2 (TRAM 2): run every job it is sent, one at a time (the dispatcher sends it one only when idle).
 * llvm-t800 port: link 0 (LIN(0), LOUT(0)). */
#include <channel.h>
#include <stddef.h>
#include "jobnode.h"
#include "../jobwire.h"

#define LOUT(k) ((Channel *)(0x80000000u + 4u * (unsigned)(k)))
#define LIN(k)  ((Channel *)(0x80000010u + 4u * (unsigned)(k)))

static unsigned char hdr[JOB_HDR], in[JOB_MAX], rep[LREP_HDR + JOB_MAX];

int main(void)
{
    Channel *ri = LIN(0), *ro = LOUT(0);
    long n, len;
    for (;;) {
        n = job_read(ri, hdr, in);
        len = job_reply(hdr, in, n, rep, 2, JOB_MAX);
        ChanOut(ro, rep, (int)len);
    }
    return 0;
}
