/* t4meter — EMULATOR-ONLY stand-in for t4serv.c that measures a kernel's peak memory (PMS tt030-ttjob-ports FR-1,
 * NFR-4). Link it instead of t4serv.c with any family's t4_kernel()/t4_out_cap(); run under t800-run (t4 emulator).
 * Reads "meter.in" (one line: op level payload-path [output-path]), allocates exactly as t4serv.c does (input buffer, then the
 * output cap or in place), runs the kernel once and prints:
 *   meter op=.. in=N out=N adler=X heap_peak=N stack_peak=N stack_reserve=N
 * heap_peak: the runtime malloc's own high-water (__malloc_hiwater) above _heap_start, so it includes the payload
 * buffers and the kernel's own allocations. stack_peak: the workspace reservation (_heap_end .. __stack) is painted
 * with a pattern first and scanned after; deepest overwritten word = peak depth.
 * NEVER run this on the real T425: painting board memory once blanked the ATW800/2 display (t4serv.c, 2026-09-25).
 * It also uses host stdio, which only the emulator answers.
 */
#include <stdio.h>
#include <stdlib.h>
#include "t4serv.h"
#include "t4adler.h"

extern char _heap_start[], _heap_end[], __stack[];
extern unsigned long __malloc_hiwater;    /* type undocumented: printed raw too, interpreted against _heap_start */
#define PAINT 0xA5A5A5A5UL

int main(void)
{
    char ops[8], path[256], save[256];
    int level;
    unsigned long *w, *top, here = 0;
    unsigned char *in, *out;
    long n, cap, olen, i;
    FILE *f = fopen("meter.in", "r");
    save[0] = 0;
    if (!f || fscanf(f, "%7s %d %255s %255s", ops, &level, path, save) < 3) { printf("meter: bad meter.in\n"); return 1; }
    fclose(f);
    /* paint the workspace reservation below the current frame (the live frames above it stay untouched) */
    top = (unsigned long *)((unsigned long)&here & ~3UL) - 64;
    for (w = (unsigned long *)_heap_end + 1; w < top; w++)   /* + 1: _heap_end holds the runtime's stack canary */
        *w = PAINT;
    if (!(f = fopen(path, "rb"))) { printf("meter: cannot open %s\n", path); return 1; }
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    in = malloc(n ? n : 1);
    if (!in || (long)fread(in, 1, n, f) != n) { printf("meter: read failed\n"); return 1; }
    fclose(f);
    cap = t4_out_cap(ops[0], level, in, n);
    if (cap == T4_IN_PLACE) { out = in; cap = n; } else out = cap > 0 ? malloc((size_t)cap) : NULL;
    olen = out ? t4_kernel(ops[0], level, in, n, out, cap) : -1;
    for (w = (unsigned long *)_heap_end + 1; w < top && *w == PAINT; w++)
        ;
    printf("meter op=%c level=%d in=%ld out=%ld adler=%08lx hiwater_raw=%08lx heap_start=%08lx stack_peak=%ld stack_reserve=%ld\n",
        ops[0], level, n, olen, olen > 0 ? t4_adler32(out, olen) : 0UL, __malloc_hiwater,
        (unsigned long)_heap_start, (long)(__stack - (char *)w), (long)(__stack - _heap_end));
    if (save[0] && olen > 0 && (f = fopen(save, "wb"))) {   /* optional 4th field: keep the output */
        fwrite(out, 1, olen, f);
        fclose(f);
    }
    (void)i;
    return olen < 0;
}
