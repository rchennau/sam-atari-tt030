/* ppmqrun — `ppmquant N` through pmshim's ps_run (ppmqserv.c, tppmquant.c, ppmqt4test.c). */
#ifndef PPMQRUN_H
#define PPMQRUN_H
#include <stdio.h>
#include "pmshim.h"
extern int ppmquant_main(int argc, char *argv[]);
static long pq_run(int ncolors, const unsigned char *ppm, long n, unsigned char *out, long cap)
{
    char arg[16], *argv[3];
    sprintf(arg, "%d", ncolors);
    argv[0] = "ppmquant";
    argv[1] = arg;
    argv[2] = NULL;
    return ps_run(ppmquant_main, 2, argv, ppm, n, out, cap);
}
#endif
