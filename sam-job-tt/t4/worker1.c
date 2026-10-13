/* worker1 (TRAM 1): runs node-1 jobs, relays node-2 jobs to TRAM 2. */
#include <misc.h>
#include "jobnode.h"
#include "../jobwire.h"
int main(void)
{
    Channel *ri = (Channel *)get_param(1), *ro = (Channel *)get_param(2);
    Channel *t2o = (Channel *)get_param(3), *t2i = (Channel *)get_param(4);
    unsigned char hdr[JOB_HDR];
    for (;;) {
        ChanIn(ri, (char *)hdr, JOB_HDR);
        if (hdr[3] == 1) job_run_here(ri, ro, hdr);
        else job_forward(ri, ro, hdr, t2o, t2i);
    }
}
