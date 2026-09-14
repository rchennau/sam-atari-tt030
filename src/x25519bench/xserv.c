/* xserv — resident X25519 server for the ATW800/2 T425 (kanban a764de13 step 2).
 *
 * Booted raw over the link by the TT (fpgabios reset + boot image, no iserver). Linked with the STANDARD
 * runtime (startup.lnk): the host-less startrd.lnk never reaches main() when booted this way (hung on the
 * TT and under t4, 2026-09-13). The standard runtime's start-up asks its host for SP_GETENV IBOARDSIZE,
 * SP_ID and SP_COMMAND (twice); the TT client answers those as iserver would. This program never calls
 * stdio afterwards, so the link is then free for the protocol below.
 *
 * The ATW800/2's link adapter is the T425's link 0 (measured: the RDYk probe answered on link 0).
 * Protocol:  T425 -> TT  "RDY0" once, when main() starts (end of start-up)
 *            per request:  TT -> T425  64 bytes = scalar (32) || u-coordinate (32), scalar unclamped
 *                          T425 -> TT  32 bytes = X25519(scalar, u), fully reduced
 * Field arithmetic: fe32/f25519.c (lmul). Ladder: Daniel Beer's c25519.c.
 */
#include <channel.h>
#include "c25519.h"

int main(void)
{
    static char ready[4] = { 'R', 'D', 'Y', '0' };
    uint8_t req[64], e[32], out[32];
    int i;

    ChanOut(LINK0OUT, ready, 4);
    for (;;) {
        ChanIn(LINK0IN, req, 64);
        for (i = 0; i < 32; i++)
            e[i] = req[i];
        e[0] &= 248;                           /* RFC 7748 clamping */
        e[31] &= 127;
        e[31] |= 64;
        c25519_smult(out, req + 32, e);
        f25519_normalize(out);
        ChanOut(LINK0OUT, out, 32);
    }
    return 0;
}
