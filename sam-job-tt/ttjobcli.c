/* ttjobcli — Phase 1 test client for ttjobd (tt030-transputer-job-queue): echo and SHA-256 on every node checked
 * against the 68030, a corrupted-request check (expects ST_BADREQ), and NFR-1: 4-byte round-trips through the
 * daemon. Usage: ttjobcli [SOCKET] */
#include "jobwire.h"
#include "sha256.h"
#include "t4adler.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
long ms_since(clock_t t0);
static unsigned char req[JOB_HDR + JOB_MAX], rep[REP_HDR + JOB_MAX];
static int fd;
static unsigned long g4(const unsigned char *b)
{ return b[0] | ((unsigned long)b[1] << 8) | ((unsigned long)b[2] << 16) | ((unsigned long)b[3] << 24); }
static void p4(unsigned char *b, unsigned long v) { b[0] = v; b[1] = v >> 8; b[2] = v >> 16; b[3] = v >> 24; }
static int rd(unsigned char *b, long n) { long g = 0, r; while (g < n) { r = read(fd, b + g, n - g); if (r <= 0) return -1; g += r; } return 0; }
static int wr(const unsigned char *b, long n) { long g = 0, r; while (g < n) { r = write(fd, b + g, n - g); if (r <= 0) return -1; g += r; } return 0; }
static unsigned id;
/* returns status (or -1 on socket error); output in rep + REP_HDR, length *olen */
static int job(int op, int node, const unsigned char *in, long n, long *olen, int corrupt)
{
    unsigned long a = t4_adler32(in, n);
    id++; req[0] = id; req[1] = id >> 8; req[2] = op; req[3] = node; req[H_LEVEL] = (op == 'b') ? 1 : 0; p4(req + H_LEN, n); p4(req + H_ADLER, corrupt ? a ^ 1 : a);
    memcpy(req + JOB_HDR, in, n);
    if (wr(req, JOB_HDR + n) || rd(rep, REP_HDR)) return -1;
    *olen = (long)g4(rep + 3);
    if (*olen > 0 && rd(rep + REP_HDR, *olen)) return -1;
    if (rep[2] == ST_OK && (g4(rep + 7) != a || g4(rep + 11) != t4_adler32(rep + REP_HDR, *olen))) return 100;  /* checksum */
    return rep[2];
}
static int hexb(unsigned char *o, const char *h) { int i = 0; while (h[0] && h[1]) { unsigned v; sscanf(h, "%2x", &v); o[i++] = (unsigned char)v; h += 2; } return i; }
/* Phase 3: RFC 7748 X25519, RFC 8032 Ed25519 test 1 (valid + corrupted), RFC 8439 ChaCha20 on one node, timed */
static int crypto_checks(int node)
{
    static unsigned char in[256], exp[256];
    static const char *pt = "Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the future, sunscreen would be it.";
    long olen, ms; int st, bad = 0, n, i; clock_t t0;
    hexb(in, "a546e36bf0527c9d3b16154b82465edd62144c0ac1fc5a18506a2244ba449ac4e6db6867583030db3594c1a424b15f7c726624ec26b3353b10a903a6d0ab1c4c");
    hexb(exp, "c3da55379de9c6908e94ea4df28d084f32eccf03491c71f754b4075577a28552");
    t0 = clock(); st = job('X', node, in, 64, &olen, 0); ms = ms_since(t0);
    if (st || olen != 32 || memcmp(rep + REP_HDR, exp, 32)) { printf("node %d X25519: FAIL (status %d)\n", node, st); bad++; }
    printf("node %d X25519 %ld ms; ", node, ms);
    hexb(in, "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b");
    hexb(in + 64, "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
    t0 = clock(); st = job('V', node, in, 96, &olen, 0); ms = ms_since(t0);
    if (st || olen != 1 || rep[REP_HDR] != 1) { printf("node %d Ed25519 valid: FAIL\n", node); bad++; }
    in[5] ^= 1; st = job('V', node, in, 96, &olen, 0);
    if (st || olen != 1 || rep[REP_HDR] != 0) { printf("node %d Ed25519 corrupted: FAIL\n", node); bad++; }
    printf("Ed25519 verify %ld ms; ", ms);
    for (i = 0; i < 32; i++) in[i] = (unsigned char)i;
    hexb(in + 32, "000000000000004a00000000"); in[44] = 1; in[45] = in[46] = in[47] = 0;
    for (n = 0; pt[n]; n++) in[48 + n] = (unsigned char)pt[n];
    hexb(exp, "6e2e359a2568f98041ba0728dd0d6981e97e7aec1d4360c20a27afccfd9fae0bf91b65c5524733ab8f593dabcd62b3571639d624e65152ab8f530c359f0861d807ca0dbf500d6a6156a38e088a22b65e52bc514d16ccf806818ce91ab77937365af90bbf74a35be6b40b8eedf2785e42874d");
    st = job('C', node, in, 48 + n, &olen, 0);
    if (st || olen != n || memcmp(rep + REP_HDR, exp, n)) { printf("node %d ChaCha20: FAIL\n", node); bad++; }
    printf("ChaCha20 KAT %s\n", bad ? "-" : "ok");
    return bad;
}
/* NFR-3: N bzip2 -1 jobs on 256 KB of pseudo-random words, first one after another on the T425, then as NODE_ANY
 * with up to 3 in flight (a sliding window: submitting all before reading any reply can deadlock against ttjobd
 * writing a large reply into a full socket). Outputs must all be identical; prints the speed-up. */
static int submit_b(const unsigned char *in, long n)
{
    unsigned long a = t4_adler32(in, n);
    id++; req[0] = id; req[1] = id >> 8; req[2] = 'b'; req[3] = NODE_ANY; req[H_LEVEL] = 1;
    p4(req + H_LEN, n); p4(req + H_ADLER, a);
    memcpy(req + JOB_HDR, in, n);
    return wr(req, JOB_HDR + n);
}
static int par_only;                 /* -q N: skip the serial pass (several clients run concurrently, timed by the shell) */
static int par_test(int jobs)
{
    static unsigned char in[262144], first[JOB_MAX];
    static const char *w[16] = { "transputer ", "link ", "the ", "atari ", "job ", "queue ", "node ", "of ", "and ",
                                 "compress ", "a ", "memory ", "TT030 ", "is ", "\n", "parallel " };
    unsigned long r = 2463534242UL;
    long o = 0, olen, flen = 0, ms_s, ms_p;
    int k, st, bad = 0, sent = 0, got = 0;
    const char *q;
    clock_t t0;
    while (o < (long)sizeof in) {
        r ^= r << 13; r &= 0xFFFFFFFFUL; r ^= r >> 17; r ^= r << 5; r &= 0xFFFFFFFFUL;
        for (q = w[r & 15]; *q && o < (long)sizeof in; q++) in[o++] = (unsigned char)*q;
    }
    t0 = clock();
    for (k = 0; k < (par_only ? 0 : jobs); k++) {        /* -q: no serial pass; the first parallel reply is the reference */
        st = job('b', 0, in, sizeof in, &olen, 0);
        if (st || olen <= 0) { printf("serial job %d failed (status %d)\n", k, st); return 1; }
        if (k == 0) { flen = olen; memcpy(first, rep + REP_HDR, olen); }
        else if (olen != flen || memcmp(first, rep + REP_HDR, olen)) bad++;
    }
    ms_s = ms_since(t0);
    t0 = clock();
    while (got < jobs) {
        while (sent < jobs && sent - got < 3) { if (submit_b(in, sizeof in)) { printf("submit failed\n"); return 1; } sent++; }
        if (rd(rep, REP_HDR)) { printf("reply read failed\n"); return 1; }
        olen = (long)g4(rep + 3);
        if (olen > 0 && rd(rep + REP_HDR, olen)) { printf("reply read failed\n"); return 1; }
        if (par_only && got == 0 && rep[2] == ST_OK) { flen = olen; memcpy(first, rep + REP_HDR, olen); }
        if (rep[2] != ST_OK || olen != flen || memcmp(first, rep + REP_HDR, olen)) bad++;
        got++;
    }
    ms_p = ms_since(t0);
    printf("NFR-3: %d x bzip2 -1 (256 KB): serial on T425 %ld ms, parallel %ld ms = %ld.%02ldx; outputs %s\n",
           jobs, ms_s, ms_p, ms_p ? ms_s / ms_p : 0, ms_p ? (ms_s * 100 / ms_p) % 100 : 0, bad ? "DIFFER" : "identical");
    return bad;
}
int main(int argc, char **argv)
{
    static unsigned char data[16384], dig[32];
    static const long sz[3] = { 4, 1024, 16384 };
    struct sockaddr_un sa;
    struct sha256 s;
    long i, olen, ms, k;
    int node, st, fails = 0;
    clock_t t0;
    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) { perror("socket"); return 1; }
    memset(&sa, 0, sizeof sa); sa.sun_family = AF_UNIX; strncpy(sa.sun_path, argc > 1 ? argv[1] : "/var/run/ttjob.sock", sizeof sa.sun_path - 1);
    if (connect(fd, (struct sockaddr *)&sa, sizeof sa) < 0) { perror("connect"); return 1; }
    if (argc > 2 && !strcmp(argv[2], "-p")) return par_test(argc > 3 ? atoi(argv[3]) : 3);
    if (argc > 2 && !strcmp(argv[2], "-q")) { par_only = 1; return par_test(argc > 3 ? atoi(argv[3]) : 3); }
    if (argc > 5 && !strcmp(argv[2], "-1")) {        /* one job: -1 NODE OP SIZE (bytes of pseudo-random text) */
        static unsigned char in1[JOB_MAX];
        static const char *w[8] = { "transputer ", "link ", "the ", "atari ", "job ", "queue ", "node ", "\n" };
        unsigned long r = 2463534242UL; long o = 0, n = atol(argv[5]); const char *q; clock_t t0;
        int node = atoi(argv[3]), op = argv[4][0];
        if (n > JOB_MAX) n = JOB_MAX;
        while (o < n) { r ^= r << 13; r &= 0xFFFFFFFFUL; r ^= r >> 17; r ^= r << 5; r &= 0xFFFFFFFFUL; for (q = w[r & 7]; *q && o < n; q++) in1[o++] = (unsigned char)*q; }
        t0 = clock();
        st = job(op, node, in1, n, &olen, 0);
        printf("job %c node %d %ld B: status %d, out %ld B, %ld ms\n", op, node, n, st, olen, ms_since(t0));
        return st != 0;
    }
    for (i = 0; i < 16384; i++) data[i] = (unsigned char)(i * 29 + 11);
    sha256_init(&s); sha256_update(&s, data, 16384); sha256_final(&s, dig);
    for (node = 0; node < 3; node++) {
        for (k = 0; k < 3; k++) {
            st = job(OP_ECHO, node, data, sz[k], &olen, 0);
            if (st || olen != sz[k] || memcmp(rep + REP_HDR, data, sz[k])) { printf("node %d echo %ld: FAIL (status %d)\n", node, sz[k], st); fails++; }
        }
        st = job(OP_SHA256, node, data, 16384, &olen, 0);
        if (st || olen != 32 || memcmp(rep + REP_HDR, dig, 32)) { printf("node %d sha256: FAIL (status %d)\n", node, st); fails++; }
        st = job(OP_ECHO, node, data, 64, &olen, 1);
        if (st != ST_BADREQ) { printf("node %d corrupted request: got status %d, want BADREQ\n", node, st); fails++; }
        t0 = clock();
        for (i = 0; i < 400; i++) if (job(OP_ECHO, node, data, 4, &olen, 0)) { fails++; break; }
        ms = ms_since(t0);
        printf("node %d: echo 4/1K/16K + sha256 16K + BADREQ checked; 4-byte round-trip via ttjobd %ld us\n", node, ms * 1000 / 400);
        fails += crypto_checks(node);
    }
    printf("%s\n", fails ? "FAILURES" : "all checks passed");
    return fails != 0;
}
long ms_since(clock_t t0) { return (long)((clock() - t0) * 1000 / CLOCKS_PER_SEC); }
