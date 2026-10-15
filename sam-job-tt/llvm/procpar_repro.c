#include <channel.h>
#include <process.h>
#include <stdio.h>
static Channel a, b;
static void prod(Channel *out, int n) { for (int i = 0; i < n; i++) ChanOutInt(out, i); ProcStop(); }
static void relay(Channel *in, Channel *out) { for (;;) ChanOutInt(out, ChanInInt(in) * 10); }
static void cons(Channel *in, int n) { for (int i = 0; i < n; i++) printf("got %d\n", ChanInInt(in)); fflush(stdout); ProcStop(); }
int main(void)
{
    ChanInit(&a); ChanInit(&b);
    Process *p1 = ProcAlloc(prod, 8192, 2, &a, 3), *p2 = ProcAlloc(relay, 8192, 2, &a, &b), *p3 = ProcAlloc(cons, 8192, 2, &b, 3);
    printf("alloc %p %p %p\n", (void *)p1, (void *)p2, (void *)p3); fflush(stdout);
    ProcPar(p1, p2, p3, NULL);
    printf("ProcPar RETURNED (relay never ends, so this should not print)\n");
    return 0;
}
