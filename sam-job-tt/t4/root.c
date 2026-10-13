/* root (T425): jobwire dispatcher. Talks raw jobwire to ttjobd on the host link after "RDY0"; node 0 runs here,
 * nodes 1 and 2 go to TRAM 1 (which relays node 2). Phase 1: synchronous, explicit node. */
#include <misc.h>
#include "jobnode.h"
#include "../jobwire.h"
int main(void)
{
    Channel *hin = (Channel *)get_param(1), *hout = (Channel *)get_param(2);
    Channel *t1o = (Channel *)get_param(3), *t1i = (Channel *)get_param(4);
    static char ready[4] = { 'R', 'D', 'Y', '0' };
    unsigned char hdr[JOB_HDR];
    ChanOut(hout, ready, 4);
    for (;;) {
        ChanIn(hin, (char *)hdr, JOB_HDR);
        if (hdr[3] == 0) job_run_here(hin, hout, hdr);
        else job_forward(hin, hout, hdr, t1o, t1i);
    }
}
