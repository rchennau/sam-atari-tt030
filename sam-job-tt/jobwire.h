/* jobwire — tt030-transputer-job-queue wire format (plan Eng §2), shared by the 68030 and transputer sides.
 * All integers little-endian. Phase 1: synchronous, one job in flight; `node` picks the executor explicitly
 * (0 = T425, 1 = TRAM 1, 2 = TRAM 2) until FR-6's dispatch rule replaces it in Phase 4.
 *   request  id (2) | op (1) | node (1) | len (4) | adler32(payload) (4) | payload
 *   reply    id (2) | status (1) | outlen (4) | adler32(request payload) (4) | adler32(out) (4) | out      */
#ifndef JOBWIRE_H
#define JOBWIRE_H
#define JOB_HDR 12
#define REP_HDR 15
#define JOB_MAX 32768L                 /* phase 1 payload cap per job */
#define OP_ECHO 'E'
#define OP_SHA256 'S'
#define ST_OK 0
#define ST_BADREQ 1                    /* request checksum failed or malformed */
#define ST_TOOBIG 2
#define ST_DOWN 3                      /* network not available: run it on the 68030 */
#define ST_BADOP 4
#define ST_NONODE 5
#endif
