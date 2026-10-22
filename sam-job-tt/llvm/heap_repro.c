/* heap_repro — defect 393e4dcc: root.c's node-0 malloc/free sequence (host -> work0 -> mux) without links, on a heap
 * the root's size (heap_repro.sh). SEQ 1: J only (fresh daemon); 2: E 384K, then J; 3: lha '5' 256K x3, then J. */
#include <stdio.h>
#include <stdlib.h>
#include "jobwire.h"
#include "jobnode.h"
#include "jobexec.h"
#include "t4adler.h"
#include <process.h>

void jobheap_begin(void), jobheap_end(void);
extern unsigned long __malloc_hiwater;
extern char *__t800_heap_start(void), *__t800_heap_end(void);

static unsigned char err0[LREP_HDR];
static unsigned long seed = 12345;
static unsigned char srcbuf[9 + 800L * 600 * 3];   /* off-heap, as on the TT; -mmem grows by its size */

static int job(int op, int level, unsigned char *src, long n)
{
    unsigned char h[JOB_HDR];
    unsigned char *in0, *rep0;
    long cap, len;
    int st;
    h[0] = 1; h[1] = 0; h[2] = (unsigned char)op; h[3] = 0; h[H_LEVEL] = (unsigned char)level;
    p4(h + H_LEN, (unsigned long)n); p4(h + H_ADLER, t4_adler32(src, n));
    in0 = malloc((size_t)n);                                     /* host */
    if (in0) { long i; for (i = 0; i < n; i++) in0[i] = src[i]; }
    cap = in0 ? job_out_cap(op, level, in0, n) : 0;              /* work0 */
    rep0 = in0 ? malloc((size_t)(LREP_HDR + cap)) : NULL;
    jobheap_begin();
    if (rep0) len = job_reply(h, in0, n, rep0, 0, cap);
    else { rep0 = err0; len = job_reply(h, in0, -1, err0, 0, 0); }
    jobheap_end();
    st = rep0[2];
    printf("op %c n %ld: in0 %s rep0 %s cap %ld -> status %d out %ld hiwater %lu\n", op, n, in0 ? "ok" : "NULL",
           rep0 != err0 ? "ok" : "NULL", cap, st, len - LREP_HDR, __malloc_hiwater);
    free(in0);
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
