/* HELPSPC.PRG — make the German Ö and Ü keys (right of L and P) type a space under TOS (AUTO, resident).
 *
 * Why: the TT's space bar is dead (2026-09-12) and the operator touch-types. TOS's key tables
 * are in ROM, so this copies them to RAM, sets scancodes 0x27/0x1A to ' ' in the unshifted and
 * caps tables (Shift+Ö/Ü still type Ö/Ü), and installs the copies with Keytbl. FreeMiNT loads its own
 * C:\MINT\...\keyboard.tbl, patched the same way by scripts/make_space_keys_tbl.py.
 *
 * Build: m68k-atari-mint-gcc -m68000 -Os -s -o HELPSPC.PRG src/helpspc.c
 */
#include <osbind.h>
#include <string.h>
#include <mint/basepage.h>

static const unsigned char keys[] = { 0x27, 0x1A };

static char tab[3][128];

int main(void)
{
    _KEYTAB *k = (_KEYTAB *)Keytbl((void *)-1, (void *)-1, (void *)-1);

    memcpy(tab[0], k->unshift, 128);
    memcpy(tab[1], k->shift, 128);
    memcpy(tab[2], k->caps, 128);
    for (unsigned i = 0; i < sizeof keys; i++)
        tab[0][keys[i]] = tab[2][keys[i]] = ' ';
    Keytbl(tab[0], tab[1], tab[2]);
    Cconws("HELPSPC: Oe/Ue keys = space\r\n");
    Ptermres(256 + _base->p_tlen + _base->p_dlen + _base->p_blen, 0);
    return 0;
}
