/* libttjob — client side of ttjobd (tt030-transputer-job-queue FR-3). Submit returns at once; replies come back
 * in submit order on the caller's own connection, so results are visible only to the submitter (FR-2). */
#ifndef LIBTTJOB_H
#define LIBTTJOB_H
#include "jobwire.h"
/* Queue a job (node 0 = T425 until FR-6 dispatch). Returns a job id > 0, or -1 if ttjobd is not reachable. */
int ttjob_submit(int op, int level, const void *in, long n);
/* 1 = the reply for id has arrived, 0 = not yet, -1 = connection lost or unknown id. Never blocks. */
int ttjob_poll(int id);
/* Wait up to timeout_ms for id; copies the output (<= cap) to out and its length to *olen. Returns the jobwire
 * status (ST_OK ...), ST_DOWN on timeout or a lost connection, ST_TOOBIG if the output exceeds cap. */
int ttjob_wait(int id, void *out, long cap, long *olen, long timeout_ms);
extern const char *ttjob_socket;               /* default /var/run/ttjob.sock */
#endif
