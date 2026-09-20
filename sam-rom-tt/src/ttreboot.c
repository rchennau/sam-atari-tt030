/* ttreboot — restart the TT from a shell. FreeMiNT on this machine ships no reboot/halt binary
 * (/sbin holds only tzinit, measured 2026-09-19), which is why `reboot` over SSH silently did
 * nothing. Clears the memory-valid markers so the restart takes the full power-on path, then jumps
 * through the ROM's reset vector — the same path ttrun uses to end an unattended run.
 *
 * Build: m68k-atari-mint-gcc -m68020-60 -O2 -s -o ttreboot.prg src/ttreboot.c
 */
#include <mint/osbind.h>
#include <stdio.h>
#include <unistd.h>

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
    printf("ttreboot: resetting\n");
    fflush(stdout);
    sync();                     /* flush caches before the reset */
    Supexec(do_reset);
    printf("ttreboot: reset returned - still running\n");
    return 1;
}
