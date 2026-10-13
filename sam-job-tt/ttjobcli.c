/* ttjobcli — Phase 1 test client for ttjobd (tt030-transputer-job-queue): echo and SHA-256 on every node checked
 * against the 68030, a corrupted-request check (expects ST_BADREQ), and NFR-1: 4-byte round-trips through the
 * daemon. Usage: ttjobcli [SOCKET] */
#include "jobwire.h"
#include "sha256.h"
#include "t4adler.h"
#include <stdio.h>
#include <string.h>
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
    id++; req[0] = id; req[1] = id >> 8; req[2] = op; req[3] = node; p4(req + 4, n); p4(req + 8, corrupt ? a ^ 1 : a);
    memcpy(req + JOB_HDR, in, n);
    if (wr(req, JOB_HDR + n) || rd(rep, REP_HDR)) return -1;
    *olen = (long)g4(rep + 3);
    if (*olen > 0 && rd(rep + REP_HDR, *olen)) return -1;
    if (rep[2] == ST_OK && (g4(rep + 7) != a || g4(rep + 11) != t4_adler32(rep + REP_HDR, *olen))) return 100;  /* checksum */
    return rep[2];
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
    }
    printf("%s\n", fails ? "FAILURES" : "all checks passed");
    return fails != 0;
}
long ms_since(clock_t t0) { return (long)((clock() - t0) * 1000 / CLOCKS_PER_SEC); }
