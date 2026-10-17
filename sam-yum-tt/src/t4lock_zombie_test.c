/* Board 3589fc15 acceptance on the TT: a lock held by a dead holder whose parent shell already exited must not block
 * the next taker. The card's case: ttjobd started with nohup over SSH, the SSH shell exited, then ttjobd was killed;
 * /kern/<pid> stayed (not in ps) and every later ttjobd waited forever. A zombie with a LIVE parent does not reproduce
 * it (measured 2026-10-17: the old t4lock already took that lock).
 *   t4lock_zombie_test hold   take the lock, print the pid, sleep until killed (start: nohup ... &, then exit the shell)
 *   t4lock_zombie_test take   try once; prints ZOMBIE_LOCK TAKEN (exit 0) or ZOMBIE_LOCK BUSY <why> (exit 1)
 * Build with -DT4LOCK='"/tmp/t4ztest.lock"' so the production /tmp/t425.lock is never touched. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "t4lock.h"

int main(int argc, char **argv)
{
    const char *why = "";
    int r;

    if (argc > 1 && !strcmp(argv[1], "hold")) {
        if (t4lock_take(&why) != 1) { printf("HOLD FAILED %s\n", why); return 1; }
        printf("HOLDING pid %ld\n", (long)getpid());
        fflush(stdout);
        for (;;) sleep(60);
    }
    r = t4lock_take(&why);
    printf("ZOMBIE_LOCK %s %s\n", r == 1 ? "TAKEN" : "BUSY", r == 1 ? "" : why);
    if (r == 1) t4lock_release();
    return r == 1 ? 0 : 1;
}
