/* pipespike — tt030-yum-gui Phase 2 spike: can a 100 ms event loop run a child in its own process group, read its
 * output through a non-blocking pipe without losing a byte, see EOF + the exit code, and stop it with killpg()?
 *   pipespike CASE          CASE: big (child writes 256 KB), sleep (child blocks 60 s; we kill it at 3 s),
 *                                 kill (child writes forever; killpg at 2 s)
 * Prints "case bytes exit_or_signal max_tick_gap_ms" — the loop's largest gap between 100 ms ticks is the NFR-2 proxy.
 * ponytail: usleep stands in for evnt_multi's MU_TIMER; ttpkg replaces it with the AES call once this holds. */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <unistd.h>

static long now_ms(void)                 /* wall time: clock() is CPU time on Linux, so a sleeping child never timed out */
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec * 1000L + tv.tv_usec / 1000;
}

int main(int argc, char **argv)
{
    const char *c = argc > 1 ? argv[1] : "big";
    int fd[2], st = 0, done = 0;
    long bytes = 0, t0, last, gap = 0, kill_at = !strcmp(c, "sleep") ? 3000 : !strcmp(c, "kill") ? 2000 : -1;
    char buf[4096];
    pid_t pid;

    if (pipe(fd) < 0) { perror("pipe"); return 2; }
    if ((pid = fork()) == 0) {
        setpgid(0, 0);
        dup2(fd[1], 1); close(fd[0]); close(fd[1]);
        if (!strcmp(c, "big"))   execl("/bin/sh", "sh", "-c", "head -c 262144 /bin/bash", (char *)0);
        if (!strcmp(c, "sleep")) execl("/bin/sh", "sh", "-c", "sleep 60", (char *)0);
        execl("/bin/sh", "sh", "-c", "while :; do cat /bin/sh; done", (char *)0);
        _exit(127);
    }
    setpgid(pid, pid);                       /* both sides, so killpg works whoever runs first */
    close(fd[1]);
    fcntl(fd[0], F_SETFL, fcntl(fd[0], F_GETFL) | O_NONBLOCK);
    t0 = last = now_ms();
    while (!done) {
        long n, t = now_ms();
        if (t - last > gap) gap = t - last;
        last = t;
        while ((n = read(fd[0], buf, sizeof buf)) > 0)
            bytes += n;
        if (n == 0)
            done = 1;                        /* EOF: every writer in the group has exited */
        else if (errno != EAGAIN) { perror("read"); return 2; }
        if (kill_at >= 0 && t - t0 >= kill_at) { killpg(pid, SIGTERM); kill_at = -1; }
        if (!done) usleep(100000);
    }
    waitpid(pid, &st, 0);
    printf("%s %ld %s%d %ld\n", c, bytes, WIFSIGNALED(st) ? "sig" : "exit", WIFSIGNALED(st) ? WTERMSIG(st) : WEXITSTATUS(st),
           gap);
    return 0;
}
