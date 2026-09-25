/* tbc — GNU bc 1.05 for scripts, with the work on the ATW800/2 T425 (tt030-t425-kernel-ports W3), and the
 * 68030 fallback running the SAME bc (its sources through bcshim.c), so the output is identical either way.
 * A batch twin of bc, not a replacement: the files, then stdin (skipped when it is a terminal and files were
 * given), are read in full and run as one program; nothing prints until it ends. Interactive use and -i /
 * -s / -w / -c stay with stock bc; BC_ENV_ARGS / BC_LINE_LENGTH / POSIXLY_CORRECT are not honoured.
 *   tbc [-v] [-l] [file...]      stdout and stderr as bc writes them; bc's exit code
 * With /etc/t425/enabled/tbc every run goes to the T425 (a run's cost cannot be known in advance; the ~1 s
 * boot is the price for a trivial one, perf.md); otherwise, or on any failure, the 68030 runs it. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "bcshim.h"
#include "t4call.h"

#define TBC_BTL "/usr/lib/t425/tbc.btl"
#define TBC_OUT (1024L * 1024)                   /* result cap on both CPUs (bcserv.c BCS_OUT) */

static int slurp(FILE *f, char **b, long *n, long *cap)
{
    long r;
    for (;;) {
        if (*n == *cap) {
            char *nb = (char *)realloc(*b, (size_t)(*cap *= 2));
            if (!nb)
                return -1;
            *b = nb;
        }
        if ((r = (long)fread(*b + *n, 1, (size_t)(*cap - *n), f)) <= 0)
            return ferror(f) ? -1 : 0;
        *n += r;
    }
}

int main(int argc, char **argv)
{
    int verbose = 0, mathlib = 0, i = 1;
    long n = 0, cap = 65536, len = -1, olen;
    char *prog = (char *)malloc((size_t)cap);
    unsigned char *out = (unsigned char *)malloc(TBC_OUT);
    const char *why = "not enabled";

    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "-v"))
            verbose = 1;
        else if (!strcmp(argv[i], "-l"))
            mathlib = 1;
        else {
            fprintf(stderr, "usage: tbc [-v] [-l] [file...]   (interactive use, -i -s -w -c: use bc)\n");
            return 2;
        }
    }
    if (!prog || !out) {
        fprintf(stderr, "tbc: out of memory\n");
        return 1;
    }
    for (; i < argc; i++) {
        FILE *f = fopen(argv[i], "r");
        if (!f || slurp(f, &prog, &n, &cap) < 0) {
            fprintf(stderr, "File %s is unavailable.\n", argv[i]);
            return 1;
        }
        fclose(f);
    }
    if ((argc == 1 || !isatty(0) || n == 0) && slurp(stdin, &prog, &n, &cap) < 0) {
        fprintf(stderr, "tbc: cannot read stdin\n");
        return 1;
    }
    t4call_timeout_ms = 6L * 3600 * 1000;        /* a bc run can take hours; a T425 hang falls back after */
    if (t4_enabled("tbc")) {
        len = t4call(TBC_BTL, 'c', mathlib, prog, n, out, TBC_OUT, &why);
        if (len < 5)
            fprintf(stderr, "tbc: T425 %s; running on the 68030\n", why);
        else if (verbose)
            fprintf(stderr, "tbc: ran on the T425\n");
    }
    if (len < 5 && (len = bs_run(mathlib, prog, n, out, TBC_OUT)) < 5) {
        fprintf(stderr, "tbc: output over %ld bytes; use bc\n", TBC_OUT);
        return 1;
    }
    olen = out[0] | ((long)out[1] << 8) | ((long)out[2] << 16) | ((long)out[3] << 24);
    fwrite(out + 5, 1, (size_t)olen, stdout);
    fflush(stdout);
    fwrite(out + 5 + olen, 1, (size_t)(len - 5 - olen), stderr);
    return out[4];
}
