/* root (T425): jobwire dispatcher, Phase 4 (FR-6). Three processes:
 *   host  — reads each request from ttjobd, picks a node (requested one, or for NODE_ANY the first idle of
 *           T425, TRAM 1, TRAM 2), marks it busy and hands the job over (local worker, or down the TRAM 1 link);
 *   work0 — runs node-0 jobs on the T425;
 *   mux   — takes replies from work0 and the TRAM 1 link, forwards each to ttjobd, THEN tells host the node is
 *           free (after, so a node's reply buffer is never reused before it has gone out).
 * ttjobd keeps at most NODES jobs in flight, so a NODE_ANY job always finds an idle node.
 * Node 0's buffers come from the heap per job (ttjob-ports FR-7, operator MCQ 2026-10-18): payload up to JOB_BIG,
 * reply sized by job_out_cap, both freed after the reply has gone out. A job the heap cannot hold is answered
 * ST_TOOBIG (the client falls back). The TRAM relay keeps its static JOB_MAX buffers.
 * llvm-t800 port: host link 0 (LIN(0), LOUT(0)), TRAM 1 link 1 (LIN(1), LOUT(1)). */
#include <channel.h>
#include <process.h>
#include <stddef.h>
#include <stdlib.h>
#include "jobnode.h"
#include "jobexec.h"

void jobheap_begin(void), jobheap_end(void);   /* jobheap.c: free each job's leftover allocations (FR-7) */
#include "../jobwire.h"

#define LOUT(k) ((Channel *)(0x80000000u + 4u * (unsigned)(k)))
#define LIN(k)  ((Channel *)(0x80000010u + 4u * (unsigned)(k)))

#ifndef SOLO
#include "worker_imgs.h"
#endif

static unsigned char hdr0[JOB_HDR], *in0, *rep0, err0[LREP_HDR];   /* in0/rep0: heap, one node-0 job at a time */
static unsigned char hdrf[JOB_HDR], fwd[JOB_MAX], repm[LREP_HDR + JOB_MAX];

static void drain(Channel *c, long n)
{
    unsigned char sink[64];
    long i;
    for (i = 0; i < n; i += 64) ChanIn(c, sink, (int)(n - i > 64 ? 64 : n - i));
}

static void work0(Channel *go, Channel *done)
{
    long n, len, cap;
    for (;;) {
        n = (long)ChanInInt(go);
        cap = n >= 0 ? job_out_cap(hdr0[2], hdr0[H_LEVEL], in0, n) : 0;
        rep0 = n >= 0 ? malloc((size_t)(LREP_HDR + cap)) : NULL;
        jobheap_begin();                     /* the kernel's own allocations from here are the job's */
        if (rep0) len = job_reply(hdr0, in0, n, rep0, 0, cap);
        else { rep0 = err0; len = job_reply(hdr0, in0, -1, err0, 0, 0); }   /* ST_TOOBIG */
        jobheap_end();
        free(in0); in0 = NULL;
        ChanOutInt(done, (int)len);
    }
    ProcStop();
}

static void mux(Channel *done0, Channel *t1i, Channel *hout, Channel *freec)
{
    long len;
    int node;
    for (;;) {
        if (!t1i || ProcAlt(done0, t1i, NULL) == 0) {     /* SOLO image: no TRAM link, only the local worker */
            len = (long)ChanInInt(done0);
            ChanOut(hout, rep0, (int)len);
            if (rep0 != err0) free(rep0);
            rep0 = NULL;
            node = 0;
        } else {
            len = rep_read(t1i, repm);
            ChanOut(hout, repm, (int)len);
            node = repm[R_NODE];
        }
        ChanOutInt(freec, node);
    }
    ProcStop();
}

static void host(Channel *hin, Channel *t1o, Channel *go0, Channel *freec)
{
    int busy[NODES] = { 0, 0, 0 }, node, k;
    unsigned char h[JOB_HDR];
    long n;
    for (;;) {
        if (ProcAlt(freec, hin, NULL) == 0) { busy[ChanInInt(freec)] = 0; continue; }
        ChanIn(hin, h, JOB_HDR);
        node = h[3];
        if (node == NODE_ANY || node >= NODES) {
            for (;;) {                       /* first idle node, fastest first; wait for one if none */
                for (k = 0; k < NODES && busy[k]; k++) ;
                if (k < NODES) break;
                busy[ChanInInt(freec)] = 0;
            }
            node = k;
        } else {
            while (busy[node]) busy[ChanInInt(freec)] = 0;
        }
        h[3] = (unsigned char)node;
        busy[node] = 1;
        n = (long)g4(h + H_LEN);
        if (node == 0) {
            for (k = 0; k < JOB_HDR; k++) hdr0[k] = h[k];
            in0 = n >= 0 && n <= JOB_BIG ? malloc((size_t)(n > 0 ? n : 1)) : NULL;
            if (!in0) { if (n > 0) drain(hin, n); n = -1; }
            else if (n > 0) ChanIn(hin, in0, (int)n);
            ChanOutInt(go0, (int)n);
        } else if (t1o && n <= JOB_MAX) {
            for (k = 0; k < JOB_HDR; k++) hdrf[k] = h[k];
            if (n > 0) ChanIn(hin, fwd, (int)n);
            ChanOut(t1o, hdrf, JOB_HDR);
            if (n > 0) ChanOut(t1o, fwd, (int)n);
        } else {                                   /* no such node, or too big for a TRAM: drain; ttjobd never sends these */
            if (n > 0) drain(hin, n);
            busy[node] = 0;
        }
    }
    ProcStop();
}

int main(void)
{
    Channel *hin = LIN(0), *hout = LOUT(0);
#ifdef SOLO                                               /* jobllvm0.btl: T425 only (FR-8 fallback) */
    Channel *t1o = NULL, *t1i = NULL;
#else
    Channel *t1o = LOUT(1), *t1i = LIN(1);
    ChanOut(LOUT(1), worker1_btl, (int)worker1_btl_len);
    {
        int sz2 = (int)worker2_btl_len;
        ChanOut(LOUT(1), &sz2, 4);
        ChanOut(LOUT(1), worker2_btl, sz2);
    }
#endif

    static const char ready[4] = { 'R', 'D', 'Y', '0' };
    ChanOut(hout, ready, 4);

    static Channel go0, done0, freec;
    ChanInit(&go0);
    ChanInit(&done0);
    ChanInit(&freec);

    Process *ph = ProcAlloc(host, 8192, 4, hin, t1o, &go0, &freec);
    Process *pw = ProcAlloc(work0, 65536, 2, &go0, &done0);          /* zlib/bzip2/crypto run on this stack */
    Process *pm = ProcAlloc(mux, 8192, 4, &done0, t1i, hout, &freec);
    if (!ph || !pw || !pm) for (;;) ;                    /* out of memory: no RDY reply follows, ttjobd times out */
    /* llvm-t800's ProcPar returns at once instead of waiting (friction F13), and main returning ends in exit():
     * start each process and park main for good. */
    ProcRun(ph); ProcRun(pw); ProcRun(pm);
    ProcStop();
    return 0;
}
