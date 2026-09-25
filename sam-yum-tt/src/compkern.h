/* compkern — one compression call, the same C on the 68030, the T425 and the host (Rev. 7 benchmark). */
#ifndef COMPKERN_H
#define COMPKERN_H
/* op 'z': zlib stream, 'g': one gzip member (what gunzip reads; members concatenate), both deflate
 * at level 1-9; op 'r': raw deflate (window bits -15, memLevel 8, default strategy: zip 2.3's zlib path),
 * result = zlib's data_type (1 byte: 0 binary, 1 text, 2 unknown) || the raw stream; op 'b': one bzip2 stream at block size level (1-9 x 100 KB; streams concatenate).
 * Returns the compressed length, or -1 (unknown op / library error / out too small). */
long ck_compress(int op, int level, const unsigned char *in, long n, unsigned char *out, long cap);
#endif
