/* heap_repro — defect 393e4dcc: root.c's node-0 malloc/free sequence (host -> work0 -> mux) without links, on a heap
 * the root's size (heap_repro.sh). SEQ 1: J only (fresh daemon); 2: E 384K, then J; 3: lha '5' 256K x3, then J;
 * 4: ppmquant 'q' 256 colours (in place, as root.c), png 'p', bc 'c', then J; each job's time in ms (64 us ticks). */
#include <stdio.h>
#include <stdlib.h>
#include "jobwire.h"
#include "jobnode.h"
#include "jobexec.h"
#include "t4adler.h"
#include <process.h>

#define QGAP(n) (LREP_HDR + 64 + (((n) / 3 + 8) & ~7L))   /* root.c's */

void jobheap_begin(void), jobheap_end(void);
extern unsigned long __malloc_hiwater;
extern char *__t800_heap_start(void), *__t800_heap_end(void);

static unsigned char err0[LREP_HDR];
static unsigned long seed = 12345;
static unsigned char srcbuf[4 + 15 + 800L * 600 * 3];   /* off-heap, as on the TT; -mmem grows by its size */

static int job(int op, int level, unsigned char *src, long n)
{
    unsigned char h[JOB_HDR];
    unsigned char *in0, *rep0;
    long cap, len;
    int st;
    h[0] = 1; h[1] = 0; h[2] = (unsigned char)op; h[3] = 0; h[H_LEVEL] = (unsigned char)level;
    p4(h + H_LEN, (unsigned long)n); p4(h + H_ADLER, t4_adler32(src, n));
    int ip = op == 'q';                                          /* in place, as root.c (QGAP) */
    long k = ip ? QGAP(n) : 0, t0 = ProcTime();
    in0 = malloc((size_t)(k + n));                               /* host */
    if (in0) { long i; in0 += k; for (i = 0; i < n; i++) in0[i] = src[i]; }
    cap = !in0 ? 0 : ip ? n : job_out_cap(op, level, in0, n);   /* work0 */
    rep0 = !in0 ? NULL : ip ? in0 - k : malloc((size_t)(LREP_HDR + cap));
    jobheap_begin();
    if (rep0) len = job_reply(h, in0, n, rep0, 0, cap);
    else { rep0 = err0; len = job_reply(h, in0, -1, err0, 0, 0); }
    jobheap_end();
    st = rep0[2];
    printf("op %c n %ld: in0 %s rep0 %s cap %ld -> status %d out %ld hiwater %lu ms %ld\n", op, n, in0 ? "ok" : "NULL",
           rep0 != err0 ? "ok" : "NULL", cap, st, len - LREP_HDR, __malloc_hiwater, ProcTimeMinus(ProcTime(), t0) * 64L / 1000);
    if (!ip) free(in0);
    if (rep0 != err0) free(rep0);                                /* mux */
    return st;
}

static unsigned char *ppm(long *n)
{
    static const unsigned char hd[9] = { 0x20, 3, 0, 0, 0x58, 2, 0, 0, 3 };   /* jpegserv 'C': w, h (LE), comps */
    long i, k = sizeof hd;
    unsigned char *p;
    *n = k + 800L * 600 * 3;
    p = srcbuf;
    for (i = 0; i < k; i++) p[i] = (unsigned char)hd[i];
    for (; i < *n; i++) { seed = seed * 1103515245 + 12345; p[i] = (unsigned char)(((i / 2400) & 0xff) ^ ((seed >> 16) & 0x1f)); }
    return p;
}

static void seq(Process *self)
{
    long n, k;
    unsigned char *src;
    (void)self;
    printf("heap %p..%p (%ld B), SEQ %d\n", __t800_heap_start(), __t800_heap_end(),
           (long)(__t800_heap_end() - __t800_heap_start()), SEQ);
    src = ppm(&n);
    if (SEQ == 2) job(OP_ECHO, 0, src, 384L * 1024);
    if (SEQ == 4) {
        static const unsigned char p6[] = "P6\n800 600\n255\n";
        long i, m = 800L * 600 * 3;
        for (i = m - 1; i >= 0; i--) src[19 + i] = src[9 + i];  /* 'q': colours (4) || P6 file */
        src[0] = 0; src[1] = 1; src[2] = 0; src[3] = 0;
        for (i = 0; i < 15; i++) src[4 + i] = p6[i];
        job('q', 0, src, 19 + m);
        job('p', 6, src + 4, 15 + m);
        job('c', 0, (unsigned char *)"2^20000\n", 8);
        for (i = 0; i < m; i++) src[9 + i] = src[19 + i];        /* back to the 'J' layout */
        src[0] = 0x20; src[1] = 3; src[2] = 0; src[3] = 0; src[4] = 0x58; src[5] = 2; src[6] = 0; src[7] = 0; src[8] = 3;
    }
    if (SEQ == 3) for (k = 0; k < 3; k++) job('5', 0, src + k * 256L * 1024, 256L * 1024);
    printf("RC %d\n", job('J', 0, src, n));
    exit(0);
}

int main(void)
{
    Process *ph = ProcAlloc(seq, 8192, 0), *pw = ProcAlloc(seq, 65536, 0), *pm = ProcAlloc(seq, 8192, 0);
    (void)ph; (void)pm;   /* root.c's host/work0/mux stacks: same heap blocks, same order; the job runs on work0's */
    ProcRun(pw);
    ProcStop();
    return 0;
}
