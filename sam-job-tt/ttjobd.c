/* ttjobd — tt030-transputer-job-queue FR-2, Phase 4 (pipelined). The only process that touches the ATW800/2 link:
 * takes /tmp/t425.lock (retrying while an SSH login's offload holds it), boots the resident image once, and serves
 * jobwire requests from local clients over a Unix socket. Up to NODES jobs are in flight at once (one per node; the
 * T425 dispatcher picks the node for NODE_ANY), so jobs from different clients run in parallel. Each job gets a
 * ttjobd wire id, mapped back to the client's own id in the reply. If the image cannot be booted, or a reply does
 * not come within JOB_DEADLINE_S, it answers ST_DOWN so clients run the op on the 68030 (C4). Messages go to stderr
 * (rc.ttjobd: /var/log/ttjobd.log) and syslog (forwarded to fractal, central logging).
 * Usage: ttjobd [IMAGE.BTL] [SOCKET]   defaults /usr/lib/t425/jobfull.btl, /var/run/ttjob.sock */
#include "atwboot.h"
#include "t4lock.h"
#include "jobwire.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <mint/mintbind.h>
#define MAXCL 8
#define QMAX 32
#define JOB_DEADLINE_S 300        /* ponytail: one deadline for every op; FR-7 makes it per-op and size-derived */

struct job { int fd; unsigned short cid, wid; long n; unsigned char *buf; time_t sent; int node; };
static struct job queue[QMAX], inflight[NODES];
static int nq, nin, up;
static unsigned short nextwid;
static unsigned char rbuf[LREP_HDR + JOB_MAX];
static long rgot;
static clock_t last_io;
static int busy[NODES];
static clock_t end0;                 /* when the T425's current job is expected to finish */

/* Expected run time in ms on the T425 (tram = 0) or a TRAM, measured on the real TT 2026-10-13: bzip2 -1 16.5 s /
 * 256 KB on the T425, 46 s on a TRAM (2.8x); crypto 2.0x (X25519 2.8 vs 5.6 s). ponytail: a static table; learn
 * per-op rates from finished jobs if the ops or nodes change. */
static long est_ms(int op, long n, int tram)
{
    long us; int comp = 0;
    switch (op) {
    case 'b': us = n * 63; comp = 1; break;
    case 'z': case 'g': case 'r': us = n * 50; comp = 1; break;
    case 'i': us = n * 5; comp = 1; break;
    case 'X': us = 2800000L; break;
    case 'V': us = 6500000L + n * 14; break;
    case 'S': us = n * 14; break;
    case 'C': us = n * 5; break;
    default: us = n / 2; break;
    }
    if (tram) us = comp ? us / 10 * 28 : us * 2;
    return us / 1000;
}
/* FR-6, queue-aware: walk the queue in order with a running T425 backlog (time left on its current job + the jobs
 * already reserved for it). A job goes to the T425 if it is idle; otherwise to an idle TRAM only if the TRAM would
 * finish it before the T425 could (backlog + its own T425 time); otherwise it stays queued, reserved for the T425,
 * and the backlog grows. Judging the head job alone never used a TRAM (one 46 s TRAM job never beats a <= 33 s
 * wait), 1.02x on 9 jobs; first-idle sent jobs to slow TRAMs that the T425 would have finished sooner, 1.07x on 3.
 * Returns the queue index to send next and sets *node, or -1 = nothing to send now. Explicit-node jobs (tests)
 * go only to that node when it is idle. */
