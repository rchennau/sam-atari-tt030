/* ttlease — run a program that boots its own T425 image (NetHack-ATW's t8gem host, af4039be) while ttjobd keeps
 * the lock: take ttjobd's lease (OP_LEASE), run the program as a child and wait for it. The lease is this process's
 * ttjobd connection: it closes when ttlease exits (after the program, or killed), and ttjobd then re-boots its job
 * image. Not exec: MiNT closed the socket across execv, so the lease ended at once (swap test 2026-10-18). Meanwhile SSH crypto and other jobs
 * run on the 68030. No ttjobd: the program runs anyway (nothing else owns the T425 then, except a Dropbear login).
 * Usage: ttlease PROGRAM [ARGS...]   exit 3 = another lease holds the T425 */
#include "libttjob.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
int main(int argc, char **argv)
{
    int id, st, ws;
    pid_t pid;
    long olen;
    char b[1];
    if (argc < 2) { fprintf(stderr, "usage: ttlease PROGRAM [ARGS...]\n"); return 2; }
    if ((id = ttjob_submit(OP_LEASE, 0, "", 0)) < 0)
        fprintf(stderr, "ttlease: ttjobd not reachable; running %s without a lease\n", argv[1]);
    else if ((st = ttjob_wait(id, b, 0, &olen, 10000)) != ST_OK) {
        fprintf(stderr, "ttlease: T425 busy (lease refused, status %d)\n", st);
        return 3;
    }
    if ((pid = fork()) == 0) { execv(argv[1], argv + 1); perror(argv[1]); _exit(127); }
    if (pid < 0) { perror("ttlease: fork"); return 127; }
    while (waitpid(pid, &ws, 0) < 0) if (errno != EINTR) return 127;
    return WIFEXITED(ws) ? WEXITSTATUS(ws) : 128 + WTERMSIG(ws);
}
