/* ttrun — one-shot unattended experiment runner for the real TT030.
 *
 * Autostarted by the desktop (#Z line in NEWDESK.INF / EMUDESK.INF), it drives the FR-8 benchmark
 * pass without anyone at the keyboard, and — this is the point — always leaves the machine back in
 * a boot set that has SSH, so the results can be collected remotely.
 *
 *   under TOS 3.06 : run VDIBENCH.PRG, then launch EMUTOS.PRG (which restarts into RAM EmuTOS)
 *   under EmuTOS   : run VDIBENCH.PRG, restore NEWDESK.INF and the MINT_ATW boot set, then reset
 *
 * The OS is told apart by the version word in the OS header (_sysbase at 0x4F2): TOS 3.06 reports
 * 0x0306, EmuTOS reports something else.
 *
 * Every step is appended to C:\TTRUN.LOG, so a run that stops halfway says where it stopped.
 *
 * Build: m68k-atari-mint-gcc -m68020-60 -O2 -s -o ttrun.prg src/ttrun.c
 * ponytail: no argument parsing and no config — two branches, one file, deleted when FR-8 is done.
 */
#include <mint/osbind.h>
#include <stdio.h>
#include <string.h>

#define LOG      "C:\\TTRUN.LOG"
#define VDIBENCH "C:\\VDIBENCH.PRG"
#define EMUTOS   "C:\\EMUTOS.PRG"
#define CFG_LIVE "C:\\XBOOT\\XBOOT.CFG"
#define CFG_MINT "C:\\XBOOT\\XBOOT.MIN"     /* pre-built: default set = MINT_ATW */
#define INF_LIVE "C:\\NEWDESK.INF"
#define INF_SAVE "C:\\NEWDESK.SAV"          /* the desktop file without the #Z autostart */

static void logline(const char *s)
{
    FILE *f = fopen(LOG, "a");

    if (f) {
        fprintf(f, "%s\n", s);
        fclose(f);
    }
}

static long os_version(void)
{
    long base = *(long *)0x4F2L;            /* _sysbase */

    return *(unsigned short *)(base + 2);
}

static long run(const char *path)
{
    static char cmd[2] = {0, 0};            /* empty Pascal command line */

    return Pexec(0, (char *)path, cmd, 0L);
}

static int copy_file(const char *from, const char *to)
{
    char buf[1024];
    size_t n;
    FILE *s, *d;

    if (!(s = fopen(from, "rb")))
        return 0;
    if (!(d = fopen(to, "wb"))) {
        fclose(s);
        return 0;
    }
    while ((n = fread(buf, 1, sizeof buf, s)) > 0)
        if (fwrite(buf, 1, n, d) != n) {
            fclose(s);
            fclose(d);
            return 0;
        }
    fclose(s);
    fclose(d);
    return 1;
}

/* Clear the memory-valid markers so the restart runs the full power-on path, then jump through the
 * ROM's reset vector. Supervisor only — call via Supexec. */
static long do_reset(void)
{
    *(long *)0x420L = 0;
    *(long *)0x43AL = 0;
    *(long *)0x51AL = 0;
    __asm__ __volatile__("move.l 0xE00004,%%a0; jmp (%%a0)" : : : "a0");
    return 0;
}

int main(void)
{
    long ver = os_version();
    char msg[80];

    sprintf(msg, "ttrun: os version %04lx", ver);
    logline(msg);

    logline("ttrun: running vdibench");
    if (run(VDIBENCH) < 0)
        logline("ttrun: VDIBENCH.PRG failed to start");

    if (ver == 0x0306) {                    /* TOS 3.06 pass -> hand over to EmuTOS */
        logline("ttrun: launching EMUTOS.PRG (machine restarts into RAM EmuTOS)");
        if (run(EMUTOS) < 0)
            logline("ttrun: EMUTOS.PRG failed to start - stopping here");
        return 0;
    }

    logline("ttrun: EmuTOS pass done, restoring boot set MINT_ATW");
    if (!copy_file(CFG_MINT, CFG_LIVE))
        logline("ttrun: WARNING could not restore XBOOT.CFG");
    if (!copy_file(INF_SAVE, INF_LIVE))
        logline("ttrun: WARNING could not restore NEWDESK.INF");
    logline("ttrun: resetting");
    Supexec(do_reset);
    return 0;                               /* not reached */
}
