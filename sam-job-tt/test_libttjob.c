/* test_libttjob — host test (the remote system management host, gcc) for libttjob against a fake ttjobd (tt030-transputer-job-queue FR-3).
 * The fake answers in submit order: op 'E' echoes, 'X' echoes with a damaged output checksum, 'B' -> ST_BADOP,
 * 'Q' -> closes the connection. Build/run: cc -I. -I../sam-yum-tt/src test_libttjob.c libttjob.c
 * ../sam-yum-tt/src/t4adler.c -o /tmp/test_libttjob && /tmp/test_libttjob */
#include "libttjob.h"
#include "t4adler.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
static const char *PATH = "/tmp/test_libttjob.sock";
static unsigned long g4(const unsigned char *b) { return b[0] | ((unsigned long)b[1] << 8) | ((unsigned long)b[2] << 16) | ((unsigned long)b[3] << 24); }
static void p4(unsigned char *b, unsigned long v) { b[0] = v; b[1] = v >> 8; b[2] = v >> 16; b[3] = v >> 24; }
static int rd(int fd, unsigned char *b, long n) { long g = 0, r; while (g < n) { r = read(fd, b + g, n - g); if (r <= 0) return -1; g += r; } return 0; }
static void fake(int ls)
{
    static unsigned char in[JOB_MAX];
    unsigned char h[JOB_HDR], r[REP_HDR];
    int c = accept(ls, NULL, NULL);
    for (;;) {
        long n;
        if (rd(c, h, JOB_HDR)) _exit(0);
        n = (long)g4(h + H_LEN);
        if (n && rd(c, in, n)) _exit(0);
        if (h[2] == 'Q') { close(c); _exit(0); }
        memset(r, 0, sizeof r); r[0] = h[0]; r[1] = h[1];
        r[2] = h[2] == 'B' ? ST_BADOP : ST_OK;
        p4(r + 3, r[2] == ST_OK ? n : 0); p4(r + 7, t4_adler32(in, n));
        p4(r + 11, t4_adler32(in, n) ^ (h[2] == 'X'));
        write(c, r, REP_HDR);
        if (r[2] == ST_OK && n) write(c, in, n);
    }
}
int main(void)
{
    struct sockaddr_un sa; int ls, ida, idb, idc, st; pid_t p; long olen;
    unsigned char out[64];
    ttjob_socket = "/tmp/no-such-ttjob.sock";
    assert(ttjob_submit('E', 0, "x", 1) == -1);                       /* no daemon -> -1, caller falls back */
    unlink(PATH); ls = socket(AF_UNIX, SOCK_STREAM, 0);
    memset(&sa, 0, sizeof sa); sa.sun_family = AF_UNIX; strcpy(sa.sun_path, PATH);
    assert(bind(ls, (struct sockaddr *)&sa, sizeof sa) == 0 && listen(ls, 1) == 0);
    if ((p = fork()) == 0) fake(ls);
    ttjob_socket = PATH;
    ida = ttjob_submit('E', 0, "alpha", 5); idb = ttjob_submit('E', 0, "bravo!", 6); idc = ttjob_submit('E', 0, "c", 1);
    assert(ida > 0 && idb > ida && idc > idb);
    st = ttjob_wait(idc, out, sizeof out, &olen, 2000);               /* collect out of submit order */
    assert(st == ST_OK && olen == 1 && out[0] == 'c');
    assert(ttjob_poll(ida) == 1 && ttjob_poll(idb) == 1);             /* already read while waiting for idc */
    st = ttjob_wait(ida, out, sizeof out, &olen, 2000); assert(st == ST_OK && olen == 5 && !memcmp(out, "alpha", 5));
    st = ttjob_wait(idb, out, 3, &olen, 2000); assert(st == ST_TOOBIG);   /* output larger than cap */
    assert(ttjob_poll(idb) == 0);                                      /* collected: gone */
    st = ttjob_wait(ttjob_submit('X', 0, "dmg", 3), out, sizeof out, &olen, 2000); assert(st == ST_BADREQ);  /* bad checksum */
    st = ttjob_wait(ttjob_submit('B', 0, "op", 2), out, sizeof out, &olen, 2000); assert(st == ST_BADOP);
    ida = ttjob_submit('Q', 0, "", 0); assert(ida > 0);
    st = ttjob_wait(ida, out, sizeof out, &olen, 2000); assert(st == ST_DOWN);   /* connection lost -> DOWN */
    waitpid(p, NULL, 0); close(ls); unlink(PATH);
    printf("test_libttjob: all checks passed\n");
    return 0;
}
