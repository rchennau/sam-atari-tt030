/* ttjobd — tt030-transputer-job-queue FR-2, Phase 4 (pipelined). The only process that touches the ATW800/2 link:
 * takes /tmp/t425.lock (retrying while an SSH login's offload holds it), boots the resident image once, and serves
 * jobwire requests from local clients over a Unix socket. Up to NODES jobs are in flight at once (one per node; the
 * T425 dispatcher picks the node for NODE_ANY), so jobs from different clients run in parallel. Each job gets a
 * ttjobd wire id, mapped back to the client's own id in the reply. If the image cannot be booted, or a reply does
 * not come by its deadline, the watchdog (FR-7) answers ST_DOWN for everything outstanding so clients run the op on
 * the 68030 (C4), resets and re-boots the image, and logs the recovery time. Messages go to stderr
 * (rc.ttjobd: /var/log/ttjobd.log) and syslog (forwarded to the remote system management host, central logging).
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
#include <signal.h>
#include <fcntl.h>
#include <mint/mintbind.h>
#define MAXCL 8
#define QMAX 32
#define REBOOT_RETRY_S 60         /* a failed re-boot is tried again this often */

struct job { int fd; unsigned short cid, wid; long n; unsigned char *buf; time_t sent; int node; clock_t due; };
static struct job queue[QMAX], inflight[NODES];
static int nq, nin, up;
static int leased = -1;              /* client index holding the T425 lease (OP_LEASE), or -1 */
static const char *img;
static int absent[NODES];            /* FR-8: nodes the booted image does not contain (never scheduled) */
static const char *topo_path = "/var/run/ttjob.topology";
static time_t down_since;
static unsigned short nextwid;
static unsigned char rbuf[LREP_HDR + JOB_BIG];          /* node 0 replies up to JOB_BIG (ttjob-ports FR-7) */
static long big_max = JOB_MAX;      /* FR-7: node 0's payload cap, from the booted image's OP_CAPS answer; JOB_MAX until then */
static long rgot;
static clock_t last_io;
static int busy[NODES];
static clock_t end0;                 /* when the T425's current job is expected to finish */
/* Client sockets are non-blocking and every reply goes through a per-client output buffer: a blocking reply write
 * into a client that is itself blocked writing its next request deadlocked both (one client with more than ~3 large
 * jobs outstanding, 2026-10-13). ponytail: one buffer per client, freed when drained, bounded by that client's own queued jobs. */
static int cl[MAXCL];
/* Replies waiting for a client's socket: one chunk per reply, freed as soon as it is sent. A single realloc-grown
 * buffer per client fragmented the heap to 33 MB and ran out of memory under a 90-job soak (2026-10-14). */
struct chunk { struct chunk *next; long len, off; unsigned char d[1]; };
static struct chunk *oq[MAXCL], *oqt[MAXCL];
#define CLIENT_MAX_JOBS 16   /* jobs one client may have outstanding; more are answered ST_DOWN (run on the 68030) */

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
    case '5': case '6': us = n * 56; comp = 1; break;   /* lha encode: 11.6-14.2 s / 256 KB on the T425 (2026-10-14 gate) */
    case 'd': us = n * 45; comp = 1; break;             /* lha decode, n = packed bytes: 3.5 s / 95 KB */
    case 'p': us = n * 150; comp = 1; break;            /* tpnmtopng: ~110 us/B on the T425 (2026-09-25 gate, 800x600 ~157 s) */
    case 'q': us = 55000000L + n * 300; comp = 1; break; /* tppmquant 256, llvm image, direct T425: 200x150 (90 KB, 21.7k colours)
                                                          * 82 s (47-112 s via ttjobd), 352x352 167 s, 512x512 237 s (2026-10-19). The colour histogram
                                                          * makes a fixed ~55 s; byte-linear alone gave a 90 KB job an 86 s
                                                          * deadline and reset the network (defect 52b70e6b). */
    case 'c': us = 300000000L; break;                   /* bc: run time is the program's, not its size; 300 s -> 905 s watchdog */
    case 'J': us = n * 30; comp = 1; break;             /* ponytail: libjpeg encode, raw rows; estimate until FR-5 measures */
    case 'j': us = n * 300; comp = 1; break;            /* libjpeg decode, n = JPEG bytes (HighWire spike: 1024x768 28 s on the T425) */
    default: us = n / 2; break;
    }
    if (tram && (op == 'J' || op == 'j' || op == 'p' || op == 'q' || op == 'c' || op == OP_CAPS || n > JOB_MAX))   /* T425-image ops; FR-7 */
        return 0x7fffffffL / 1000;
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
            if (want < NODES && !absent[want] && !busy[want]) { *node = want; return q; }
            continue;
        }
        if (!busy[0] && backlog == 0) { *node = 0; return q; }    /* node 0 (the T425) is in every image */
        t425 = backlog + est_ms(j->buf[2], j->n, 0);
        for (k = 1; k < NODES; k++)
            if (!absent[k] && !busy[k] && est_ms(j->buf[2], j->n, 1) < t425) { *node = k; return q; }
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
/* FR-2: job payloads and results (X25519 scalars, shared secrets) are wiped before their memory is reused */
static void wipe(void *p, long n) { volatile unsigned char *v = p; while (n-- > 0) *v++ = 0; }
static void wipe_free(unsigned char *p, long n) { if (p) { wipe(p, n); free(p); } }
static unsigned long g4(const unsigned char *b)
{ return b[0] | ((unsigned long)b[1] << 8) | ((unsigned long)b[2] << 16) | ((unsigned long)b[3] << 24); }
/* read exactly n bytes from a non-blocking client: wait up to 5 s for each piece, so a stalled client is dropped
 * instead of hanging the daemon */
