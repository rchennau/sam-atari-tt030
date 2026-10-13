/* worker2 (TRAM 2): run every job it is sent, one at a time (the dispatcher sends it one only when idle). */
#include <misc.h>
#include "jobnode.h"
#include "../jobwire.h"
static unsigned char hdr[JOB_HDR], in[JOB_MAX], rep[LREP_HDR + JOB_MAX];
int main(void)
{
    Channel *ri = (Channel *)get_param(1), *ro = (Channel *)get_param(2);
    long n, len;
    for (;;) {
        n = job_read(ri, hdr, in);
        len = job_reply(hdr, in, n, rep, 2);
        ChanOut(ro, (char *)rep, (int)len);
    }
}
