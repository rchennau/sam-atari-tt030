/* jobexec — see jobexec.h. */
#include "jobexec.h"
#include "../jobwire.h"
#include "sha256.h"
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
#ifdef JOB_COMPRESS
    case 'z': case 'g': case 'r': case 'i': case 'b': {
        long olen = ck_compress(op, level, in, n, out, cap);
        return olen < 0 ? -ST_FAILED : olen;
    }
#endif
    default:
        return -ST_BADOP;
    }
}