static int rd(int fd, unsigned char *b, long n)
{
    long g = 0, r;
    while (g < n) {
        r = read(fd, b + g, n - g);
        if (r > 0) { g += r; continue; }
        if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
            fd_set f; struct timeval tv = { 5, 0 };
            FD_ZERO(&f); FD_SET(fd, &f);
            if (select(fd + 1, &f, NULL, NULL, &tv) > 0) continue;
        }
        return -1;
    }
    return 0;
}
static void drop_out(int i)                                 /* free (wiped) every reply still queued for client i */
{
    while (oq[i]) { struct chunk *c = oq[i]; oq[i] = c->next; wipe_free((unsigned char *)c, (long)sizeof *c + c->len); }
    oqt[i] = NULL;
}
static int boot_network(void);
static void drop_client(int i)                              /* close client i: its jobs' replies go nowhere */
{
    int k;
    for (k = 0; k < nin; k++) if (inflight[k].fd == cl[i]) inflight[k].fd = -1;
    for (k = 0; k < nq; k++) if (queue[k].fd == cl[i]) queue[k].fd = -1;
    close(cl[i]); cl[i] = -1;
    drop_out(i);
    if (i == leased) {                                      /* lease holder exited (or crashed): take the T425 back */
        leased = -1; rgot = 0;
        up = boot_network();
        say(up ? LOG_INFO : LOG_ERR, "lease released; re-boot %s", up ? "succeeded, serving again" : "failed, retrying");
        if (!up) down_since = time(NULL);
    }
}
static void flush_out(int i)                                /* write what the client's socket takes now */
{
    while (oq[i]) {
        struct chunk *c = oq[i];
        long r = write(cl[i], c->d + c->off, c->len - c->off);
        if (r > 0) {
            c->off += r;
            if (c->off == c->len) { oq[i] = c->next; if (!oq[i]) oqt[i] = NULL; wipe_free((unsigned char *)c, (long)sizeof *c + c->len); }
            continue;
        }
        if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) return;
        drop_out(i);                                        /* client gone: the read side closes it */
        return;
    }
}
static void wr(int fd, const unsigned char *b, long n)      /* queue a reply to a client and send what fits */
{
    int i;
    for (i = 0; i < MAXCL && cl[i] != fd; i++) ;
    if (i == MAXCL || fd < 0) return;
    {
        struct chunk *c = malloc(sizeof *c + (size_t)n);
        if (!c) { say(LOG_ERR, "out of memory for a reply; dropping client"); drop_client(i); return; }
        c->next = NULL; c->len = n; c->off = 0; memcpy(c->d, b, (size_t)n);
        if (oqt[i]) oqt[i]->next = c; else oq[i] = c;
        oqt[i] = c;
    }
    flush_out(i);
}
static void answer(int fd, unsigned short cid, int st)      /* a status-only reply to a client */
{
    unsigned char r[REP_HDR];
    memset(r, 0, sizeof r); r[0] = (unsigned char)cid; r[1] = (unsigned char)(cid >> 8); r[2] = (unsigned char)st;
    if (fd >= 0) wr(fd, r, REP_HDR);
}
/* FR-8: boot the largest image the hardware allows. A network image whose loader hangs on a missing TRAM never sends
 * its ready marker, which atw_boot now reports as a failure, so try: full (T425 + TRAM 1 + TRAM 2) -> T425 + TRAM 1 ->
 * T425 only, from the same directory as IMAGE (or the comma-separated list in TTJOBD_IMAGES, for tests). On success
 * mark the nodes this image lacks absent and write the topology file. Returns 1 if an image booted. */
