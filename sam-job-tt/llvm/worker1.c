/* worker1 (TRAM 1), Phase 4: route (node-1 jobs to the local worker, node-2 jobs down the TRAM 2 link), work, and
 * mux (replies from the local worker and from TRAM 2 back up to the T425).
 * llvm-t800 port: link 3 <-> Root (T425), link 1 <-> TRAM 2. */
#include <channel.h>
#include <process.h>
#include <stddef.h>
#include "jobnode.h"
#include "../jobwire.h"

#define LOUT(k) ((Channel *)(0x80000000u + 4u * (unsigned)(k)))
#define LIN(k)  ((Channel *)(0x80000010u + 4u * (unsigned)(k)))

static unsigned char hdr1[JOB_HDR], in1[JOB_MAX], rep1[LREP_HDR + JOB_MAX];
static unsigned char hdrf[JOB_HDR], fwd[JOB_MAX], repm[LREP_HDR + JOB_MAX];

static void work1(Channel *go, Channel *done)
{
    long n, len;
    for (;;) {
        n = (long)ChanInInt(go);
        len = job_reply(hdr1, in1, n, rep1, 1);
        ChanOutInt(done, (int)len);
    }
    ProcStop();
}

static void mux1(Channel *done1, Channel *t2i, Channel *ro)
{
    long len;
    for (;;) {
        if (!t2i || ProcAlt(done1, t2i, NULL) == 0) {     /* NO_T2 image: TRAM 2 absent */
            len = (long)ChanInInt(done1);
            ChanOut(ro, rep1, (int)len);
        } else {
            len = rep_read(t2i, repm);
            ChanOut(ro, repm, (int)len);
        }
    }
    ProcStop();
}

static void route(Channel *ri, Channel *t2o, Channel *go1)
{
    unsigned char h[JOB_HDR];
    long n;
    int k;
    for (;;) {
        ChanIn(ri, h, JOB_HDR);
        n = (long)g4(h + H_LEN);
        if (h[3] == 1) {
            for (k = 0; k < JOB_HDR; k++) hdr1[k] = h[k];
            if (n < 0 || n > JOB_MAX) n = -1;
            else if (n > 0) ChanIn(ri, in1, (int)n);
            ChanOutInt(go1, (int)n);
        } else if (t2o) {
            for (k = 0; k < JOB_HDR; k++) hdrf[k] = h[k];
            if (n > 0 && n <= JOB_MAX) ChanIn(ri, fwd, (int)n);
            ChanOut(t2o, hdrf, JOB_HDR);
            if (n > 0 && n <= JOB_MAX) ChanOut(t2o, fwd, (int)n);
        } else if (n > 0 && n <= JOB_MAX) {
            ChanIn(ri, fwd, (int)n);       /* no TRAM 2 in this image: drain; ttjobd never sends these */
        }
    }
    ProcStop();
}

int main(void)
{
#ifndef NO_T2
    static unsigned char img2[262144];
    int n2;
    ChanIn(LIN(3), &n2, 4);
    if (n2 > 0 && n2 <= (int)sizeof(img2)) {
        ChanIn(LIN(3), img2, n2);
        ChanOut(LOUT(1), img2, n2);
    }
#endif

    Channel *ri = LIN(3), *ro = LOUT(3);
#ifdef NO_T2                                              /* NO_T2 image: TRAM 2 absent */
    Channel *t2o = NULL, *t2i = NULL;
#else
    Channel *t2o = LOUT(1), *t2i = LIN(1);
#endif
    static Channel go1, done1;
    ChanInit(&go1);
    ChanInit(&done1);

    Process *pr = ProcAlloc(route, 8192, 3, ri, t2o, &go1);
    Process *pw = ProcAlloc(work1, 65536, 2, &go1, &done1);
    Process *pm = ProcAlloc(mux1, 8192, 3, &done1, t2i, ro);
    if (!pr || !pw || !pm) for (;;) ;
    /* llvm-t800's ProcPar returns at once instead of waiting (friction F13), and main returning ends in exit():
     * start each process and park main for good. */
    ProcRun(pr); ProcRun(pw); ProcRun(pm);
    ProcStop();
    return 0;
}
