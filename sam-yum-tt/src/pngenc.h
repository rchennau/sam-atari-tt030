/* pngenc — see pngenc.c. */
#ifndef PNGENC_H
#define PNGENC_H
/* Worst-case PNG size for a raw P5/P6 file of n bytes, or -1 if it is not one. */
long pe_cap(const unsigned char *pnm, long n);
/* The PNG for a raw P5/P6 file (maxval <= 255) at zlib level 0..9, or -1. */
long pe_encode(const unsigned char *pnm, long n, int level, unsigned char *out, long cap);
#endif