static int boot_network(void)
{
    static char list[3][256];
    const char *names[3] = { NULL, "jobt1.btl", "jobt0.btl" }, *env = getenv("TTJOBD_IMAGES"), *sl;
    int nimg = 0, i, k, rc;
    FILE *f;
    if (env) {                                      /* test override: up to 3 paths; present nodes = 3, 2, 1 in order */
        const char *p = env;
        while (*p && nimg < 3) {
            size_t len = strcspn(p, ",");
            snprintf(list[nimg++], sizeof list[0], "%.*s", (int)len, p);
            p += len + (p[len] == ',');
        }
    } else {
        snprintf(list[nimg++], sizeof list[0], "%s", img);
        sl = strrchr(img, '/');
        for (i = 1; i < 3; i++)
            snprintf(list[nimg++], sizeof list[0], "%.*s%s", sl ? (int)(sl - img + 1) : 0, img, names[i]);
    }
    for (i = 0; i < nimg; i++) {
        if ((rc = atw_boot(list[i], "#500000")) == 0) {
            for (k = 0; k < NODES; k++) absent[k] = k >= NODES - i;
            say(i ? LOG_WARNING : LOG_INFO, "image %s booted (%d of %d nodes)", list[i], NODES - i, NODES);
            if ((f = fopen(topo_path, "w"))) {
                time_t now = time(NULL);
                fprintf(f, "# ttjobd %s# image %s\n# node link cpu clock_mhz ram_kb state\n", ctime(&now), list[i]);
                fprintf(f, "0 host:L0 T425 40 5120 present\n");
                fprintf(f, "1 T425.L1-L3 T4 20 4096 %s\n", absent[1] ? "absent" : "present");
                fprintf(f, "2 TRAM1.L1-L0 T4 20 4096 %s\n", absent[2] ? "absent" : "present");
                fclose(f);
            }
            big_max = JOB_MAX;              /* FR-7: ask the image; an older one answers ST_BADOP and keeps JOB_MAX */
            if (nq < QMAX && (queue[nq].buf = calloc(1, JOB_HDR))) {
                queue[nq].buf[2] = OP_CAPS; queue[nq].buf[3] = 0; queue[nq].buf[H_ADLER] = 1;   /* adler32 of nothing */
                queue[nq].fd = -1; queue[nq].cid = 0; queue[nq].n = 0; nq++;
            }
            return 1;
        }
        say(LOG_WARNING, "image %s did not boot (atw_boot %d)%s", list[i], rc, i + 1 < nimg ? "; trying the next" : "");
    }
    return 0;
}
static void go_down(const char *why)                        /* link out of step: fail everything to the 68030 */
{
    int i;
    if (up) say(LOG_ERR, "%s, now DOWN; jobs fall back to the 68030", why);
    up = 0;
    down_since = time(NULL);                        /* main loop retries the boot every REBOOT_RETRY_S */
    for (i = 0; i < nin; i++) { answer(inflight[i].fd, inflight[i].cid, ST_DOWN); wipe_free(inflight[i].buf, JOB_HDR + inflight[i].n); }
    nin = 0;
    memset(busy, 0, sizeof busy);
    for (i = 0; i < nq; i++) { answer(queue[i].fd, queue[i].cid, ST_DOWN); wipe_free(queue[i].buf, JOB_HDR + queue[i].n); }
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
        /* FR-7: deadline = 3 x the expected time on that node + 5 s */
        j.due = clock() + (clock_t)((3 * est_ms(j.buf[2], j.n, node != 0) + 5000) * (long)CLOCKS_PER_SEC / 1000);
        j.wid = ++nextwid;
        j.buf[0] = (unsigned char)j.wid; j.buf[1] = (unsigned char)(j.wid >> 8);
        if (link_write(j.buf, JOB_HDR + j.n) != JOB_HDR + j.n) { wipe_free(j.buf, JOB_HDR + j.n); go_down("link write failed"); return; }
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
        if (rgot >= LREP_HDR && ((long)g4(rbuf + 3) < 0 || (long)g4(rbuf + 3) > JOB_BIG)) { go_down("bad reply length"); rgot = 0; return; }
        if (want == 0) {                                       /* one whole reply */
            unsigned short wid = (unsigned short)(rbuf[0] | (rbuf[1] << 8));
            olen = (long)g4(rbuf + 3);
            for (i = 0; i < nin && inflight[i].wid != wid; i++) ;
            if (i < nin && inflight[i].buf[2] == OP_CAPS) {
                big_max = rbuf[2] == ST_OK && olen >= 4 ? (long)g4(rbuf + LREP_HDR) : JOB_MAX;
                if (big_max > JOB_BIG) big_max = JOB_BIG;
                if (big_max < JOB_MAX) big_max = JOB_MAX;
                say(LOG_INFO, "node 0 takes jobs up to %ld B", big_max);
                if (olen >= 16) say(LOG_INFO, "node 0 heap high-water %ld B of %ld", (long)g4(rbuf + LREP_HDR + 4),
                                    (long)(g4(rbuf + LREP_HDR + 12) - g4(rbuf + LREP_HDR + 8)));
            }
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
                wipe_free(inflight[i].buf, JOB_HDR + inflight[i].n);
                inflight[i] = inflight[--nin];
            } else
                say(LOG_WARNING, "reply for unknown job %u dropped", wid);
            wipe(rbuf, LREP_HDR + olen);                       /* the result now lives only in the client's buffer */
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
    img = argc > 1 ? argv[1] : "/usr/lib/t425/jobfull.btl";
    const char *path = argc > 2 ? argv[2] : "/var/run/ttjob.sock";
    const char *why = "";
    struct sockaddr_un sa;
    int ls, i, m, try;
    fd_set rs, ws;
    openlog("ttjobd", LOG_PID, LOG_DAEMON);
    signal(SIGPIPE, SIG_IGN);   /* a client that exits mid-reply must not kill the daemon (silent death, 2026-10-14) */
    for (try = 1; t4lock_take(&why) != 1; try++) {          /* FR-2: an SSH login can hold the lock at boot */
        if (try >= 40) { say(LOG_ERR, "T425 lock still not ours after %d tries (%s); giving up", try, why ? why : "?"); return 1; }
        if (try == 1) say(LOG_INFO, "T425 lock not ours (%s); retrying every 3 s", why ? why : "?");
        sleep(3);
    }
    atw_verbose = 0;
    up = boot_network();
    if (!up) { say(LOG_ERR, "no image booted - serving DOWN, retrying every %d s", REBOOT_RETRY_S); down_since = time(NULL); }
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
            for (k = 0; k < MAXCL; k++) if (cl[k] >= 0 && oq[k]) flush_out(k);
            continue;
        }
        /* Jobs in flight: poll the link. FreeMiNT rounds a short select() timeout up to its scheduler tick (a 2 ms
         * timeout gave 45 ms round-trips, 2026-10-13), so for 100 ms after link activity poll without sleeping
         * (yielding between polls; short jobs answer within that), then sleep 20 ms between polls so a long job
         * does not burn the 68030. */
        int hot = nin > 0 && (long)(clock() - last_io) * 1000L / (long)CLOCKS_PER_SEC < 100;
        struct timeval tv = { 0, hot ? 0 : 20000 };
        FD_ZERO(&rs); FD_ZERO(&ws); FD_SET(ls, &rs); m = ls;
        for (i = 0; i < MAXCL; i++) if (cl[i] >= 0) {
            FD_SET(cl[i], &rs); if (cl[i] > m) m = cl[i];
            if (oq[i]) FD_SET(cl[i], &ws);
        }
        /* block only when nothing is in flight AND nothing is queued: a queued job waiting for the T425 must be
         * sent as soon as a reply frees it (blocking with a queue left jobs stranded, 2026-10-13) */
        if (select(m + 1, &rs, &ws, NULL, (nin > 0 || nq > 0) ? &tv : NULL) < 0) { if (errno == EINTR) continue; perror("select"); return 1; }
        if (FD_ISSET(ls, &rs)) {
            int c = accept(ls, NULL, NULL);
            for (i = 0; i < MAXCL && cl[i] >= 0; i++) ;
            if (c >= 0 && i < MAXCL) { cl[i] = c; fcntl(c, F_SETFL, fcntl(c, F_GETFL, 0) | O_NONBLOCK); drop_out(i); }
            else if (c >= 0) close(c);
        }
        for (i = 0; i < MAXCL; i++) if (cl[i] >= 0 && FD_ISSET(cl[i], &ws)) flush_out(i);
        for (i = 0; i < MAXCL; i++) {
            unsigned char h[JOB_HDR];
            long n;
            int k, mine;
            if (cl[i] < 0 || !FD_ISSET(cl[i], &rs)) continue;
            if (rd(cl[i], h, JOB_HDR)) { drop_client(i); continue; }   /* client gone */
            n = (long)g4(h + H_LEN);
            if (n < 0 || n > big_max || (n > JOB_MAX && h[3] != NODE_ANY && h[3] != 0)) {   /* above JOB_MAX: the T425 only */
                answer(cl[i], (unsigned short)(h[0] | (h[1] << 8)), ST_TOOBIG); drop_client(i); continue;
            }
            /* af4039be: a lease hands the T425 to one client (a program that boots its own image, e.g. NetHack-ATW)
             * until its connection closes, which the kernel does when the process exits or crashes: no pid polling,
             * no idle timeout. Meanwhile jobs get ST_DOWN (68030) and the lock file still names ttjobd, so Dropbear
             * never boots its own offload over the holder's image. A second lease request is refused with ST_DOWN. */
            if (h[2] == OP_LEASE) {
                unsigned short cid = (unsigned short)(h[0] | (h[1] << 8));
                if (n != 0) { drop_client(i); continue; }
                if (leased >= 0) { answer(cl[i], cid, ST_DOWN); continue; }
                go_down("T425 leased to a client");
                down_since = 0;                             /* no re-boot retries while leased */
                leased = i;
                say(LOG_INFO, "T425 leased; jobs run on the 68030 until the holder exits");
                answer(cl[i], cid, ST_OK);
                continue;
            }
            if (h[3] != NODE_ANY && (h[3] >= NODES || absent[h[3]])) { unsigned char sink[256]; long g; for (g = 0; g < n; g += 256) rd(cl[i], sink, n - g > 256 ? 256 : n - g); answer(cl[i], (unsigned short)(h[0] | (h[1] << 8)), ST_NONODE); continue; }
            for (k = 0, mine = 0; k < nq; k++) mine += queue[k].fd == cl[i];
            for (k = 0; k < nin; k++) mine += inflight[k].fd == cl[i];
            if (!up || nq >= QMAX || mine >= CLIENT_MAX_JOBS) {   /* full: the client runs it on the 68030 */
                unsigned char sink[256]; long g;
                for (g = 0; g < n; g += 256) rd(cl[i], sink, n - g > 256 ? 256 : n - g);
                answer(cl[i], (unsigned short)(h[0] | (h[1] << 8)), ST_DOWN);
                continue;
            }
            queue[nq].fd = cl[i]; queue[nq].cid = (unsigned short)(h[0] | (h[1] << 8)); queue[nq].n = n;
            if (!(queue[nq].buf = malloc(JOB_HDR + n)) || (memcpy(queue[nq].buf, h, JOB_HDR), n > 0 && rd(cl[i], queue[nq].buf + JOB_HDR, n))) {
                wipe_free(queue[nq].buf, JOB_HDR + n); answer(cl[i], queue[nq].cid, ST_DOWN); continue;
            }
            nq++;
        }
        send_jobs();
        if (nin > 0) { poll_link(); send_jobs(); if (hot) Syield(); }
        /* FR-7 watchdog: a job past its deadline means a node is hung. Fail everything outstanding to the 68030 (the
         * clients run it there), reset + re-boot the image (the T425 reset also resets the TRAMs), log the recovery. */
        for (i = 0; i < nin; i++)
            if ((long)clock() - (long)inflight[i].due > 0) {
                clock_t t0 = clock();
                say(LOG_ERR, "job '%c' (%ld B) on node %d missed its deadline: resetting the transputer network",
                    inflight[i].buf[2], inflight[i].n, inflight[i].node);
                go_down("watchdog");
                rgot = 0;
                up = boot_network();
                if (up) say(LOG_INFO, "watchdog: network back up after %ld ms", (long)(clock() - t0) * 1000L / (long)CLOCKS_PER_SEC);
                else { say(LOG_ERR, "watchdog: re-boot failed; serving DOWN, retrying every %d s", REBOOT_RETRY_S); down_since = time(NULL); }
                break;
            }
        if (!up && down_since && time(NULL) - down_since >= REBOOT_RETRY_S) {
            up = boot_network();
            say(up ? LOG_INFO : LOG_ERR, "watchdog: re-boot %s", up ? "succeeded, network back up" : "failed again");
            down_since = up ? 0 : time(NULL);
        }
    }
}
