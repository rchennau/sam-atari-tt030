/* atwx25519 — offload Dropbear's X25519 to the ATW800/2's T425 transputer (kanban a764de13 step 2).
 *
 * Measured on the real TT (2026-09-13): 2,250 ms per X25519 round trip on the T425 vs 4,065 ms for
 * Monocypher on the 68030. Dropbear does two per login (kex-x25519.c: ephemeral key, shared secret).
 *
 * Link access: fpgabios.tos's private GEMDOS opcodes on trap #1 (from iserver's hardst.s):
 *   100 presence (arg 0x17 -> 0x17), 112 reset T425, 105 block write (buf, len), 106 block read (buf, len).
 * The server image (src/x25519bench/xserv.btl in the work repo) is booted raw: reset, write the image,
 * answer the INMOS runtime's start-up requests the way iserver does, wait for "RDY0". Then per call:
 * 64 bytes out (scalar || u), 32 bytes back. Test harness with the same logic: src/x25519bench/xclient.c.
 *
 * Any failure (no fpgabios, no image, no reply) marks the offload failed for this process and the caller
 * falls back to Monocypher on the 68030. DROPBEAR_NO_ATW (set to anything) disables it, for A/B timing.
 * ponytail: boots the T425 once per Dropbear process (one per login, ~1.2 s); upgrade path is a server
 * booted at system start plus a liveness check. Two simultaneous logins would contend for the one link;
 * the loser times out and falls back. MiNT only: other builds get a stub that always declines.
 */
#ifdef __MINT__
#include "dbutil.h"                     /* dropbear_log */
#include <mint/osbind.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ATW_IMAGE "/etc/dropbear/xserv.btl"
#define ATW_OP_CHECK 100
#define ATW_OP_WRITE 105
#define ATW_OP_READ 106
#define ATW_OP_RESET 112

static int atw_state;                  /* 0 untried, 1 serving, -1 failed: stay on the 68030 */

/* The driver may send fewer bytes than asked (even 0); iserver's WriteLink loops on the count. */
static long atw_write(const unsigned char *buf, long len)
{
    long done = 0;
    clock_t end = clock() + 5 * CLOCKS_PER_SEC;
    while (done < len) {
        long n = len - done > 0x7000 ? 0x7000 : len - done;
        long r = trap_1_wll(ATW_OP_WRITE, (long)(buf + done), n);
        if (r < 0)
            return r;
        if (r > 0) {
            done += r;
            end = clock() + 5 * CLOCKS_PER_SEC;
        } else if (clock() > end)
            break;
    }
    return done;
}

static long atw_read(unsigned char *buf, long len, long timeout_ms)
{
    long got = 0;
    clock_t end = clock() + timeout_ms * CLOCKS_PER_SEC / 1000;
    while (got < len) {
        long r = trap_1_wll(ATW_OP_READ, (long)(buf + got), len - got);
        if (r > 0)
            got += r;
        if (clock() > end)
            break;
    }
    return got;
}

/* Reset the T425, boot the server, answer start-up requests until it sends "RDY0". 0 on success. */
static int atw_boot(const char **why)
{
    unsigned char *img, pkt[1024], rep[16];
    long imglen, len, rl;
    FILE *f;

    if ((short)trap_1_ww(ATW_OP_CHECK, 0x17) != 0x17)
        return *why = "fpgabios.tos not resident", -1;
    if (!(f = fopen(ATW_IMAGE, "rb")))
        return *why = "no " ATW_IMAGE, -1;
    fseek(f, 0, SEEK_END);
    imglen = ftell(f);
    rewind(f);
    img = malloc(imglen);
    if (!img || fread(img, 1, imglen, f) != (size_t)imglen) {
        fclose(f);
        free(img);
        return *why = "cannot read " ATW_IMAGE, -1;
    }
    fclose(f);
    trap_1_w(ATW_OP_RESET);
    len = atw_write(img, imglen);
    free(img);
    if (len != imglen)
        return *why = "boot image write failed", -1;

    for (;;) {                             /* INMOS runtime start-up: SP_GETENV, SP_ID, SP_COMMAND x2 */
        if (atw_read(pkt, 2, 5000) != 2)
            return *why = "no start-up request from the T425", -1;
        if (pkt[0] == 'R' && pkt[1] == 'D')    /* "RDY0": main() is running */
            return atw_read(pkt + 2, 2, 2000) == 2 ? 0 : (*why = "truncated ready marker", -1);
        len = pkt[0] | (pkt[1] << 8);
        if (len < 1 || len > (long)sizeof pkt || atw_read(pkt, len, 2000) != len)
            return *why = "bad start-up packet", -1;
        rl = 1;
        rep[2] = 1;                        /* SP_UNIMPLEMENTED unless handled below */
        if (pkt[0] == 32 && len >= 13 && pkt[1] == 10 && !memcmp(pkt + 3, "IBOARDSIZE", 10)) {
            static const char board[] = "#200000";     /* 2 MB, the vendor profile's value */
            rep[2] = 0;
            rep[3] = sizeof board - 1;
            rep[4] = 0;
            memcpy(rep + 5, board, sizeof board - 1);
            rl = 3 + sizeof board - 1;
        } else if (pkt[0] == 32) {
            rep[2] = 129;                  /* SP_ERROR: variable not set */
        } else if (pkt[0] == 40) {         /* SP_COMMAND: empty command line */
            rep[2] = rep[3] = rep[4] = 0;
            rl = 3;
        } else if (pkt[0] == 42) {         /* SP_ID */
            rep[2] = rep[3] = rep[4] = rep[5] = rep[6] = 0;
            rl = 5;
        }
        rep[0] = (unsigned char)rl;
        rep[1] = (unsigned char)(rl >> 8);
        if (atw_write(rep, rl + 2) != rl + 2)
            return *why = "start-up reply write failed", -1;
    }
}

/* q = X25519(n, p) on the T425. Returns 0 on success, -1 if the caller must compute it itself. */
static int atw_x25519(unsigned char *q, const unsigned char *n, const unsigned char *p)
{
    unsigned char req[64], out[32];
    const char *why = NULL;

    if (atw_state == 0) {
        if (getenv("DROPBEAR_NO_ATW"))
            why = "disabled by DROPBEAR_NO_ATW";
        atw_state = why || atw_boot(&why) ? -1 : 1;
        if (atw_state > 0)
            dropbear_log(LOG_INFO, "atw: X25519 offload to the ATW800/2 T425 active");
        else
            dropbear_log(LOG_INFO, "atw: X25519 offload unavailable (%s), using the 68030", why);
    }
    if (atw_state < 0)
        return -1;
    memcpy(req, n, 32);
    memcpy(req + 32, p, 32);
    if (atw_write(req, 64) != 64 || atw_read(out, 32, 10000) != 32) {
        atw_state = -1;
        dropbear_log(LOG_WARNING, "atw: T425 did not answer, falling back to the 68030");
        return -1;
    }
    memcpy(q, out, 32);
    return 0;
}
#else
static int atw_x25519(unsigned char *q, const unsigned char *n, const unsigned char *p)
{
    (void)q; (void)n; (void)p;
    return -1;
}
#endif
