/* compkern — one compression call, the same C on the 68030, the T425 and the host (Rev. 7 benchmark). */
#ifndef COMPKERN_H
#define COMPKERN_H
/* op 'z': zlib stream, 'g': one gzip member (what gunzip reads; members concatenate), both deflate
 * at level 1-9; op 'b': one bzip2 stream at block size level (1-9 x 100 KB; streams concatenate).
 * Returns the compressed length, or -1 (unknown op / library error / out too small). */
long ck_compress(int op, int level, const unsigned char *in, long n, unsigned char *out, long cap);
#endif
