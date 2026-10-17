/* ttsaver — screensaver for the ATW800/2 (PMS tt030-atw-display-tuning FR-5).
 * After N idle minutes it switches the card's video timing generator off (ctrl_reg bit a = 0), so the monitor sleeps;
 * the first key, mouse move or mouse button switches it back on. Idle = no change in the IKBD buffer tail index and
 * the Line-A mouse position/buttons, polled every 0.25 s; polling only reads, so no keystroke is swallowed.
 * Serial traffic does not wake the screen (automation over Modem 2 needs no picture).
 * The mode's ctrl_reg comes from the driver's VTG shadow (ATWInfo + 32), so it works in every mode.
 * Usage: ttsaver [-m MINUTES] [-s SECONDS]   default 10 minutes; -s for testing.
 * Installed as C:\TTSAVER.PRG (a .TTP would make XaAES open a toswin2 window for it).
 * Build: m68k-atari-mint-gcc -m68020-60 -O2 -s -o ttsaver.prg src/atwbg/ttsaver.c
 */
#include <mint/osbind.h>
#include <mint/linea.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CTRL (*(volatile unsigned short *)0xFEDFF800UL)        /* VTG control register (write-only) */
#define SHADOW (*(volatile unsigned short *)0xFEDFDC20UL)      /* driver's copy of the mode's ctrl_reg */

static unsigned short on_val;
static int off;

static long vtg_off(void) { on_val = SHADOW; CTRL = on_val & ~0x01; return 0; }
static long vtg_on(void) { CTRL = on_val; return 0; }

int main(int argc, char **argv)
{
	long limit = 600, idle = 0;
	_IOREC *kb = (_IOREC *)Iorec(1);
	short tail, mx, my, mb, t, x, y, b;
	int i;
	for (i = 1; i + 1 < argc; i += 2) {
		if (!strcmp(argv[i], "-m")) limit = atol(argv[i + 1]) * 60;
		if (!strcmp(argv[i], "-s")) limit = atol(argv[i + 1]);
	}
	linea0();
	tail = kb->ibuftl; mx = GCURX; my = GCURY; mb = MOUSE_BT;
	if (isatty(1)) {                                     /* silent when started from XaAES autorun (.PRG, no window) */
		printf("ttsaver: blank after %ld s idle\n", limit);
		fflush(stdout);
	}
	for (;;) {
		usleep(250000);
		t = kb->ibuftl; x = GCURX; y = GCURY; b = MOUSE_BT;
		if (t != tail || x != mx || y != my || b != mb) {
			tail = t; mx = x; my = y; mb = b;
			idle = 0;
			if (off) { Supexec(vtg_on); off = 0; }
		} else if (!off && ++idle >= limit * 4) {
			Supexec(vtg_off);
			off = 1;
		}
	}
}
