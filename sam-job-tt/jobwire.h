/* jobwire — tt030-transputer-job-queue wire format (plan Eng §2), shared by the 68030 and transputer sides.
 * All integers little-endian. Phase 1: synchronous, one job in flight; `node` picks the executor explicitly
 * (0 = T425, 1 = TRAM 1, 2 = TRAM 2) until FR-6's dispatch rule replaces it in Phase 4.
 *   request  id (2) | op (1) | node (1) | level (1) | len (4) | adler32(payload) (4) | payload   (Phase 2: +level)
 *   reply    id (2) | status (1) | outlen (4) | adler32(request payload) (4) | adler32(out) (4) | out
 *   Phase 4: node 0xFF in a request = any idle node (the dispatcher picks). On the transputer LINK the reply header
 *   has one more byte, the node that ran it (LREP_HDR); ttjobd logs it and strips it, so the client protocol is
 *   unchanged (installed Dropbear / tgzip expect 15 bytes - a 16-byte client header broke every login, 2026-10-13). */
#ifndef JOBWIRE_H
#define JOBWIRE_H
#define JOB_HDR 13
#define H_LEVEL 4
#define H_LEN 5
#define H_ADLER 9
#define REP_HDR 15                     /* client protocol */
#define LREP_HDR 16                    /* link protocol: + node */
#define R_NODE 15
#define NODE_ANY 0xFF
#define NODES 3
#define JOB_MAX (400L * 1024)          /* payload and output cap per job: tgzip pieces are 256 KB in, <= 385 KB out */
#define OP_ECHO 'E'
#define OP_SHA256 'S'
#define OP_X25519 'X'                  /* Phase 3 crypto ops, every node (integer only) */
#define OP_ED25519V 'V'
#define OP_CHACHA20 'C'
/* compression (T425 node only, compkern.h): 'z' deflate, 'g' gzip, 'r' raw, 'i' inflate, 'b' bzip2 */
#define ST_OK 0
#define ST_BADREQ 1                    /* request checksum failed or malformed */
#define ST_TOOBIG 2
#define ST_DOWN 3                      /* network not available: run it on the 68030 */
#define ST_BADOP 4
#define ST_NONODE 5
#define ST_FAILED 6                    /* the kernel itself failed (e.g. bzip2 -9 needs more than the T425 heap) */
#endif
