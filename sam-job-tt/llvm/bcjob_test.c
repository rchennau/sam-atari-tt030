/* bcjob_test — op 'c' in a resident image runs every bc program as if freshly booted (ttjob-ports FR-3).
 * T4 build (bcjob_test.sh): PROGS run back to back through bc_job, then LOOP rounds, then the state-reading
 * program again: a global that survives a job shows up as a diff against the host. HOST build (-DHOST): runs ONE
 * program (argv[1] = index) through bs_run in a fresh process; the script runs it per program as the reference.
 * No heap figure: a largest-block probe hit its own 8 MB search cap on host and t4 alike, so it measured nothing,
 * and the libc keeps its brk file-static. bc.o's only allocator calls are bcshim's tracked malloc/free, and bs_run
 * frees the list after every run. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct { int lib; const char *src; } progs[] = {
    {1, "scale=20; 4*a(1)\n"},
    {0, "define f(n) {\n return (n*2)\n}\nx=5\nf(x)\nobase=16\n255\n"},
    {0, "x; f(3)\n255\n"},                       /* fresh: x is 0, f undefined, obase 10 */
    {0, "2^100\n"},
    {0, "1+\n"},                                 /* parse error on stderr */
    {0, "a[3]=7\na[3]\nquit\n9\n"},            /* quit leaves bc mid-run via exit */
    {1, "scale=50; e(1)\n"},
};
#define NP (int)(sizeof progs / sizeof progs[0])
static unsigned char out[200000];

static void show(int i, long r)
{
    long k;
    printf("prog %d len %ld:", i, r);
    for (k = 0; k < r; k++)
        printf(out[k] >= 32 && out[k] < 127 && out[k] != '\\' ? "%c" : "\\%02x", out[k]);
    printf("\n");
    fflush(stdout);
}

#ifdef HOST
long bs_run(int mathlib, const char *prog, long n, unsigned char *out, long cap);
int main(int argc, char **argv)
{
    int i = atoi(argv[1]);
    show(i, bs_run(progs[i].lib, progs[i].src, (long)strlen(progs[i].src), out, sizeof out));
    return 0;
}
#else
long bc_job(int op, int level, const unsigned char *in, long n, unsigned char *out, long cap);

static long run(int i)
{
    return bc_job('c', progs[i].lib, (const unsigned char *)progs[i].src, (long)strlen(progs[i].src), out, sizeof out);
}

int main(void)
{
    int i, k;
    for (i = 0; i < NP; i++)
        show(i, run(i));
    for (k = 0; k < LOOP; k++)
        for (i = 1; i < NP; i++)
            if (run(i) < 0) { printf("loop %d prog %d FAILED\n", k, i); return 1; }
    show(2, run(2));                             /* still fresh after the loop */
    printf("rounds %d done\n", LOOP);
    return 0;
}
#endif
