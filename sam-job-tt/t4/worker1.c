/* worker1 (TRAM 1), Phase 4: route (node-1 jobs to the local worker, node-2 jobs down the TRAM 2 link), work, and
 * mux (replies from the local worker and from TRAM 2 back up to the T425). */
#include <misc.h>
#include <process.h>
#include <stddef.h>
#include "jobnode.h"
#include "../jobwire.h"

static unsigned char hdr1[JOB_HDR], in1[JOB_MAX], rep1[LREP_HDR + JOB_MAX];
static unsigned char hdrf[JOB_HDR], fwd[JOB_MAX], repm[LREP_HDR + JOB_MAX];

static void work1(Process *p, Channel *go, Channel *done)
{
    long n, len;
    (void)p;
    for (;;) { n = (long)ChanInInt(go); len = job_reply(hdr1, in1, n, rep1, 1); ChanOutInt(done, (int)len); }
}

static void mux1(Process *p, Channel *done1, Channel *t2i, Channel *ro)
{
    long len;
    (void)p;
    for (;;) {
        if (ProcAlt(done1, t2i, NULL) == 0) { len = (long)ChanInInt(done1); ChanOut(ro, (char *)rep1, (int)len); }
        else { len = rep_read(t2i, repm); ChanOut(ro, (char *)repm, (int)len); }
    }
}

static void route(Process *p, Channel *ri, Channel *t2o, Channel *go1)
{
    unsigned char h[JOB_HDR];
    long n;
    int k;
    (void)p;
    for (;;) {
        ChanIn(ri, (char *)h, JOB_HDR);
        n = (long)g4(h + H_LEN);
        if (h[3] == 1) {
            for (k = 0; k < JOB_HDR; k++) hdr1[k] = h[k];
            if (n < 0 || n > JOB_MAX) n = -1; else if (n > 0) ChanIn(ri, (char *)in1, (int)n);
            ChanOutInt(go1, (int)n);
        } else {
            for (k = 0; k < JOB_HDR; k++) hdrf[k] = h[k];
            if (n > 0 && n <= JOB_MAX) ChanIn(ri, (char *)fwd, (int)n);
            ChanOut(t2o, (char *)hdrf, JOB_HDR);
            if (n > 0 && n <= JOB_MAX) ChanOut(t2o, (char *)fwd, (int)n);
        }
    }
}

int main(void)
{
    Channel *ri = (Channel *)get_param(1), *ro = (Channel *)get_param(2);
    Channel *t2o = (Channel *)get_param(3), *t2i = (Channel *)get_param(4);
    Channel *go1 = ChanAlloc(), *done1 = ChanAlloc();
    Process *pr = ProcAlloc(route, 4096, 3, ri, t2o, go1);
    Process *pw = ProcAlloc(work1, 32768, 2, go1, done1);
    Process *pm = ProcAlloc(mux1, 4096, 3, done1, t2i, ro);
    if (!pr || !pw || !pm) for (;;) ;
    ProcPar(pr, pw, pm, NULL);
    return 0;
}
