/* vtgprobe — display-tuning FR-5 probe: switch the ATW800/2 video timing generator off and back on.
 * Usage: vtgprobe CTRL [SECS]   CTRL = the mode's ctrl_reg (mode 32: 0x1d); clears bit a (VTG enable) for SECS (5),
 *                               then writes CTRL back. Prints the register and the driver's VTG shadow first.
 *        vtgprobe -r CTRL       recovery: write CTRL with bit d (PLL load) toggled 0 -> 1, as the driver's mode set does
 * VTG registers at 0xFEDFF800 (Programmer's Manual memory map, TT); ATWInfo VTG shadow at 0xFEDFDC00 + 32.
 * Build: m68k-atari-mint-gcc -m68020-60 -O2 -s -o vtgprobe.ttp src/atwbg/vtgprobe.c
 */
#include <mint/osbind.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CTRL (*(volatile unsigned short *)0xFEDFF800UL)
static unsigned short ctrl, val, shadow[16];
static int i;

static long rd(void) { ctrl = CTRL; for (i = 0; i < 16; i++) shadow[i] = ((volatile unsigned short *)0xFEDFDC20UL)[i]; return 0; }
static long wr(void) { CTRL = val; return 0; }

int main(int argc, char **argv)
{
	int rec = argc > 2 && !strcmp(argv[1], "-r");
	unsigned short c = (unsigned short)strtol(argv[rec ? 2 : 1], NULL, 0);
	int secs = !rec && argc > 2 ? atoi(argv[2]) : 5;
	if (argc < 2) { printf("usage: vtgprobe CTRL [SECS] | -r CTRL\n"); return 2; }
	Supexec(rd);
	printf("vtgprobe: ctrl reads %04x; shadow", ctrl);
	for (i = 0; i < 16; i++) printf(" %04x", shadow[i]);
	printf("\n");
	if (rec) {
		val = c & ~0x08; Supexec(wr);
		val = c; Supexec(wr);
		printf("vtgprobe: recovery: wrote %04x then %04x\n", c & ~0x08, c);
		return 0;
	}
	val = c & ~0x01; Supexec(wr);
	printf("vtgprobe: VTG off (%04x) for %d s\n", val, secs);
	fflush(stdout);
	sleep(secs);
	val = c; Supexec(wr);
	printf("vtgprobe: VTG on (%04x)\n", val);
	return 0;
}
