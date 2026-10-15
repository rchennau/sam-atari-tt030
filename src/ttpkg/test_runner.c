#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "runner.h"

int main(void)
{
    /* 1. Valid forms accepted by runner_allowed */
    {
        char *cmd[] = {"/usr/bin/yum", "install", "bash", NULL};
        assert(runner_allowed(cmd) == 1);
    }
    {
        char *cmd[] = {"/usr/bin/yum", "install", "pkg-1.0+mint_2.3", "nano", NULL};
        assert(runner_allowed(cmd) == 1);
    }
    {
        char *cmd[] = {"/usr/bin/yum", "update", "bash", NULL};
        assert(runner_allowed(cmd) == 1);
    }
    {
        char *cmd[] = {"/usr/bin/yum", "update", NULL};
        assert(runner_allowed(cmd) == 1);
    }
    {
        char *cmd[] = {"/usr/bin/yum", "remove", "bash", NULL};
        assert(runner_allowed(cmd) == 1);
    }
    {
        char *cmd[] = {"/usr/bin/yum", "remove", "pkg1", "pkg2", NULL};
        assert(runner_allowed(cmd) == 1);
    }
    {
        char *cmd[] = {"/usr/bin/yum", "makecache", NULL};
        assert(runner_allowed(cmd) == 1);
    }
    {
        char *cmd[] = {
            "/bin/rpm",
            "-qa",
            "--queryformat",
            "%{NAME}\t%{VERSION}-%{RELEASE}\n",
            NULL
        };
        assert(runner_allowed(cmd) == 1);
    }

    /* 2. Refuses bad forms (at least 8 distinct required categories) */

    /* Bad 1: other binary */
    {
        char *cmd[] = {"/bin/sh", "-c", "echo hello", NULL};
        assert(runner_allowed(cmd) == 0);
    }
    {
        char *cmd[] = {"/usr/bin/apt-get", "install", "bash", NULL};
        assert(runner_allowed(cmd) == 0);
    }

    /* Bad 2: other verb */
    {
        char *cmd[] = {"/usr/bin/yum", "search", "bash", NULL};
        assert(runner_allowed(cmd) == 0);
    }
    {
        char *cmd[] = {"/usr/bin/yum", "list", NULL};
        assert(runner_allowed(cmd) == 0);
    }

    /* Bad 3: rpm -e */
    {
        char *cmd[] = {"/bin/rpm", "-e", "bash", NULL};
        assert(runner_allowed(cmd) == 0);
    }

    /* Bad 4: package name with ';' */
    {
        char *cmd[] = {"/usr/bin/yum", "install", "bash;reboot", NULL};
        assert(runner_allowed(cmd) == 0);
    }

    /* Bad 5: package name with space */
    {
        char *cmd[] = {"/usr/bin/yum", "install", "bash extra", NULL};
        assert(runner_allowed(cmd) == 0);
    }

    /* Bad 6: package name with '$(' */
    {
        char *cmd[] = {"/usr/bin/yum", "install", "$(reboot)", NULL};
        assert(runner_allowed(cmd) == 0);
    }

    /* Bad 7: empty install */
    {
        char *cmd[] = {"/usr/bin/yum", "install", NULL};
        assert(runner_allowed(cmd) == 0);
    }
    {
        char *cmd[] = {"/usr/bin/yum", "install", "", NULL};
        assert(runner_allowed(cmd) == 0);
    }

    /* Bad 8: extra rpm args */
    {
        char *cmd[] = {
            "/bin/rpm",
            "-qa",
            "--queryformat",
            "%{NAME}\t%{VERSION}-%{RELEASE}\n",
            "extra_arg",
            NULL
        };
        assert(runner_allowed(cmd) == 0);
    }

    /* Additional edge cases */
    {
        char *cmd[] = {"/usr/bin/yum", "remove", NULL};
        assert(runner_allowed(cmd) == 0);
    }
    {
        char *cmd[] = {"/usr/bin/yum", "makecache", "extra", NULL};
        assert(runner_allowed(cmd) == 0);
    }
    {
        char *cmd[] = {"/bin/rpm", "-qa", NULL};
        assert(runner_allowed(cmd) == 0);
    }
    {
        char *cmd[] = {"/bin/rpm", "-qa", "--queryformat", "%{NAME}", NULL};
        assert(runner_allowed(cmd) == 0);
    }
    {
        assert(runner_allowed(NULL) == 0);
    }
    {
        char *cmd[] = {NULL};
        assert(runner_allowed(cmd) == 0);
    }

    /* 3. Test runner_start with refused argv (must return -1, refused before starting process) */
    {
        char *bad1[] = {"/bin/sh", "-c", "echo bad", NULL};
        assert(runner_start(bad1) == -1);
    }
    {
        char *bad2[] = {"/usr/bin/yum", "install", NULL};
        assert(runner_start(bad2) == -1);
    }
    {
        char *bad3[] = {"/usr/bin/yum", "install", "bad;cmd", NULL};
        assert(runner_start(bad3) == -1);
    }
    {
        char *bad4[] = {"/bin/rpm", "-e", "bash", NULL};
        assert(runner_start(bad4) == -1);
    }
    {
        assert(runner_start(NULL) == -1);
    }
    {
        /* Ensure no child is running after refused starts */
        assert(runner_is_running() == 0);
        char buf[64];
        assert(runner_poll(buf, sizeof(buf)) == -1);
        assert(runner_finish() == -1);
    }

        {   /* option injection through a package name */
        char *b1[] = {"/usr/bin/yum", "install", "--build", "x", NULL};
        char *b2[] = {"/usr/bin/yum", "install", "--nodeps", NULL};
        char *b3[] = {"/usr/bin/yum", "remove", "-x", NULL};
        char *b4[] = {"/usr/bin/yum", "update", "-y", NULL};
        assert(!runner_allowed(b1) && !runner_allowed(b2) && !runner_allowed(b3) && !runner_allowed(b4));
    }
    printf("test_runner: all assertions passed.\n");
    return 0;
}
