/* jobheap — node 0's heap returns to its pre-job state after every job (ttjob-ports FR-7, NFR-5).
 * The kernels came from one-request-per-boot servers and do not all free what they allocate; in a resident image
 * those leftovers pin the freed space into pieces, and the next large job finds no block big enough (TT
 * 2026-10-18: zip and png refused after the lha jobs). The root image links with --wrap=malloc,free,realloc,calloc:
 * every block carries a header on one list, marked as a job's while jobheap_begin()..jobheap_end() brackets it,
 * and jobheap_end() frees the job's leftovers and counts them (OP_CAPS reports the counts).
 * Defect 393e4dcc: libt800's K&R malloc carves each block from the TAIL of the free block it picks and grows the
 * heap when none fits, so after a small job (lha, echo) a 1.44 MB job split around the old high-water and ran out
 * of heap (TT 2026-10-22: lha then cjpeg refused; cjpeg, lha, cjpeg ran). The blocks now come from this allocator
 * over [_heap_start, _heap_end): free list in address order, first fit, carved from the HEAD, neighbours coalesced
 * on free. Once a job's blocks are freed the heap is the same single free block every time.
 * ponytail: first fit walks the free list (as K&R does); a size-binned list if a kernel's small allocs get slow. */
#include <stddef.h>
#include <string.h>

extern char *__t800_heap_start(void), *__t800_heap_end(void);
extern unsigned long __malloc_hiwater;                        /* libt800's, still read by OP_CAPS (jobexec.c) */

struct hb { struct hb *next, *prev; size_t n; long job; };   /* 16 B: keeps the caller's block 8-aligned */
struct fb { struct fb *next; size_t size; };                  /* a free block (size includes its header) */
static struct hb live = { &live, &live, 0, 0 };
static struct fb *fl;
static char *base;
static int on;
unsigned long jobheap_last, jobheap_total;                    /* leftover bytes freed: last job, all jobs */

void *__wrap_malloc(size_t n)
{
    struct fb **pp, *f;
    struct hb *b;
    size_t sz;
    if (!base) {                                              /* first call: the whole heap is one free block */
        base = (char *)(((unsigned long)__t800_heap_start() + 7) & ~7UL);
        fl = (struct fb *)base; fl->next = NULL; fl->size = (size_t)(__t800_heap_end() - base) & ~(size_t)7;
    }
    if (n > (size_t)(__t800_heap_end() - base))
        return NULL;
    sz = (sizeof *b + n + 7) & ~(size_t)7;
    for (pp = &fl; (f = *pp) && f->size < sz; pp = &f->next) ;
    if (!f)
        return NULL;
    if (f->size - sz >= sizeof *b + 8) {                      /* split: the rest stays free, above the block */
        struct fb *r = (struct fb *)((char *)f + sz);
        r->next = f->next; r->size = f->size - sz; *pp = r;
    } else {
        sz = f->size; *pp = f->next;
    }
    if ((unsigned long)((char *)f + sz - base) > __malloc_hiwater)
        __malloc_hiwater = (unsigned long)((char *)f + sz - base);
    b = (struct hb *)f;
    b->n = sz - sizeof *b; b->job = on;                       /* n = the block's capacity (>= the request) */
    b->next = live.next; b->prev = &live;
    live.next->prev = b; live.next = b;
    return b + 1;
}

void __wrap_free(void *p)
{
    struct hb *b;
    struct fb **pp, *f, *prev = NULL;
    if (!p)
        return;
    b = (struct hb *)p - 1;
    b->prev->next = b->next; b->next->prev = b->prev;
    f = (struct fb *)b;
    f->size = b->n + sizeof *b;
    for (pp = &fl; *pp && *pp < f; pp = &(*pp)->next)
        prev = *pp;
    f->next = *pp;
    if (f->next && (char *)f + f->size == (char *)f->next) {  /* merge with the free block above */
        f->size += f->next->size; f->next = f->next->next;
    }
    if (prev && (char *)prev + prev->size == (char *)f) {     /* and with the one below */
        prev->size += f->size; prev->next = f->next;
    } else
        *pp = f;
}

void *__wrap_realloc(void *p, size_t n)
{
    void *q;
    size_t old;
    if (!p)
        return __wrap_malloc(n);
    if (!(q = __wrap_malloc(n)))
        return NULL;
    old = ((struct hb *)p - 1)->n;
    memcpy(q, p, n < old ? n : old);
    __wrap_free(p);
    return q;
}

void *__wrap_calloc(size_t k, size_t n)
{
    void *p = __wrap_malloc(k * n);
    if (p)
        memset(p, 0, k * n);
    return p;
}

void jobheap_begin(void)
{
    on = 1;
}

void jobheap_end(void)
{
    struct hb *b, *nx;
    on = 0;
    jobheap_last = 0;
    for (b = live.next; b != &live; b = nx) {
        nx = b->next;
        if (b->job) {
            jobheap_last += b->n;
            __wrap_free(b + 1);
        }
    }
    jobheap_total += jobheap_last;
}
