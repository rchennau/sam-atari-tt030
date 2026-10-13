/* worker2 (TRAM 2): runs every job it is sent. */
#include <misc.h>
#include "jobnode.h"
#include "../jobwire.h"
int main(void)
{
    Channel *ri = (Channel *)get_param(1), *ro = (Channel *)get_param(2);
    unsigned char hdr[JOB_HDR];
    for (;;) {
        ChanIn(ri, (char *)hdr, JOB_HDR);
        job_run_here(ri, ro, hdr);
    }
}
