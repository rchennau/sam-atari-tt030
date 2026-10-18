/* jobheap — node 0's heap returns to its pre-job state after every job (ttjob-ports FR-7, NFR-5).
 * The kernels came from one-request-per-boot servers and do not all free what they allocate; in a resident image
 * those leftovers pin the freed space into pieces, and the next large job finds no block big enough (TT
 * 2026-10-18: zip and png refused after the lha jobs). The root image links with --wrap=malloc,free,realloc,calloc:
 * every block carries a header on one list, marked as a job's while jobheap_begin()..jobheap_end() brackets it,
 * and jobheap_end() frees the job's leftovers and counts them (OP_CAPS reports the counts). */
#include <stddef.h>
#include <string.h>

void *__real_malloc(size_t n);
void __real_free(void *p);

struct hb { struct hb *next, *prev; size_t n; long job; };   /* 16 B: keeps the caller's block 8-aligned */
static struct hb live = { &live, &live, 0, 0 };
static int on;
unsigned long jobheap_last, jobheap_total;                    /* leftover bytes freed: last job, all jobs */

void *__wrap_malloc(size_t n)
{
    struct hb *b = __real_malloc(sizeof *b + n);
    if (!b)
        return NULL;
    b->n = n; b->job = on;
    b->next = live.next; b->prev = &live;
    live.next->prev = b; live.next = b;
    return b + 1;
}

void __wrap_free(void *p)
{
    struct hb *b;
    if (!p)
        return;
    b = (struct hb *)p - 1;
    b->prev->next = b->next; b->next->prev = b->prev;
    __real_free(b);
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
