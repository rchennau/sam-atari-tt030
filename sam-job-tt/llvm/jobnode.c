/* jobnode — run or move one jobwire message. llvm-t800 port. */
#include "jobnode.h"
#include "../jobwire.h"
#include "t4adler.h"
#include "jobexec.h"

unsigned long g4(const unsigned char *b)
{ return b[0] | ((unsigned long)b[1] << 8) | ((unsigned long)b[2] << 16) | ((unsigned long)b[3] << 24); }

void p4(unsigned char *b, unsigned long v)
{ b[0] = (unsigned char)v; b[1] = (unsigned char)(v >> 8); b[2] = (unsigned char)(v >> 16); b[3] = (unsigned char)(v >> 24); }

long job_reply(const unsigned char *hdr, const unsigned char *in, long n, unsigned char *rep, int node, long cap)
{
    unsigned long areq = n >= 0 ? t4_adler32(in, n) : 0;
    long olen = 0;
    int st = ST_OK;
    if (n < 0) st = ST_TOOBIG;
    else if (areq != g4(hdr + H_ADLER)) st = ST_BADREQ;
    else {
        olen = job_exec(hdr[2], hdr[H_LEVEL], in, n, rep + LREP_HDR, cap);
        if (olen < 0) { st = (int)-olen; olen = 0; }
    }
    rep[0] = hdr[0]; rep[1] = hdr[1]; rep[2] = (unsigned char)st;
    p4(rep + 3, (unsigned long)olen); p4(rep + 7, areq);
    p4(rep + 11, olen > 0 ? t4_adler32(rep + LREP_HDR, olen) : 1UL);
    rep[R_NODE] = (unsigned char)node;
    return LREP_HDR + olen;
}

long job_read(Channel *c, unsigned char *hdr, unsigned char *buf)
{
    long n, i;
    ChanIn(c, hdr, JOB_HDR);
    n = (long)g4(hdr + H_LEN);
    if (n < 0 || n > JOB_MAX) {                 /* ttjobd refuses these first; drain so the link stays in step */
        unsigned char sink[64];
        for (i = 0; i < n; i += 64) ChanIn(c, sink, (int)(n - i > 64 ? 64 : n - i));
        return -1;
    }
    if (n > 0) ChanIn(c, buf, (int)n);
    return n;
}

long rep_read(Channel *c, unsigned char *rep)
{
    long olen;
    ChanIn(c, rep, LREP_HDR);
    olen = (long)g4(rep + 3);
    if (olen > 0) ChanIn(c, rep + LREP_HDR, (int)olen);
    return LREP_HDR + olen;
}
