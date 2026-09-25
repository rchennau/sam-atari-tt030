/* gifrun — `giftopnm [-image N]` through pmshim's ps_run (gifserv.c, tgiftopnm.c, gift4test.c). giftopnm's
 * one float is the verbose aspect-ratio message, which never runs here (pm_message is quiet). */
#ifndef GIFRUN_H
#define GIFRUN_H
#include <stdio.h>
#include "pmshim.h"
extern int giftopnm_main(int argc, char *argv[]);

/* Pixels of the GIF's logical screen, or -1 if it is not a GIF. The output is sized from the screen: an
 * image larger than its screen fails ("output too small") and is left to stock giftopnm. */
static long gf_pixels(const unsigned char *in, long n)
{
    if (n < 13 || in[0] != 'G' || in[1] != 'I' || in[2] != 'F')
        return -1;
    return (long)(in[6] | (in[7] << 8)) * (long)(in[8] | (in[9] << 8));
}

static long gf_cap(const unsigned char *in, long n)
{
    long p = gf_pixels(in, n);
    return p < 1 ? -1 : 64 + p * 3;
}

static long gf_run(long image, const unsigned char *in, long n, unsigned char *out, long cap)
{
    char is[16], *argv[4];
    int argc = 0;
    argv[argc++] = "giftopnm";
    if (image > 1) {
        argv[argc++] = "-image";
        sprintf(is, "%ld", image);
        argv[argc++] = is;
    }
    argv[argc] = NULL;
    return ps_run(giftopnm_main, argc, argv, in, n, out, cap);
}
#endif
