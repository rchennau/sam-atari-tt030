/* libttjob — see libttjob.h. One connection per process, opened on first use. */
#include "libttjob.h"
#include "t4adler.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
const char *ttjob_socket = "/var/run/ttjob.sock";
struct done { int id, st; long olen; unsigned char *out; struct done *next; };
static int fd = -1, lastid;
static struct done *got;                       /* replies read but not yet collected */
static unsigned long g4(const unsigned char *b)
{ return b[0] | ((unsigned long)b[1] << 8) | ((unsigned long)b[2] << 16) | ((unsigned long)b[3] << 24); }
static void p4(unsigned char *b, unsigned long v) { b[0] = v; b[1] = v >> 8; b[2] = v >> 16; b[3] = v >> 24; }
static int wr(const unsigned char *b, long n) { long g = 0, r; while (g < n) { r = write(fd, b + g, n - g); if (r <= 0) return -1; g += r; } return 0; }
static int rd(unsigned char *b, long n) { long g = 0, r; while (g < n) { r = read(fd, b + g, n - g); if (r <= 0) return -1; g += r; } return 0; }
static void lost(void) { if (fd >= 0) close(fd); fd = -1; }
int ttjob_submit(int op, int level, const void *in, long n)
{
    unsigned char h[JOB_HDR];
    if (n < 0 || n > JOB_MAX) return -1;
    if (fd < 0) {
        struct sockaddr_un sa;
        memset(&sa, 0, sizeof sa); sa.sun_family = AF_UNIX; strncpy(sa.sun_path, ttjob_socket, sizeof sa.sun_path - 1);
        if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) return -1;
        if (connect(fd, (struct sockaddr *)&sa, sizeof sa) < 0) { lost(); return -1; }
    }
    lastid = (lastid % 65535) + 1;
    h[0] = lastid; h[1] = lastid >> 8; h[2] = op; h[3] = NODE_ANY;   /* Phase 4: the dispatcher picks the node */ h[H_LEVEL] = level;
    p4(h + H_LEN, n); p4(h + H_ADLER, t4_adler32(in, n));
    if (wr(h, JOB_HDR) || (n > 0 && wr(in, n))) { lost(); return -1; }
    return lastid;
}
/* read one reply from the socket into the got list; 0 ok, -1 lost */
static int take_one(void)
{
    unsigned char r[REP_HDR];
    struct done *d;
    if (rd(r, REP_HDR)) { lost(); return -1; }
    if (!(d = calloc(1, sizeof *d))) { lost(); return -1; }
    d->id = r[0] | (r[1] << 8); d->st = r[2]; d->olen = (long)g4(r + 3);
    if (d->olen < 0 || d->olen > JOB_MAX || (d->olen > 0 && (!(d->out = malloc(d->olen)) || rd(d->out, d->olen)))) {
        free(d->out); free(d); lost(); return -1;
    }
    if (d->st == ST_OK && d->olen > 0 && t4_adler32(d->out, d->olen) != g4(r + 11)) d->st = ST_BADREQ;   /* damaged */
    d->next = got; got = d;
    return 0;
}
static struct done *find(int id) { struct done *d; for (d = got; d; d = d->next) if (d->id == id) return d; return 0; }
static int readable(long ms)
{
    fd_set rs; struct timeval tv;
    FD_ZERO(&rs); FD_SET(fd, &rs); tv.tv_sec = ms / 1000; tv.tv_usec = (ms % 1000) * 1000;
    return select(fd + 1, &rs, NULL, NULL, &tv) > 0;
}
int ttjob_poll(int id)
{
    if (find(id)) return 1;
    if (fd < 0) return -1;
    while (readable(0)) { if (take_one()) return -1; if (find(id)) return 1; }
    return 0;
}
int ttjob_wait(int id, void *out, long cap, long *olen, long timeout_ms)
{
    struct done *d, **pp;
    int st;
    while (!(d = find(id))) {
        if (fd < 0 || !readable(timeout_ms) || take_one()) return ST_DOWN;
    }
    for (pp = &got; *pp != d; pp = &(*pp)->next) ;
    *pp = d->next;
    st = d->st;
    if (st == ST_OK && d->olen > cap) st = ST_TOOBIG;
    else if (st == ST_OK) { memcpy(out, d->out, d->olen); *olen = d->olen; }
    if (d->out) { memset(d->out, 0, d->olen); free(d->out); }   /* crypto results do not linger (FR-2) */
    free(d);
    return st;
}
