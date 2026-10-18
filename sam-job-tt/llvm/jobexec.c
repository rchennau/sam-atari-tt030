/* jobexec — see jobexec.h. */
#include "jobexec.h"
#include <stdlib.h>
#ifdef JOB_PNG
long png_kernel(int op, int level, const unsigned char *in, long n, unsigned char *out, long cap);
long png_out_cap(int op, int level, const unsigned char *in, long n);
#endif
#ifdef JOB_PPMQ
long ppmq_kernel(int op, int level, const unsigned char *in, long n, unsigned char *out, long cap);
#endif
#ifdef JOB_JPEG
long jpeg_kernel(int op, int level, const unsigned char *in, long n, unsigned char *out, long cap);
long jpeg_out_cap(int op, int level, const unsigned char *in, long n);
#endif
#ifdef JOB_BC
long bc_job(int op, int level, const unsigned char *in, long n, unsigned char *out, long cap);
long bc_out_cap(int op, int level, const unsigned char *in, long n);
#endif
#ifdef JOB_LHA
long lha_kernel(int op, int level, const unsigned char *in, long n, unsigned char *out, long cap);
long lha_out_cap(int op, int level, const unsigned char *in, long n);
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
#ifdef JOB_PNG
    case 'p': {                              /* tpnmtopng (pngserv's kernel; ttjob-ports FR-3) */
        long olen = png_kernel(op, level, in, n, out, cap);
        return olen < 0 ? -ST_FAILED : olen;
    }
#endif
#ifdef JOB_PPMQ
    case 'q': {                              /* tppmquant (ppmqserv's kernel; ttjob-ports FR-3) */
        long olen = ppmq_kernel(op, level, in, n, out, cap);
        return olen < 0 ? -ST_FAILED : olen;
    }
#endif
#ifdef JOB_BC
    case 'c': {                              /* GNU bc, level 1 = -l (bcserv's kernel, globals reset per job; ttjob-ports FR-3) */
        long olen = bc_job(op, level, in, n, out, cap);
        return olen < 0 ? -ST_FAILED : olen;
    }
#endif
#ifdef JOB_ROOT
    case OP_CAPS: {                          /* FR-7 / NFR-4: payload cap, then the runtime malloc's own high-water, the
                                                heap bounds, and the leftover bytes jobheap.c freed (last job, all
                                                jobs): 6 x 4 bytes LE. No probing
                                                by allocation (a broken bound would make the probe itself overrun). */
        extern unsigned long __malloc_hiwater, jobheap_last, jobheap_total;   /* jobheap.c: leftovers freed */
        extern char *__t800_heap_start(void), *__t800_heap_end(void);
        unsigned long v[6];
        v[0] = JOB_BIG; v[1] = __malloc_hiwater;
        v[2] = (unsigned long)__t800_heap_start(); v[3] = (unsigned long)__t800_heap_end();
        v[4] = jobheap_last; v[5] = jobheap_total;
        if (cap < 24) return -ST_FAILED;
        for (i = 0; i < 24; i++) out[i] = (unsigned char)(v[i / 4] >> (8 * (i % 4)));
        return 24;
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

long job_out_cap(int op, int level, const unsigned char *in, long n)
{
    long c;
    switch (op) {
    case OP_SHA256: case OP_X25519: c = 32; break;
    case OP_ED25519V: c = 1; break;
    case OP_CAPS: c = 24; break;
    case OP_ECHO: case OP_CHACHA20: c = n; break;
    case 'z': case 'g': case 'r': case 'b': c = n + n / 8 + 1024; break;   /* deflate/bzip2 worst case is well inside */
#ifdef JOB_LHA
    case '5': case '6': case 'd': c = lha_out_cap(op, level, in, n); break;
#endif
#ifdef JOB_JPEG
    case 'J': c = jpeg_out_cap('C', level, in, n); break;   /* jpegserv's own letters: 'C' encode, 'D' decode */
    case 'j': c = jpeg_out_cap('D', level, in, n); break;
#endif
#ifdef JOB_PNG
    case 'p': c = png_out_cap(op, level, in, n); break;
#endif
#ifdef JOB_PPMQ
    case 'q': c = n; break;                  /* in place in ppmqserv: the quantized PPM is no larger than the input */
#endif
#ifdef JOB_BC
    case 'c': c = bc_out_cap(op, level, in, n); break;
#endif
    default: c = JOB_MAX; break;             /* 'i' inflate and anything else: the size it always had */
    }
    if (c < 64) c = 64;
    return c > JOB_BIG ? JOB_BIG : c;
}
