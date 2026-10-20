/* t4adler — adler32 for both ends of the T425 link (tt030-t425-kernel-ports), so neither a package
 * nor a family server needs zlib just to check a result. Equal to zlib's adler32(1, p, n). */
#include "t4adler.h"

unsigned long t4_adler32(const unsigned char *p, long n)
{
    unsigned long a = 1, b = 0;
    long k;

    while (n > 0) {
        k = n < 5552 ? n : 5552;                 /* largest block before b can overflow 32 bits */
        n -= k;
        /* 16x unrolled for the 68030 (ttjob-ports NFR-2: 0.78 ms/KB measured 2026-10-20) */
        while (k >= 16) {
            a += *p++; b += a;
            a += *p++; b += a;
            a += *p++; b += a;
            a += *p++; b += a;
            a += *p++; b += a;
            a += *p++; b += a;
            a += *p++; b += a;
            a += *p++; b += a;
            a += *p++; b += a;
            a += *p++; b += a;
            a += *p++; b += a;
            a += *p++; b += a;
            a += *p++; b += a;
            a += *p++; b += a;
            a += *p++; b += a;
            a += *p++; b += a;
            k -= 16;
        }
        while (k-- > 0) {
            a += *p++;
            b += a;
        }
        a %= 65521UL;
        b %= 65521UL;
    }
    return (b << 16) | a;
}
