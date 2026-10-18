/* bootdump — write the boot console text that C:\AUTO\BOOTLOG.PRG captured to /var/log/boot.log and syslog
 * (tag "boot", so it reaches fractal's rsyslog and the central log index like every other TT log).
 * Finds the buffer through the "log 0x..." line BOOTLOG.PRG wrote to C:\BOOTLOG.TXT, checks its magic, freezes it
 * (cap = len) so later console output is not appended, and drops VT52 escapes (ESC Y r c, ESC b/c x, ESC x).
 * Run once per boot from XaAES autorun (a .PRG, so XaAES opens no window). Exit 1 = no capture this boot, said aloud.
 * Installed as /usr/sbin/bootdump.prg by the atwtools RPM. */
#include <mint/osbind.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include "blog.h"

static struct blog *b;
static long len;
static char text[BLOG_CAP];

static long grab(void)
{
	if (b->magic != BLOG_MAGIC || b->len < 0 || b->len > BLOG_CAP) return -1;
	len = b->len;
	memcpy(text, b->data, len);
	b->cap = len;
	return 0;
}

int main(void)
{
	FILE *f = fopen("C:\\BOOTLOG.TXT", "r");
	char line[256], out[256];
	long i, n = 0, lines = 0;
	unsigned long addr = 0;
	openlog("boot", 0, LOG_DAEMON);
	while (f && fgets(line, sizeof line, f))
		if (sscanf(line, "log 0x%lx", &addr) == 1) break;
	if (f) fclose(f);
	if (!addr || (b = (struct blog *)addr, Supexec(grab))) {
		syslog(LOG_WARNING, "bootdump: no boot console capture this boot (BOOTLOG.TXT log line %s)", addr ? "bad magic" : "missing");
		return 1;
	}
	if (!(f = fopen("/var/log/boot.log", "w"))) { syslog(LOG_ERR, "bootdump: cannot write /var/log/boot.log"); return 1; }
	for (i = 0; i < len; i++) {
		char c = text[i];
		if (c == 27 && i + 1 < len) {
			char e = text[++i];
			i += e == 'Y' ? 2 : (e == 'b' || e == 'c') ? 1 : 0;
		} else if (c == '\n' || n == (long)sizeof out - 1) {
			out[n] = 0; fprintf(f, "%s\n", out);
			if (n) { syslog(LOG_INFO, "%s", out); lines++; }
			n = 0;
		} else if (c >= ' ' || c == '\t')
			out[n++] = c;
	}
	if (n) { out[n] = 0; fprintf(f, "%s\n", out); syslog(LOG_INFO, "%s", out); lines++; }
	fclose(f);
	syslog(LOG_INFO, "bootdump: %ld bytes, %ld lines%s", len, lines, len >= BLOG_CAP ? " (buffer full, later text lost)" : "");
	return 0;
}
