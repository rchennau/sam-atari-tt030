#include "runner.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static pid_t child_pid = -1;
static int pipe_fd = -1;
static int child_running = 0;

static int is_valid_pkg_name(const char *name)
{
    if (!name || *name == '\0') return 0;
    /* a name must not start with '-': "--build" or "--nodeps" would reach yum as an option */
    if (!((*name >= 'a' && *name <= 'z') || (*name >= 'A' && *name <= 'Z') || (*name >= '0' && *name <= '9')))
        return 0;
    for (const char *p = name; *p; p++) {
        char c = *p;
        if (!((c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') ||
              c == '.' || c == '_' || c == '+' || c == '-')) {
            return 0;
        }
    }
    return 1;
}

int runner_allowed(char *const argv[])
{
    if (!argv || !argv[0]) return 0;

    if (strcmp(argv[0], "/usr/bin/yum") == 0) {
        if (!argv[1]) return 0;

        if (strcmp(argv[1], "makecache") == 0) {
            return argv[2] == NULL ? 1 : 0;
        }

        if (strcmp(argv[1], "install") == 0 || strcmp(argv[1], "remove") == 0) {
            if (!argv[2]) return 0;
            for (int i = 2; argv[i]; i++) {
                if (!is_valid_pkg_name(argv[i])) return 0;
            }
            return 1;
        }

        if (strcmp(argv[1], "update") == 0) {
            for (int i = 2; argv[i]; i++) {
                if (!is_valid_pkg_name(argv[i])) return 0;
            }
            return 1;
        }

        return 0;
    }

    if (strcmp(argv[0], "/bin/rpm") == 0) {
        if (!argv[1] || !argv[2] || !argv[3] || argv[4] != NULL) return 0;
        if (strcmp(argv[1], "-qa") != 0) return 0;
        if (strcmp(argv[2], "--queryformat") != 0) return 0;
        if (strcmp(argv[3], "%{NAME}\t%{VERSION}-%{RELEASE}\n") != 0) return 0;
        return 1;
    }

    return 0;
}

int runner_is_running(void)
{
    return child_running;
}

int runner_start(char *const argv[])
{
    if (child_running) return -1;
    if (!runner_allowed(argv)) return -1;

    int fd[2];
    if (pipe(fd) < 0) return -1;

    pid_t pid = fork();
    if (pid < 0) {
        close(fd[0]);
        close(fd[1]);
        return -1;
    }

    if (pid == 0) {
        setpgid(0, 0);

        int devnull = open("/dev/null", O_RDONLY);
        if (devnull >= 0) {
            dup2(devnull, 0);
            if (devnull != 0) close(devnull);
        }

        dup2(fd[1], 1);
        dup2(fd[1], 2);
        close(fd[0]);
        close(fd[1]);

        execv(argv[0], argv);
        _exit(127);
    }

    setpgid(pid, pid);
    close(fd[1]);

    int flags = fcntl(fd[0], F_GETFL);
    if (flags >= 0) {
        fcntl(fd[0], F_SETFL, flags | O_NONBLOCK);
    }

    pipe_fd = fd[0];
    child_pid = pid;
    child_running = 1;
    return 0;
}

long runner_poll(char *buf, long cap)
{
    if (!child_running || pipe_fd < 0 || !buf || cap <= 0) {
        return -1;
    }

    long n = (long)read(pipe_fd, buf, (size_t)cap);
    if (n > 0) {
        return n;
    }
    if (n == 0) {
        close(pipe_fd);
        pipe_fd = -1;
        return -1;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return 0;
    }
    close(pipe_fd);
    pipe_fd = -1;
    return -1;
}

int runner_finish(void)
{
    if (!child_running && child_pid < 0) {
        return -1;
    }

    if (pipe_fd >= 0) {
        close(pipe_fd);
        pipe_fd = -1;
    }

    int st = 0;
    if (child_pid > 0) {
        waitpid(child_pid, &st, 0);
    }
    child_running = 0;
    child_pid = -1;

    if (WIFSIGNALED(st)) {
        return 1000 + WTERMSIG(st);
    }
    if (WIFEXITED(st)) {
        return WEXITSTATUS(st);
    }
    return st;
}

void runner_cancel(void)
{
    if (child_running && child_pid > 0) {
        killpg(child_pid, SIGTERM);
    }
}
