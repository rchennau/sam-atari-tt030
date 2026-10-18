/* jobexec — see jobexec.h. */
#include "jobexec.h"
#ifdef JOB_JPEG
long jpeg_kernel(int op, int level, const unsigned char *in, long n, unsigned char *out, long cap);
#endif
#ifdef JOB_LHA
long lha_kernel(int op, int level, const unsigned char *in, long n, unsigned char *out, long cap);
#endif
#include "../jobwire.h"
#include "sha256.h"
#include "chacha20.h"
#include "c25519.h"
#include "edsign.h"
#ifdef JOB_COMPRESS
#include "compkern.h"
#endif

long job_exec(int op, int level, const unsigned char *in, long n, unsigned char *out, long cap)
{
    long i;
    struct sha256 s;
    switch (op) {
    case OP_ECHO:
        if (n > cap) return -ST_FAILED;
        for (i = 0; i < n; i++) out[i] = in[i];
        return n;
    case OP_SHA256:
        if (cap < 32) return -ST_FAILED;
        sha256_init(&s); sha256_update(&s, in, (unsigned long)n); sha256_final(&s, out);
        return 32;
    case OP_X25519: {                       /* scalar (32) || u (32) -> X25519(scalar, u); as xserv2 */
        unsigned char e[32];
        if (n != 64 || cap < 32) return -ST_FAILED;
        for (i = 0; i < 32; i++) e[i] = in[i];
        e[0] &= 248; e[31] &= 127; e[31] |= 64;
        c25519_smult(out, in + 32, e);
        f25519_normalize(out);
        return 32;
    }
    case OP_ED25519V:                       /* sig (64) || pub (32) || msg -> 1 byte: 1 valid, 0 invalid */
        if (n < 96 || cap < 1) return -ST_FAILED;
        out[0] = (unsigned char)(edsign_verify(in, in + 64, in + 96, (size_t)(n - 96)) ? 1 : 0);
        return 1;
    case OP_CHACHA20:                       /* key (32) || nonce (12) || counter (4, LE) || data -> data ^ keystream */
        if (n < 48 || cap < n - 48) return -ST_FAILED;
        chacha20_xor(out, in + 48, n - 48, in, in + 32,
                     in[44] | ((unsigned long)in[45] << 8) | ((unsigned long)in[46] << 16) | ((unsigned long)in[47] << 24));
        return n - 48;
#ifdef JOB_COMPRESS
    case 'z': case 'g': case 'r': case 'i': case 'b': {
        long olen = ck_compress(op, level, in, n, out, cap);
        return olen < 0 ? -ST_FAILED : olen;
    }
#endif
#ifdef JOB_LHA
    case '5': case '6': case 'd': {          /* lha -lh5-/-lh6- encode, 'd' decode (lhaserv's kernel; ttjob-ports FR-3) */
        long olen = lha_kernel(op, level, in, n, out, cap);
        return olen < 0 ? -ST_FAILED : olen;
    }
#endif
#ifdef JOB_JPEG
    case 'J': case 'j': {                    /* libjpeg encode / decode (jpegserv's kernel; ttjob-ports FR-3) */
        long olen = jpeg_kernel(op, level, in, n, out, cap);
        return olen < 0 ? -ST_FAILED : olen;
    }
#endif
#ifdef JOB_TEST
    case 'H':                               /* test image only: never returns, to exercise ttjobd's watchdog (FR-7) */
        for (;;) ;
#endif
    default:
        return -ST_BADOP;
    }
}
