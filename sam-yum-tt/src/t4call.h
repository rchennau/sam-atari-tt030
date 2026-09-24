/* t4call — the shared TT-side T425 runtime (tt030-t425-kernel-ports FR-2, 2026-09-24).
 *
 * One call per kernel invocation. The first call boots BTL (a server built on t4serv.c) under
 * /tmp/t425.lock; later calls reuse it. Any failure returns -1 with *why set, and every later call in
 * the process returns -1 at once (the link may be out of step), so the caller runs the same kernel on
 * the 68030. Wire format and lock rule: maestro/tracks/tt030-t425-kernel-ports/plan.md, Eng §2.
 */
#ifndef T4CALL_H
#define T4CALL_H

/* Run op/level on n bytes of in on the T425; result into out (cap bytes). Returns its length, or -1. */
long t4call(const char *btl, int op, int level, const void *in, long n, void *out, long cap,
            const char **why);

/* Lock helpers, exposed for tests and for Dropbear (FR-6). t4lock_take: 1 = held by us,
 * 0 = held by a live process (*why says which), -1 = could not create. */
int t4lock_take(const char **why);
void t4lock_release(void);

#endif
