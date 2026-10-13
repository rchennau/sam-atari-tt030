/* ttjobd — tt030-transputer-job-queue FR-2 (Phase 1: synchronous). The only process that touches the ATW800/2
 * link: takes /tmp/t425.lock for its lifetime, boots the resident image once, and serves jobwire requests from
 * local clients over a Unix socket. If the image cannot be booted or the link fails, it answers ST_DOWN at once
 * so clients run the op on the 68030 (C4). Usage: ttjobd [IMAGE.BTL] [SOCKET]
 *   defaults /usr/lib/t425/jobfull.btl, /var/run/ttjob.sock */
#include "atwboot.h"
#include "t4lock.h"
#include "jobwire.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#define MAXCL 8
static unsigned char req[JOB_HDR + JOB_MAX], rep[REP_HDR + JOB_MAX];
static int up;
static unsigned long g4(const unsigned char *b)
{ return b[0] | ((unsigned long)b[1] << 8) | ((unsigned long)b[2] << 16) | ((unsigned long)b[3] << 24); }
static int rd(int fd, unsigned char *b, long n) { long g = 0, r; while (g < n) { r = read(fd, b + g, n - g); if (r <= 0) return -1; g += r; } return 0; }
static int wr(int fd, const unsigned char *b, long n) { long g = 0, r; while (g < n) { r = write(fd, b + g, n - g); if (r <= 0) return -1; g += r; } return 0; }
static long status_only(int st)
{
    memset(rep, 0, REP_HDR); rep[0] = req[0]; rep[1] = req[1]; rep[2] = (unsigned char)st;
    return REP_HDR;
}
/* One job over the link; returns the reply length in rep[]. Header merged into the payload write (one trap). */
static long do_job(long n)
{
    long olen;
    if (!up) return status_only(ST_DOWN);
    if (link_write(req, JOB_HDR + n) != JOB_HDR + n || link_read(rep, REP_HDR, 120000) != REP_HDR) {
        up = 0;                                    /* link out of step: FR-7's watchdog will own this later */
        fprintf(stderr, "ttjobd: link failed, now DOWN\n");
        return status_only(ST_DOWN);
    }
    olen = (long)g4(rep + 3);
    if (olen < 0 || olen > JOB_MAX || (olen > 0 && link_read(rep + REP_HDR, olen, 30000) != olen)) {
        up = 0;
        fprintf(stderr, "ttjobd: bad reply, now DOWN\n");
        return status_only(ST_DOWN);
    }
    return REP_HDR + olen;
}
int main(int argc, char **argv)
{
    const char *img = argc > 1 ? argv[1] : "/usr/lib/t425/jobfull.btl";
    const char *path = argc > 2 ? argv[2] : "/var/run/ttjob.sock";
    const char *why = "";
    struct sockaddr_un sa;
    int ls, cl[MAXCL], i, m;
    fd_set rs;
    if (t4lock_take(&why) != 1) { fprintf(stderr, "ttjobd: T425 lock not ours: %s\n", why ? why : "?"); return 1; }
    atw_verbose = 0;
    up = atw_boot(img, "#500000") == 0;
    fprintf(stderr, "ttjobd: image %s %s\n", img, up ? "booted" : "NOT booted - serving DOWN");
    if ((ls = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) { perror("socket"); return 1; }
    memset(&sa, 0, sizeof sa); sa.sun_family = AF_UNIX; strncpy(sa.sun_path, path, sizeof sa.sun_path - 1);
    unlink(path);
    if (bind(ls, (struct sockaddr *)&sa, sizeof sa) < 0 || listen(ls, MAXCL) < 0) { perror(path); return 1; }
    chmod(path, 0660);
    for (i = 0; i < MAXCL; i++) cl[i] = -1;
    for (;;) {
        FD_ZERO(&rs); FD_SET(ls, &rs); m = ls;
        for (i = 0; i < MAXCL; i++) if (cl[i] >= 0) { FD_SET(cl[i], &rs); if (cl[i] > m) m = cl[i]; }
        if (select(m + 1, &rs, NULL, NULL, NULL) < 0) { if (errno == EINTR) continue; perror("select"); return 1; }
        if (FD_ISSET(ls, &rs)) {
            int c = accept(ls, NULL, NULL);
            for (i = 0; i < MAXCL && cl[i] >= 0; i++) ;
            if (c >= 0 && i < MAXCL) cl[i] = c; else if (c >= 0) close(c);
        }
        for (i = 0; i < MAXCL; i++) {
            long n, rl;
            if (cl[i] < 0 || !FD_ISSET(cl[i], &rs)) continue;
            if (rd(cl[i], req, JOB_HDR)) { close(cl[i]); cl[i] = -1; continue; }
            n = (long)g4(req + H_LEN);
            if (n < 0 || n > JOB_MAX) { rl = status_only(ST_TOOBIG); wr(cl[i], rep, rl); close(cl[i]); cl[i] = -1; continue; }
            if (n > 0 && rd(cl[i], req + JOB_HDR, n)) { close(cl[i]); cl[i] = -1; continue; }
            if (req[3] > 2) rl = status_only(ST_NONODE); else rl = do_job(n);
            if (wr(cl[i], rep, rl)) { close(cl[i]); cl[i] = -1; }
        }
    }
}
