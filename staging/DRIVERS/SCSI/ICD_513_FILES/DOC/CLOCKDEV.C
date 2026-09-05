/*
 * A simple device driver for u:\dev\clock. Reading from this
 * device produces a single line containing the current time, in the
 * format:
 * MM/DD/YY hh:mm:ss\r\n
 * Writing to it will change the time to the given one.
 *
 * This program is written by Eric R. Smith and is hereby placed in
 * the public domain.
 *
 * COMPILER NOTE: I've assumed that you're using a compiler (like gcc
 * or Lattice) that preserves registers d2 and a2 across function calls.
 * If your compiler uses these registers as scratch registers (e.g.
 * MWC, Alcyon) then you'll have to provide assembly language wrapper
 * functions that the kernel can call.
 * This code also assumes that sizeof(int) == 2.
 *
 * for gcc: compile with gcc -mshort -O clockdev.c -o clockdev.prg
 * for lcc: compile with -bn -b0 -r0 -v -w -t= clockdev.c -oclockdev.prg
 */

#ifdef __GNUC__
#include <minimal.h>
#endif
#include <osbind.h>
#include <basepage.h>
#include "mintbind.h"
#include "filesys.h"
#include "atarierr.h"

#ifdef LATTICE
#define BP _pbase
#else
#define BP _base
#endif

/* the name of the device we're installing */
char name[] = "U:\\DEV\\CLOCK";

/* kernel information */
struct kerinfo *kernel;
#define CCONWS (void)(*kernel->dos_tab[0x09])
#define RWABS (*kernel->bios_tab[4])
#define GETBPB (void *)(*kernel->bios_tab[7])
#define TGETTIME (*kernel->dos_tab[0x2c])
#define TGETDATE (*kernel->dos_tab[0x2a])
#define TSETTIME (*kernel->dos_tab[0x2d])
#define TSETDATE (*kernel->dos_tab[0x2b])

#define SPRINTF (*kernel->sprintf)
#define DEBUG (*kernel->debug)
#define ALERT (*kernel->alert)
#define TRACE (*kernel->trace)
#define FATAL (*kernel->fatal)

/* assumption: 16 bit integers */
#define word int

/* device driver information */
static long	clock_open	P_((FILEPTR *)),
		clock_write	P_((FILEPTR *, char *, long)),
		clock_read	P_((FILEPTR *, char *, long)),
		clock_lseek	P_((FILEPTR *, long, word)),
		clock_ioctl	P_((FILEPTR *, word, void *)),
		clock_datime	P_((FILEPTR *, word *, word)),
		clock_close	P_((FILEPTR *));

static long 	clock_select();
static void	clock_unselect();

DEVDRV clock_device = {
	clock_open, clock_write, clock_read, clock_lseek, clock_ioctl,
	clock_datime, clock_close, clock_select, clock_unselect,
	0, 0, 0
};

struct dev_descr devinfo = {
	&clock_device, 0, 0, (struct tty *)0, 0L, 0L, 0L, 0L
};

#ifdef LATTICE
BASEPAGE *BP;

void
start(BASEPAGE *bp)
{
	BP = bp;
	
	main();
}
#endif

/*
 * the main program just installs the device, and then does Ptermres
 * to remain resident
 */

main()
{
	kernel = (struct kerinfo *)Dcntl(DEV_INSTALL, name, &devinfo);
	if (!kernel || ((long)kernel) == -32) {
		Cconws("Unable to install clock device\r\n");
		Pterm(1);
	}
	Ptermres(256L + BP->p_tlen + BP->p_dlen + BP->p_blen, 0);
}

/*
 * here are the actual device driver functions
 */

/*
 * utility functions:
 * getclock(buf): get the current date and time and write it into
 * the pointed to buffer in the format "MM/DD/YY hh:mm:ss\r\n"
 *
 * setclock(buf): set the current date and time from the ASCII
 * string pointed to by buf, which must have the same format
 * as that returned by getdate
 */

