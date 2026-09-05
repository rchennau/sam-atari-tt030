#include <stdio.h>
#include <ioctl.h>
#include <string.h>

#define MAXKEY 32
#ifndef TIOCGXKEY
#define TIOCGXKEY	(('T'<< 8) | 13)
#define TIOCSXKEY	(('T'<< 8) | 14)
#endif

struct xkey {
	short xk_num;
	char xk_def[8];
} xk;

char *kname[MAXKEY] = {
"f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9", "f10",
"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10",
"up", "down", "right", "left", "help", "undo", "insert", "home",
"UP", "DOWN", "RIGHT", "LEFT"
};

char buf[80];

char *unescape(char *string)
{
	char *u = buf;
	char c;

	while (c = *string++) {
		if (c == '\\') {
			c = *string++;
			if (!c) break;
		}
		else if (c == '^') {
			c = *string++;
			if (!c) break;
			else if (c == '?') c = 127;
			else c &= 0x1f;
		}
		*u++ = c;
	}
	*u++ = 0;
	return buf;
}

char *escape(char *string)
{
	char *e, c;

	e = buf;
	while ( c = *string++ ) {
		if (c < ' ' && c > 0) {
			*e++ = '^';
			c += '@';
		}
		else if (c == 127) {
			*e++ = '^';
			c = '?';
		}
		else if (c == '\\' || c == '^')
			*e++ = '\\';
		*e++ = c;
	}
	*e++ = 0;
	return buf;
}

void
displaykeys()
{
	int i, r;
	int col = 0, maxcol;
	char *e;

	maxcol = 80;		/* really should do ioctl to get this */
	for (i = 0; i < MAXKEY; i++) {
		xk.xk_num = i;
		r = ioctl(0, TIOCGXKEY, &xk);
		if (r) {
			perror("ioctl");
			exit(1);
		}
		e = escape(xk.xk_def);
		col += (r = strlen(kname[i]) + strlen(e) + 2);
		if (col >= maxcol) {
			printf("\n");
			col = r;
		}
		printf("%s %s ", kname[i], e);
	}
	printf("\n");
}

void
definekey(char *key, char *def)
{
	int i;

	for (i = 0; i < MAXKEY; i++)
		if (!strcmp(kname[i], key)) break;
	if (i >= MAXKEY) {
		fprintf(stderr, "Unknown key: %s\n", key);
		return;
	}
	strncpy(xk.xk_def, unescape(def), 7);
	xk.xk_num = i;
	xk.xk_def[7] = 0;
	i = ioctl(0, TIOCSXKEY, &xk);
	if (i) {
		perror("ioctl");
	}
}

int
main(int argc, char **argv)
{
	char *progname;
	char *key, *def;

	progname = *argv++;

	if (!isatty(0)) {
		fprintf(stderr, "%s must be run on a terminal\n", progname);
		return 1;
	}
	if (!*argv) {
		displaykeys();
		return 0;
	}
	while (*argv) {
		key = *argv++;
		def = *argv++;
		if (!def) {
			fprintf(stderr, "Usage: %s [key def]...\n", progname);
			return 2;
		}
		definekey(key, def);
	}
	return 0;
}
