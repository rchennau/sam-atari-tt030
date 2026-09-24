/* compkern — one compression call, the same C on the 68030, the T425 and the host (Rev. 7 benchmark). */
#ifndef COMPKERN_H
#define COMPKERN_H
/* op: 'z' deflate level 1, 'Z' deflate level 6 (gzip's default), 'b' bzip2 -1 (100 KB blocks).
 * Returns the compressed length, or -1 (unknown op / library error / out too small). */
long ck_compress(int op, const unsigned char *in, long n, unsigned char *out, long cap);
#endif