void
getclock(buf)
	char *buf;
{
	int DD, MM, YY, hh, ss, mm;
	unsigned date, time;

	date = TGETDATE();
	time = TGETTIME();

	DD = date & 31;
	MM = (date >> 5) & 15;
	YY = 80 + ( (date >> 9) & 127 ); if (YY > 99) YY -= 100;

	ss = (time & 31) << 1;
	mm = (time >> 5) & 63;
	hh = (time >> 11) & 31;

	SPRINTF(buf, "%02d/%02d/%02d %02d:%02d:%02d\r\n", MM, DD, YY,
		hh, mm, ss);
}

static int
getint(buf)
	char *buf;
{
	int val = 0;

	val = *buf++ - '0';
	val = 10 * val + *buf - '0';
	return val;
}

void
setclock(buf)
	char *buf;
{
	int DD, MM, YY, hh, mm, ss;
	unsigned time, date;

	MM = getint(buf); buf += 3;
	if (MM < 1 || MM > 12) return;
	DD = getint(buf); buf += 3;
	if (DD < 1 || DD > 31) return;
	YY = getint(buf); buf += 3;
	if (YY < 80 || YY > 99) return;
	hh = getint(buf); buf += 3;
	if (hh < 0 || hh > 23) return;
	mm = getint(buf); buf += 3;
	if (mm < 0 || mm > 59) return;
	ss = getint(buf);
	if (ss < 0 || ss > 59) return;

	time = (hh << 11) | (mm << 5) | (ss >> 1);
	date = ((YY - 80) << 9) | (MM << 5) | DD;
	TSETTIME(time);
	TSETDATE(date);
}

#define NBYTES 19	/* strlen("DD/MM/YY hh:mm:ss\r\n") */

static long
clock_open(f)
	FILEPTR *f;
{
	return 0;
}

static long
clock_write(f, buf, bytes)
	FILEPTR *f; char *buf; long bytes;
{
	static char writebuf[NBYTES];
	static int  bufptr = 0;
	long wrote = 0;

	while (bytes-- > 0 && bufptr < NBYTES) {
/* ignore CR/LF at beginning of line */
		if (bufptr == 0 && (*buf == '\r' || *buf == '\n'))
			buf++;
		else
			writebuf[bufptr++] = *buf++;
		wrote++;
	}

/* do we have a complete date now? if so, set the clock */
	if (bufptr == NBYTES) {
		setclock(writebuf);
		bufptr = 0;
	}
	return wrote;
}

static long
clock_read(f, buf, bytes)
	FILEPTR *f; char *buf; long bytes;
{
/* SPRINTF will stuff one too many bytes in here (the \0) */
	static char readbuf[NBYTES+1];
	int where;
	long total = 0;

	getclock(readbuf);
	while (f->pos < NBYTES) {
		*buf++ = readbuf[f->pos++];
		total++;
	}
	return total;
}

static long
clock_lseek(f, where, whence)
	FILEPTR *f; long where; int whence;
{
	long newplace;

	switch(whence) {
	case 0:
		newplace = where;
		break;
	case 1:
		newplace = f->pos + where;
		break;
	case 2:
		newplace = (NBYTES-1) - where;
		break;
	}

	if (newplace < 0 || newplace >= NBYTES)
		return ERANGE;

	f->pos = newplace;
	return newplace;
}

static long
clock_ioctl(f, mode, buf)
	FILEPTR *f; int mode; void *buf;
{
	if (mode == FIONREAD || mode == FIONWRITE) {
		*((long *)buf) = (NBYTES-1) - f->pos;
		return 0;
	}
	else
		return EINVFN;
}

static long
clock_datime(f, timeptr, rwflag)
	FILEPTR *f;
	word *timeptr;
	int rwflag;
{
	if (rwflag)
		return EACCDN;
	*timeptr++ = TGETTIME();
	*timeptr = TGETDATE();
	return 0;
}

static long
clock_close(f)
	FILEPTR *f;
{
	return 0;
}

static long
clock_select()
{
	return 1;	/* we're always ready for I/O */
}

static void
clock_unselect()
{
	/* nothing for us to do here */
}
