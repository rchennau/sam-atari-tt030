/* bootlog — make the FreeMiNT boot console smaller and dark (PMS tt030-atw800-video-modes FR-9).
 * Run from C:\AUTO after the xVDI driver: switches the VT52 console to the Linux VGA 8x8 font (font8x8.h) through the Line-A
 * variables and swaps its foreground/background colours (white text on black), then clears the screen.
 * Writes what it found and set to C:\BOOTLOG.TXT so a remote check can tell whether it ran and what the console was.
 * Off = rename BOOTLOG.PRG to BOOTLOG.PRX (scripts/tt_bootlog.sh). */
#include <mint/osbind.h>
#include <mint/linea.h>
#include <stdio.h>
#include "font8x8.h"

static unsigned char *keep;                       /* Malloc'd copy of the font: survives Ptermres */
static short ht0, my0, fg0, bg0, ht1, my1, fg1, bg1, ok;

static long apply(void)
{
    short t;
    ht0 = V_CEL_HT; my0 = V_CEL_MY; fg0 = V_COL_FG; bg0 = V_COL_BG;
    if (keep) {                                   /* the console reads the font for as long as it runs */
        V_FNT_AD = (char *)keep;
        V_FNT_WR = 256;
        V_FNT_ST = 0;
        V_FNT_ND = 255;
        V_CEL_HT = 8;
        V_CEL_MY = (V_Y_MAX + 1) / 8 - 1;
        V_CEL_WR = V_BYTES_LIN * 8;
        ok = 1;
    }
    t = V_COL_FG; V_COL_FG = V_COL_BG; V_COL_BG = t;
    ht1 = V_CEL_HT; my1 = V_CEL_MY; fg1 = V_COL_FG; bg1 = V_COL_BG;
    return 0;
}

int main(void)
{
    FILE *f;
    int i;
    linea0();
    if ((keep = (unsigned char *)Malloc(sizeof linux_8x8)))
        for (i = 0; i < (int)sizeof linux_8x8; i++)
            keep[i] = linux_8x8[i];
    Supexec(apply);
    Cconws("\033E");                              /* clear to the new background, cursor home */
    if ((f = fopen("C:\\BOOTLOG.TXT", "w"))) {
        fprintf(f, "Linux VGA 8x8 %s; cell height %d -> %d, rows %d -> %d, fg %d -> %d, bg %d -> %d, screen %dx%d\n",
                ok ? "set" : "NOT FOUND", ht0, ht1, my0 + 1, my1 + 1, fg0, fg1, bg0, bg1, V_X_MAX + 1, V_Y_MAX + 1);
        fclose(f);
    }
    Cconws("bootlog: Linux VGA 8x8 font, white on black\r\n");
    Ptermres(256, 0);                             /* keep only the basepage; Malloc'd blocks stay allocated (TOS) */
    return 0;
}
