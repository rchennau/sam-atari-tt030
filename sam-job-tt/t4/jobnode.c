/* jobnode — executes or forwards one jobwire request. No host runtime calls (linked with startrd). */
#include "jobnode.h"
#include "../jobwire.h"
#include "sha256.h"
#include "t4adler.h"
static unsigned char in_buf[JOB_MAX], out_buf[JOB_MAX];
unsigned long g4(const unsigned char *b)
{ return b[0] | ((unsigned long)b[1] << 8) | ((unsigned long)b[2] << 16) | ((unsigned long)b[3] << 24); }
void p4(unsigned char *b, unsigned long v)
{ b[0] = (unsigned char)v; b[1] = (unsigned char)(v >> 8); b[2] = (unsigned char)(v >> 16); b[3] = (unsigned char)(v >> 24); }
static void reply(Channel *out, unsigned char *hdr, int st, long olen, unsigned long areq)
{
    unsigned char r[REP_HDR];
    r[0] = hdr[0]; r[1] = hdr[1]; r[2] = (unsigned char)st;
    p4(r + 3, (unsigned long)olen); p4(r + 7, areq); p4(r + 11, olen > 0 ? t4_adler32(out_buf, olen) : 1UL);
    ChanOut(out, (char *)r, REP_HDR);
    if (olen > 0) ChanOut(out, (char *)out_buf, (int)olen);
}
static long read_payload(Channel *in, unsigned char *hdr)
{
    long n = (long)g4(hdr + 4);
    if (n < 0 || n > JOB_MAX) return -1;          /* the 68030 side refuses these before the link */
    if (n > 0) ChanIn(in, (char *)in_buf, (int)n);
    return n;
}
void job_run_here(Channel *in, Channel *out, unsigned char *hdr)
{
    long n = read_payload(in, hdr), i;
    unsigned long areq;
    struct sha256 s;
    if (n < 0) { reply(out, hdr, ST_TOOBIG, 0, 0); return; }
    areq = t4_adler32(in_buf, n);
    if (areq != g4(hdr + 8)) { reply(out, hdr, ST_BADREQ, 0, areq); return; }
    switch (hdr[2]) {
    case OP_ECHO:
        for (i = 0; i < n; i++) out_buf[i] = in_buf[i];
        reply(out, hdr, ST_OK, n, areq);
        break;
    case OP_SHA256:
        sha256_init(&s); sha256_update(&s, in_buf, (unsigned long)n); sha256_final(&s, out_buf);
        reply(out, hdr, ST_OK, 32, areq);
        break;
    default:
        reply(out, hdr, ST_BADOP, 0, areq);
    }
}
void job_forward(Channel *in, Channel *out, unsigned char *hdr, Channel *fwd_out, Channel *fwd_in)
{
    unsigned char r[REP_HDR];
    long n = read_payload(in, hdr), olen;
    if (n < 0) { reply(out, hdr, ST_TOOBIG, 0, 0); return; }
    ChanOut(fwd_out, (char *)hdr, JOB_HDR);
    if (n > 0) ChanOut(fwd_out, (char *)in_buf, (int)n);
    ChanIn(fwd_in, (char *)r, REP_HDR);
    olen = (long)g4(r + 3);
    if (olen > 0) ChanIn(fwd_in, (char *)out_buf, (int)olen);
    ChanOut(out, (char *)r, REP_HDR);
    if (olen > 0) ChanOut(out, (char *)out_buf, (int)olen);
}
