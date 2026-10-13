/* ipcbench — where does ttjobd's ~3.8 ms socket hop go? (tt030-transputer-job-queue, operator MCQ 2026-10-12)
 * 4-byte ping-pong between two processes, 400 rounds each: Unix socketpair, pipe pair, socketpair with the
 * server waiting in select() (as ttjobd does), and the pipe with the server at raised priority. No link. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/resource.h>
#include <sys/un.h>
#include <mint/mintbind.h>
static long ms_since(clock_t t0) { return (long)((clock() - t0) * 1000 / CLOCKS_PER_SEC); }
static void run(const char *name, int sock, int use_select, int prio)
{
    int a[2], b[2], i; pid_t p; char buf[4] = "ping"; clock_t t0; long ms;
    if (sock) { if (socketpair(AF_UNIX, SOCK_STREAM, 0, a) < 0) { perror("socketpair"); return; } b[0] = a[1]; b[1] = a[0]; }
    else if (pipe(a) < 0 || pipe(b) < 0) { perror("pipe"); return; }
    /* sock: parent uses a[0], child uses a[1] both ways. pipe: parent writes a[1] reads b[0]; child reads a[0] writes b[1] */
    if ((p = fork()) == 0) {
        int rfd = sock ? a[1] : a[0], wfd = sock ? a[1] : b[1];
        if (prio) setpriority(PRIO_PROCESS, 0, -20);
        for (;;) {
            if (use_select) { fd_set rs; FD_ZERO(&rs); FD_SET(rfd, &rs); if (select(rfd + 1, &rs, NULL, NULL, NULL) < 0) _exit(1); }
            if (read(rfd, buf, 4) != 4) _exit(0);
            if (write(wfd, buf, 4) != 4) _exit(0);
        }
    }
    {
        int wfd = sock ? a[0] : a[1], rfd = sock ? a[0] : b[0];
        t0 = clock();
        for (i = 0; i < 400; i++) if (write(wfd, buf, 4) != 4 || read(rfd, buf, 4) != 4) break;
        ms = ms_since(t0);
        printf("%-34s %5ld us per round-trip\n", name, ms * 1000 / 400);
        kill(p, SIGTERM); waitpid(p, NULL, 0);
        close(a[0]); close(a[1]); if (!sock) { close(b[0]); close(b[1]); }
    }
}
/* path-bound AF_UNIX socket, as ttjobd uses; yield != 0: client calls Syield() after each write */
static void run_path(const char *name, int yield, int prio)
{
    struct sockaddr_un sa; int ls, c, i; pid_t p; char buf[4] = "ping"; clock_t t0; long ms;
    const char *path = "/tmp/ipcb.sock";
    memset(&sa, 0, sizeof sa); sa.sun_family = AF_UNIX; strcpy(sa.sun_path, path);
    unlink(path); ls = socket(AF_UNIX, SOCK_STREAM, 0);
    if (bind(ls, (struct sockaddr *)&sa, sizeof sa) < 0 || listen(ls, 1) < 0) { perror("bind"); return; }
    if ((p = fork()) == 0) {
        int s = accept(ls, NULL, NULL);
        if (prio) setpriority(PRIO_PROCESS, 0, -20);
        for (;;) { if (read(s, buf, 4) != 4) _exit(0); if (write(s, buf, 4) != 4) _exit(0); if (yield) Syield(); }
    }
    c = socket(AF_UNIX, SOCK_STREAM, 0);
    if (connect(c, (struct sockaddr *)&sa, sizeof sa) < 0) { perror("connect"); return; }
    t0 = clock();
    for (i = 0; i < 400; i++) { if (write(c, buf, 4) != 4) break; if (yield) Syield(); if (read(c, buf, 4) != 4) break; }
    ms = ms_since(t0);
    printf("%-34s %5ld us per round-trip\n", name, ms * 1000 / 400);
    kill(p, SIGTERM); waitpid(p, NULL, 0); close(c); close(ls); unlink(path);
}
int main(void)
{
    run_path("path socket, blocking", 0, 0);
    run_path("path socket, Syield after write", 1, 0);
    run_path("path socket, Syield + prio -20", 1, 1);
    run("unix socketpair, blocking read", 1, 0, 0);
    run("unix socketpair, server select()", 1, 1, 0);
    run("pipe pair, blocking read", 0, 0, 0);
    run("pipe pair, server prio -20", 0, 0, 1);
    run("unix socketpair, server prio -20", 1, 1, 1);
    return 0;
}
