/* t4call — the shared TT-side T425 runtime (tt030-t425-kernel-ports FR-2, 2026-09-24).
 *
 * One call per kernel invocation. The first call boots BTL (a server built on t4serv.c) under
 * /tmp/t425.lock; later calls reuse it. Any failure returns -1 with *why set, and every later call in
 * the process returns -1 at once (the link may be out of step), so the caller runs the same kernel on
 * the 68030. Wire format and lock rule: maestro/tracks/tt030-t425-kernel-ports/plan.md, Eng §2.
 */
#ifndef T4CALL_H
#define T4CALL_H

/* How long t4call waits for a kernel's reply (ms). 2 minutes suits kernels that take seconds; a caller
 * whose kernel runs for minutes (tppmquant: ~5-10 min) raises it before its first call. */
extern long t4call_timeout_ms;

/* Run op/level on n bytes of in on the T425; result into out (cap bytes). Returns its length, or -1. */
long t4call(const char *btl, int op, int level, const void *in, long n, void *out, long cap,
            const char **why);

/* 1 if /etc/t425/enabled/PKG exists: the switch a package's `<pkg>-t425-on` RPM installs once the
 * package passed the FR-5 gate. A T425 build calls t4call() only when this is true, so the binary
 * that was measured is the binary that ships, and enabling it needs no rebuild (plan FR-3). */
int t4_enabled(const char *pkg);

#include "t4adler.h"                             /* t4_adler32 */

#include "t4lock.h"                              /* t4lock_take / t4lock_release / t4_loaded_* */

#endif
