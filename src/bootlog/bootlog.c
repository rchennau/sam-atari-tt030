/* bootlog — make the FreeMiNT boot console smaller and dark (PMS tt030-atw800-video-modes FR-9).
 * Run from C:\AUTO after the xVDI driver: switches the VT52 console to the Linux VGA 8x8 font (font8x8.h) through the Line-A
 * variables and swaps its foreground/background colours (white text on black), then clears the screen.
 * Writes what it found and set to C:\BOOTLOG.TXT so a remote check can tell whether it ran and what the console was.
 * Off = rename BOOTLOG.PRG to BOOTLOG.PRX (scripts/tt_bootlog.sh).
 * Capture (operator 2026-10-17: the boot text reached no log): it also hooks the BIOS console vector (xconout[2], 0x586)
 * and copies every character printed after it into a Malloc'd buffer (struct blog, BLOG_CAP bytes, stops when full);
 * its address goes into BOOTLOG.TXT as "log 0x...". /usr/sbin/bootdump.prg (bootdump.c, XaAES autorun) writes it to
 * /var/log/boot.log and syslog. ponytail: output before BOOTLOG.PRG runs and GEM alert boxes are not captured, and a
 * boot that hangs loses the buffer (the serial tee is the upgrade). -DBLOG_TEST builds a runtime self-check. */
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/linea.h>
#include <stdio.h>
#include <string.h>
#include "font8x8.h"
#include "blog.h"

static volatile struct blog *blog;
long old_conout __asm__("blog_old");
void blog_put(long c) __asm__("blog_put");
void blog_put(long c) { if (blog->len < blog->cap) blog->data[blog->len++] = (char)c; }
/* xconout handlers get the character as a word at 6(sp); save the C scratch registers, record it, chain to the old one */
extern char blog_hook[] __asm__("blog_hook");
__asm__("blog_hook:\n\tmovem.l %d0-%d2/%a0-%a2,-(%sp)\n\tmoveq #0,%d0\n\tmove.w 30(%sp),%d0\n\tmove.l %d0,-(%sp)\n"
        "\tjsr blog_put\n\taddq.l #4,%sp\n\tmovem.l (%sp)+,%d0-%d2/%a0-%a2\n\tmove.l blog_old,-(%sp)\n\trts\n");
#define XCONOUT2 (*(volatile long *)0x586L)
static long hook_on(void) { old_conout = XCONOUT2; XCONOUT2 = (long)blog_hook; return 0; }
static long __attribute__((unused)) hook_off(void) { XCONOUT2 = old_conout; return 0; }

static unsigned char *keep;                       /* Malloc'd copy of the font: survives Ptermres */
static short ht0, my0, fg0, bg0, ht1, my1, fg1, bg1, ok;

static long __attribute__((unused)) apply(void)
{
    short t;
    ht0 = V_CEL_HT; my0 = V_CEL_MY; fg0 = V_COL_FG; bg0 = V_COL_BG;
    if (keep) {                                   /* the console reads the font for as long as it runs */
        V_FNT_AD = (char *)keep + 1;                /* this console indexes char - 1 (operator photo 2026-10-16:
                                                     every glyph showed one code low with keep + 0) */
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

#ifdef BLOG_TEST
int main(void)                                    /* hook, print a marker through the BIOS, unhook, compare */
{
    static struct blog b = { BLOG_MAGIC, 0, BLOG_CAP };
    blog = &b;
    Supexec(hook_on);
    { FILE *c = fopen("/dev/console", "w"); if (c) { fputs("blog-test 123\r\n", c); fclose(c); } }   /* console -> bconout(2) -> xconout[2]; a process's own Bconout(2) goes to its tty */
    Supexec(hook_off);
    printf("captured %ld bytes: %s\n", b.len, b.len >= 13 && !memcmp(b.data, "blog-test 123", 13) ? "PASS" : "FAIL");
    return b.len < 13;
}
#else
int main(void)
{
    FILE *f;
    int i;
    linea0();
    if ((blog = (struct blog *)Malloc(sizeof *blog))) {
        blog->magic = BLOG_MAGIC; blog->len = 0; blog->cap = BLOG_CAP;
        Supexec(hook_on);
    }
    if ((keep = (unsigned char *)Malloc(sizeof linux_8x8 + 1))) {   /* + 1: the last glyph row reads one past */
        for (i = 0; i < (int)sizeof linux_8x8; i++)
            keep[i] = linux_8x8[i];
        keep[i] = 0;
    }
    Supexec(apply);
    Cconws("\033E");                              /* clear to the new background, cursor home */
    if ((f = fopen("C:\\BOOTLOG.TXT", "w"))) {
        fprintf(f, "Linux VGA 8x8 %s; cell height %d -> %d, rows %d -> %d, fg %d -> %d, bg %d -> %d, screen %dx%d\n",
                ok ? "set" : "NOT FOUND", ht0, ht1, my0 + 1, my1 + 1, fg0, fg1, bg0, bg1, V_X_MAX + 1, V_Y_MAX + 1);
        if (blog) fprintf(f, "log 0x%lx\n", (long)blog);
        fclose(f);
    }
    Cconws("bootlog: Linux VGA 8x8 font, white on black\r\n");
    /* keep text + data + bss: the console hook and blog_old live there (256 alone freed them); Malloc'd blocks stay */
    Ptermres(256 + _base->p_tlen + _base->p_dlen + _base->p_blen, 0);
    return 0;
}
#endif
