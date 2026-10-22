/* ttkbdreq — the TT desktop icon that asks the remote system management host for a keyboard + mouse session
 * (tt030-remote-keyboard FR-13). Runs /usr/bin/ttkbd-request (sign + MQTT publish, ~3 s on the 68030) with the busy
 * bee showing; an alert only on failure, since the session starting (the host's mouse moves the TT's pointer) is the
 * success signal. Installed as /opt/ttkbd/ttkbd.prg by the ttkbdd RPM; drag it to the desktop once for an icon.
 * Build: m68k-atari-mint-gcc -m68020-60 -O2 -s -o ttkbd.prg src/ttkbdreq.c -lgem
 */
#include <gem.h>
#include <stdlib.h>

int main(void)
{
	int rc;
	if (appl_init() < 0) return 2;
	graf_mouse(BUSYBEE, 0);
	rc = system("/bin/sh /usr/bin/ttkbd-request > /dev/null 2>&1");
	graf_mouse(ARROW, 0);
	if (rc != 0)
		form_alert(1, "[3][Keyboard session request failed.|Is the WiFi up? Details:|run ttkbd-request in a shell.][ OK ]");
	appl_exit();
	return rc != 0;
}
