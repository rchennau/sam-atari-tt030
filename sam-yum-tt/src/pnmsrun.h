/* pnmsrun — `pnmscale -xsize/-ysize/-xysize` through pmshim's ps_run (pnmsserv.c, tpnmscale.c,
 * pnmst4test.c). Only the integer-size modes: pnmscale's float use there is one int/int division per axis
 * (the TT side is built -ffloat-store so both CPUs round it to single precision). -xscale/-yscale/-pixels
 * go through sscanf("%f") and sqrt, so they stay with stock pnmscale.
 *   mode 0: -xsize x and/or -ysize y (0 = not given)     mode 1: -xysize x y */
#ifndef PNMSRUN_H
#define PNMSRUN_H
#include <stdio.h>
#include "pmshim.h"
extern int pnmscale_main(int argc, char *argv[]);

/* The largest output pnmscale can write for this request (header included), or -1 if the input is not a
 * raw P5/P6 or the request is malformed. pnmscale rounds each derived size up by 0.999 of a pixel. */
static long pn_cap(int mode, long x, long y, const unsigned char *in, long n)
{
    long v[3], pos = 2, k;
    if (n < 11 || in[0] != 'P' || (in[1] != '5' && in[1] != '6') || x < 0 || y < 0 || x > 32767 || y > 32767
        || (x == 0 && y == 0) || (mode == 1 && (x == 0 || y == 0)))
        return -1;
    for (k = 0; k < 3; k++) {
        while (pos < n && (in[pos] == ' ' || in[pos] == '\t' || in[pos] == '\n' || in[pos] == '\r'))
            pos++;
        if (pos >= n || in[pos] < '0' || in[pos] > '9')
            return -1;
        for (v[k] = 0; pos < n && in[pos] >= '0' && in[pos] <= '9' && v[k] < 100000L; pos++)
            v[k] = v[k] * 10 + (in[pos] - '0');
    }
    if (v[0] < 1 || v[1] < 1)
        return -1;
    if (mode == 1)
        x++, y++;
    else if (x == 0)
        x = v[0] * y / v[1] + 2;
    else if (y == 0)
        y = v[1] * x / v[0] + 2;
    return 64 + x * y * (in[1] == '6' ? 3 : 1);
}

static long pn_run(int mode, long x, long y, const unsigned char *in, long n, unsigned char *out, long cap)
{
    char xs[16], ys[16], *argv[8];
    int argc = 0;
    argv[argc++] = "pnmscale";
    if (mode == 1) {
        argv[argc++] = "-xysize";
        sprintf(xs, "%ld", x);
        argv[argc++] = xs;
        sprintf(ys, "%ld", y);
        argv[argc++] = ys;
    } else {
        if (x) {
            argv[argc++] = "-xsize";
            sprintf(xs, "%ld", x);
            argv[argc++] = xs;
        }
        if (y) {
            argv[argc++] = "-ysize";
            sprintf(ys, "%ld", y);
            argv[argc++] = ys;
        }
    }
    argv[argc] = NULL;
    return ps_run(pnmscale_main, argc, argv, in, n, out, cap);
}
#endif