static int pick(int *node)
{
    long backlog = 0;
    int q, k;
    if (busy[0]) {
        backlog = ((long)end0 - (long)clock()) * 1000L / (long)CLOCKS_PER_SEC;
        if (backlog < 0) backlog = 0;
    }
    for (q = 0; q < nq; q++) {
        struct job *j = &queue[q];
        int want = j->buf[3];
        long t425;
        if (want != NODE_ANY) {
            if (want < NODES && !busy[want]) { *node = want; return q; }
            continue;
        }
        if (!busy[0] && backlog == 0) { *node = 0; return q; }
        t425 = backlog + est_ms(j->buf[2], j->n, 0);
        for (k = 1; k < NODES; k++)
            if (!busy[k] && est_ms(j->buf[2], j->n, 1) < t425) { *node = k; return q; }
        backlog = t425;                                   /* reserved for the T425 */
    }
    return -1;
}
static void say(int prio, const char *fmt, ...)
{
    char m[256];
    va_list ap;
    va_start(ap, fmt); vsnprintf(m, sizeof m, fmt, ap); va_end(ap);
    fprintf(stderr, "ttjobd: %s\n", m);
    syslog(prio, "%s", m);
}
static unsigned long g4(const unsigned char *b)
{ return b[0] | ((unsigned long)b[1] << 8) | ((unsigned long)b[2] << 16) | ((unsigned long)b[3] << 24); }
static int rd(int fd, unsigned char *b, long n) { long g = 0, r; while (g < n) { r = read(fd, b + g, n - g); if (r <= 0) return -1; g += r; } return 0; }
static int wr(int fd, const unsigned char *b, long n) { long g = 0, r; while (g < n) { r = write(fd, b + g, n - g); if (r <= 0) return -1; g += r; } return 0; }
static void answer(int fd, unsigned short cid, int st)      /* a status-only reply to a client */
{
    unsigned char r[REP_HDR];
    memset(r, 0, sizeof r); r[0] = (unsigned char)cid; r[1] = (unsigned char)(cid >> 8); r[2] = (unsigned char)st;
    if (fd >= 0) wr(fd, r, REP_HDR);
}
static void go_down(const char *why)                        /* link out of step: fail everything to the 68030 */
{
    int i;
    if (up) say(LOG_ERR, "%s, now DOWN; jobs fall back to the 68030", why);
    up = 0;
    for (i = 0; i < nin; i++) { answer(inflight[i].fd, inflight[i].cid, ST_DOWN); free(inflight[i].buf); }
    nin = 0;
    memset(busy, 0, sizeof busy);
    for (i = 0; i < nq; i++) { answer(queue[i].fd, queue[i].cid, ST_DOWN); free(queue[i].buf); }
    nq = 0;
}
static void send_jobs(void)
{
    while (up && nq > 0 && nin < NODES) {
        struct job j;
        int node, q = pick(&node);
        if (q < 0) return;                          /* nothing should start now: queued jobs wait for the T425 */
        j = queue[q];
        if (getenv("TTJOBD_TRACE"))
            say(LOG_INFO, "send wid %u op %c n %ld req %d qpos %d -> node %d (busy %d%d%d, T425 left %ld ms)",
                (unsigned)(nextwid + 1), j.buf[2], j.n, j.buf[3], q, node, busy[0], busy[1], busy[2],
                busy[0] ? ((long)end0 - (long)clock()) * 1000L / (long)CLOCKS_PER_SEC : 0L);
        memmove(queue + q, queue + q + 1, (size_t)(nq - q - 1) * sizeof queue[0]); nq--;
        j.node = node; j.buf[3] = (unsigned char)node; busy[node] = 1;
        if (node == 0) end0 = clock() + (clock_t)(est_ms(j.buf[2], j.n, 0) * CLOCKS_PER_SEC / 1000);
        j.wid = ++nextwid;
        j.buf[0] = (unsigned char)j.wid; j.buf[1] = (unsigned char)(j.wid >> 8);
        if (link_write(j.buf, JOB_HDR + j.n) != JOB_HDR + j.n) { free(j.buf); go_down("link write failed"); return; }
        j.sent = time(NULL);
        last_io = clock();
        inflight[nin++] = j;
    }
}
static void poll_link(void)
{
    long want, olen;
    int i;
    for (;;) {
        want = rgot < LREP_HDR ? LREP_HDR - rgot : LREP_HDR + (long)g4(rbuf + 3) - rgot;
        if (rgot >= LREP_HDR && ((long)g4(rbuf + 3) < 0 || (long)g4(rbuf + 3) > JOB_MAX)) { go_down("bad reply length"); rgot = 0; return; }
        if (want == 0) {                                       /* one whole reply */
            unsigned short wid = (unsigned short)(rbuf[0] | (rbuf[1] << 8));
            olen = (long)g4(rbuf + 3);
            for (i = 0; i < nin && inflight[i].wid != wid; i++) ;
            if (i < nin) {
                rbuf[0] = (unsigned char)inflight[i].cid; rbuf[1] = (unsigned char)(inflight[i].cid >> 8);
                if (inflight[i].fd >= 0) {          /* client protocol: header without the node byte, ONE write: */
                    int nd = rbuf[R_NODE];           /* two writes cost the client a second FreeMiNT wake-up per job */
                    if (olen > 0) memmove(rbuf + REP_HDR, rbuf + LREP_HDR, (size_t)olen);
                    wr(inflight[i].fd, rbuf, REP_HDR + olen);
                    rbuf[R_NODE] = (unsigned char)nd; /* the log line below still reads it */
                }
                if (time(NULL) - inflight[i].sent >= 1)
                    say(LOG_INFO, "job '%c' %ld B on node %d: %ld s, status %d", inflight[i].buf[2], inflight[i].n,
                        rbuf[R_NODE], (long)(time(NULL) - inflight[i].sent), rbuf[2]);
                if (getenv("TTJOBD_TRACE"))
                    say(LOG_INFO, "reply wid %u from node %d (assigned %d)", wid, rbuf[R_NODE], inflight[i].node);
                busy[inflight[i].node] = 0;
                free(inflight[i].buf);
                inflight[i] = inflight[--nin];
            } else
                say(LOG_WARNING, "reply for unknown job %u dropped", wid);
            rgot = 0;
            continue;
        }
        {
            long r = link_read_some(rbuf + rgot, want);
            if (r <= 0) return;                                 /* nothing more yet: back to select */
            rgot += r;
            last_io = clock();
        }
    }
}
int main(int argc, char **argv)
{
    const char *img = argc > 1 ? argv[1] : "/usr/lib/t425/jobfull.btl";
    const char *path = argc > 2 ? argv[2] : "/var/run/ttjob.sock";
    const char *why = "";
    struct sockaddr_un sa;
    int ls, cl[MAXCL], i, m, try;
    fd_set rs;
    openlog("ttjobd", LOG_PID, LOG_DAEMON);
    for (try = 1; t4lock_take(&why) != 1; try++) {          /* FR-2: an SSH login can hold the lock at boot */
        if (try >= 40) { say(LOG_ERR, "T425 lock still not ours after %d tries (%s); giving up", try, why ? why : "?"); return 1; }
        if (try == 1) say(LOG_INFO, "T425 lock not ours (%s); retrying every 3 s", why ? why : "?");
        sleep(3);
    }
    atw_verbose = 0;
    up = atw_boot(img, "#500000") == 0;
    say(up ? LOG_INFO : LOG_ERR, "image %s %s", img, up ? "booted" : "NOT booted - serving DOWN");
    if ((ls = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) { perror("socket"); return 1; }
    memset(&sa, 0, sizeof sa); sa.sun_family = AF_UNIX; strncpy(sa.sun_path, path, sizeof sa.sun_path - 1);
    unlink(path);
    if (bind(ls, (struct sockaddr *)&sa, sizeof sa) < 0 || listen(ls, MAXCL) < 0) { perror(path); return 1; }
    chmod(path, 0660);
    for (i = 0; i < MAXCL; i++) cl[i] = -1;
    for (;;) {
        /* Short jobs: for 20 ms after link activity, spin on the link alone (no select, no Syield). Syield hands the
         * 68030 to any other ready process for up to a whole time slice, which cost 6-8 ms per small job against
         * 4 ms for the Phase 3 daemon that waited on the link directly (NFR-1, 2026-10-13). */
        if (nin > 0 && (long)(clock() - last_io) * 1000L / (long)CLOCKS_PER_SEC < 20) {
            /* the clock is a system call too: look at it only every 32 polls, so the spin is almost all driver reads
             * (each pass of the Phase 3 daemon was one driver read; this loop's extra calls cost ~1-2 ms per job) */
            int k, was = nin;
            for (k = 0; k < 32 && nin == was; k++) poll_link();
            send_jobs();
            continue;
        }
        /* Jobs in flight: poll the link. FreeMiNT rounds a short select() timeout up to its scheduler tick (a 2 ms
         * timeout gave 45 ms round-trips, 2026-10-13), so for 100 ms after link activity poll without sleeping
         * (yielding between polls; short jobs answer within that), then sleep 20 ms between polls so a long job
         * does not burn the 68030. */
        int hot = nin > 0 && (long)(clock() - last_io) * 1000L / (long)CLOCKS_PER_SEC < 100;
        struct timeval tv = { 0, hot ? 0 : 20000 };
        FD_ZERO(&rs); FD_SET(ls, &rs); m = ls;
        for (i = 0; i < MAXCL; i++) if (cl[i] >= 0) { FD_SET(cl[i], &rs); if (cl[i] > m) m = cl[i]; }
        /* block only when nothing is in flight AND nothing is queued: a queued job waiting for the T425 must be
         * sent as soon as a reply frees it (blocking with a queue left jobs stranded, 2026-10-13) */
        if (select(m + 1, &rs, NULL, NULL, (nin > 0 || nq > 0) ? &tv : NULL) < 0) { if (errno == EINTR) continue; perror("select"); return 1; }
        if (FD_ISSET(ls, &rs)) {
            int c = accept(ls, NULL, NULL);
            for (i = 0; i < MAXCL && cl[i] >= 0; i++) ;
            if (c >= 0 && i < MAXCL) cl[i] = c; else if (c >= 0) close(c);
        }
        for (i = 0; i < MAXCL; i++) {
            unsigned char h[JOB_HDR];
            long n;
            int k;
            if (cl[i] < 0 || !FD_ISSET(cl[i], &rs)) continue;
            if (rd(cl[i], h, JOB_HDR)) {                     /* client gone: its in-flight replies go nowhere */
                for (k = 0; k < nin; k++) if (inflight[k].fd == cl[i]) inflight[k].fd = -1;
                for (k = 0; k < nq; k++) if (queue[k].fd == cl[i]) queue[k].fd = -1;
                close(cl[i]); cl[i] = -1; continue;
            }
            n = (long)g4(h + H_LEN);
            if (n < 0 || n > JOB_MAX) { answer(cl[i], (unsigned short)(h[0] | (h[1] << 8)), ST_TOOBIG); close(cl[i]); cl[i] = -1; continue; }
            if (h[3] != NODE_ANY && h[3] >= NODES) { unsigned char sink[256]; long g; for (g = 0; g < n; g += 256) rd(cl[i], sink, n - g > 256 ? 256 : n - g); answer(cl[i], (unsigned short)(h[0] | (h[1] << 8)), ST_NONODE); continue; }
            if (!up || nq >= QMAX) {
                unsigned char sink[256]; long g;
                for (g = 0; g < n; g += 256) rd(cl[i], sink, n - g > 256 ? 256 : n - g);
                answer(cl[i], (unsigned short)(h[0] | (h[1] << 8)), ST_DOWN);
                continue;
            }
            queue[nq].fd = cl[i]; queue[nq].cid = (unsigned short)(h[0] | (h[1] << 8)); queue[nq].n = n;
            if (!(queue[nq].buf = malloc(JOB_HDR + n)) || (memcpy(queue[nq].buf, h, JOB_HDR), n > 0 && rd(cl[i], queue[nq].buf + JOB_HDR, n))) {
                free(queue[nq].buf); answer(cl[i], queue[nq].cid, ST_DOWN); continue;
            }
            nq++;
        }
        send_jobs();
        if (nin > 0) { poll_link(); send_jobs(); if (hot) Syield(); }
        for (i = 0; i < nin; i++)
            if (time(NULL) - inflight[i].sent > JOB_DEADLINE_S) { go_down("a job missed its deadline"); break; }
    }
}
