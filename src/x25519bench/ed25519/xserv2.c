/* xserv2 — combined X25519 + Ed25519-verify server for the ATW800/2 T425 (kanban 3c29ce8a).
 * Booted raw over the link (see xserv.c). Same startup handshake; sends "RDY0" when main() runs.
 *
 * Request framing (versioned so the old fixed-64 X25519 protocol is no longer assumed):
 *   op 'X' (0x58): 64 bytes  = scalar(32) || u(32)                 -> reply 32 = X25519(scalar,u)
 *   op 'V' (0x56): 2 bytes N (big-endian) || sig(64) || pub(32) || msg(N)
 *                                                                  -> reply 1 = 1 valid / 0 invalid
 * Messages longer than MSG_MAX make the TT client fall back to the 68030 (it never sends them here).
 *
 * Field maths: fe32/f25519.c (lmul, constant-time). SHA-512: fe32/sha512_32.c (32-bit). Ed25519: edsign.
 */
#include <channel.h>
#include "c25519.h"
#include "edsign.h"

#define MSG_MAX 1024

int main(void)
{
    static char ready[4] = { 'R', 'D', 'Y', '0' };
    uint8_t op, xreq[64], out[32], e[32], vhdr[2], sig[64], pub[32], msg[MSG_MAX], vr[1];
    int i, n;

    ChanOut(LINK0OUT, ready, 4);
    for (;;) {
        ChanIn(LINK0IN, &op, 1);
        if (op == 'X') {
            ChanIn(LINK0IN, xreq, 64);
            for (i = 0; i < 32; i++)
                e[i] = xreq[i];
            e[0] &= 248;
            e[31] &= 127;
            e[31] |= 64;
            c25519_smult(out, xreq + 32, e);
            f25519_normalize(out);
            ChanOut(LINK0OUT, out, 32);
        } else if (op == 'V') {
            ChanIn(LINK0IN, vhdr, 2);
            n = (vhdr[0] << 8) | vhdr[1];
            if (n < 0 || n > MSG_MAX)
                n = 0;                         /* caller guarantees <= MSG_MAX; clamp defensively */
            ChanIn(LINK0IN, sig, 64);
            ChanIn(LINK0IN, pub, 32);
            if (n)
                ChanIn(LINK0IN, msg, n);
            vr[0] = edsign_verify(sig, pub, msg, n) ? 1 : 0;
            ChanOut(LINK0OUT, vr, 1);
        }
        /* an unknown op is ignored: the link stream stays byte-aligned only for known ops, so the
         * TT client must send only 'X'/'V' (it does). */
    }
    return 0;
}
