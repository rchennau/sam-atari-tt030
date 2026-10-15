/* runcli — NFR-5 test driver for the ttpkg runner (tt030-yum-gui): run an allowed yum command through runner.c and,
 * with --cancel-at TEXT, killpg the group the moment TEXT appears in its output (e.g. yum's "installing" line, i.e.
 * while rpm runs). TEST TOOL ONLY: the GUI decides by exit code and never reads message text.
 *   runcli [--cancel-at TEXT] /usr/bin/yum VERB [PKG...]     prints the output, then "outcome N" (1015 = SIGTERM) */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "runner.h"

int main(int argc, char **argv)
{
    const char *at = NULL;
    static char all[65536];
    char buf[4096];
    long n, len = 0;
    int i = 1, cancelled = 0;
    if (argc > 2 && !strcmp(argv[1], "--cancel-at")) { at = argv[2]; i = 3; }
    if (runner_start(argv + i) < 0) { fprintf(stderr, "runcli: refused\n"); return 2; }
    for (;;) {
        while ((n = runner_poll(buf, sizeof buf - 1)) > 0) {
            fwrite(buf, 1, (size_t)n, stdout);
            fflush(stdout);
            if (len + n < (long)sizeof all) { memcpy(all + len, buf, (size_t)n); len += n; all[len] = 0; }
        }
        if (n < 0) break;
        if (at && !cancelled && strstr(all, at)) { runner_cancel(); cancelled = 1; printf("\n[runcli: cancelled]\n"); }
        usleep(100000);
    }
    printf("outcome %d\n", runner_finish());
    return 0;
}
