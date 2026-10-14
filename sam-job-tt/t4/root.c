/* root (T425): jobwire dispatcher, Phase 4 (FR-6). Three processes:
 *   host  — reads each request from ttjobd, picks a node (requested one, or for NODE_ANY the first idle of
 *           T425, TRAM 1, TRAM 2), marks it busy and hands the job over (local worker, or down the TRAM 1 link);
 *   work0 — runs node-0 jobs on the T425;
 *   mux   — takes replies from work0 and the TRAM 1 link, forwards each to ttjobd, THEN tells host the node is
 *           free (after, so a node's reply buffer is never reused before it has gone out).
 * ttjobd keeps at most NODES jobs in flight, so a NODE_ANY job always finds an idle node. */
#include <misc.h>
#include <process.h>
#include <stddef.h>
#include "jobnode.h"
#include "../jobwire.h"

static unsigned char hdr0[JOB_HDR], in0[JOB_MAX], rep0[LREP_HDR + JOB_MAX];
static unsigned char hdrf[JOB_HDR], fwd[JOB_MAX], repm[LREP_HDR + JOB_MAX];

static void work0(Process *p, Channel *go, Channel *done)
{
    long n, len;
    (void)p;
    for (;;) {
        n = (long)ChanInInt(go);
        len = job_reply(hdr0, in0, n, rep0, 0);
        ChanOutInt(done, (int)len);
    }
}

static void mux(Process *p, Channel *done0, Channel *t1i, Channel *hout, Channel *freec)
{
    long len;
    int node;
    (void)p;
    for (;;) {
        if (!t1i || ProcAlt(done0, t1i, NULL) == 0) {     /* SOLO image: no TRAM link, only the local worker */
            len = (long)ChanInInt(done0);
            ChanOut(hout, (char *)rep0, (int)len);
            node = 0;
        } else {
            len = rep_read(t1i, repm);
            ChanOut(hout, (char *)repm, (int)len);
            node = repm[R_NODE];
        }
        ChanOutInt(freec, node);
    }
}

static void host(Process *p, Channel *hin, Channel *t1o, Channel *go0, Channel *freec)
{
    int busy[NODES] = { 0, 0, 0 }, node, k;
    unsigned char h[JOB_HDR];
    long n;
    (void)p;
    for (;;) {
        if (ProcAlt(freec, hin, NULL) == 0) { busy[ChanInInt(freec)] = 0; continue; }
        ChanIn(hin, (char *)h, JOB_HDR);
        node = h[3];
        if (node == NODE_ANY || node >= NODES) {
            for (;;) {                       /* first idle node, fastest first; wait for one if none */
                for (k = 0; k < NODES && busy[k]; k++) ;
                if (k < NODES) break;
                busy[ChanInInt(freec)] = 0;
            }
            node = k;
        } else
            while (busy[node]) busy[ChanInInt(freec)] = 0;
        h[3] = (unsigned char)node;
        busy[node] = 1;
        n = (long)g4(h + H_LEN);
        if (node == 0) {
            for (k = 0; k < JOB_HDR; k++) hdr0[k] = h[k];
            if (n < 0 || n > JOB_MAX) { n = -1; }
            else if (n > 0) ChanIn(hin, (char *)in0, (int)n);
            ChanOutInt(go0, (int)n);
        } else if (t1o) {
            for (k = 0; k < JOB_HDR; k++) hdrf[k] = h[k];
            if (n > 0 && n <= JOB_MAX) ChanIn(hin, (char *)fwd, (int)n);
            ChanOut(t1o, (char *)hdrf, JOB_HDR);
            if (n > 0 && n <= JOB_MAX) ChanOut(t1o, (char *)fwd, (int)n);
        } else {                                   /* no such node in this image: drain; ttjobd never sends these */
            if (n > 0 && n <= JOB_MAX) ChanIn(hin, (char *)fwd, (int)n);
            busy[node] = 0;
        }
    }
}

int main(void)
{
    Channel *hin = (Channel *)get_param(1), *hout = (Channel *)get_param(2);
#ifdef SOLO                                               /* jobt0.btl: T425 only (FR-8 fallback) */
    Channel *t1o = NULL, *t1i = NULL;
#else
    Channel *t1o = (Channel *)get_param(3), *t1i = (Channel *)get_param(4);
#endif
    Channel *go0 = ChanAlloc(), *done0 = ChanAlloc(), *freec = ChanAlloc();
    static char ready[4] = { 'R', 'D', 'Y', '0' };
    Process *ph, *pw, *pm;
    ChanOut(hout, ready, 4);
    ph = ProcAlloc(host, 4096, 4, hin, t1o, go0, freec);
    pw = ProcAlloc(work0, 32768, 2, go0, done0);          /* zlib/bzip2/crypto run on this stack */
    pm = ProcAlloc(mux, 4096, 4, done0, t1i, hout, freec);
    if (!ph || !pw || !pm) for (;;) ;                    /* out of memory: no RDY reply follows, ttjobd times out */
    ProcPar(ph, pw, pm, NULL);
    return 0;
}
