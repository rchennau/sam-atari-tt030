/* ttsound — audio check for the TT030: the YM2149 sound chip (internal speaker) and DMA sound (rear audio out).
 * Usage: ttsound psg [0|1]    2 s 440 Hz tone on YM2149 channel A (0|1: set port A bit 6, the speaker control, first)
 *        ttsound dma [l|r|b]  2 s 440 Hz sine on DMA sound, 25 kHz stereo, left / right / both (default both)
 * DMA sound reads ST-RAM only, so the buffer comes from Mxalloc(.., 0). The LMC1992 volume chip is set through the
 * Microwire interface (master and both channels to maximum) before playing, since its state after boot is not known.
 * Build: m68k-atari-mint-gcc -m68020-60 -O2 -s -o ttsound.ttp src/ttsound.c -lm
 */
#include <mint/osbind.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define RATE 25033L                   /* DMA sound "25 kHz" */
#define SECS 2
#define REG8(a) (*(volatile unsigned char *)(a))
#define REG16(a) (*(volatile unsigned short *)(a))

static signed char *buf;
static long len;

static int mw(unsigned short cmd)     /* one Microwire write to the LMC1992; 0 if the interface never finishes */
{
	long spin;
	REG16(0xFF8924) = 0x07FF;
	REG16(0xFF8922) = 0x0400 | cmd;
	/* the mask rotates while the 11 bits shift out; bounded, since this runs in supervisor mode where a
	   stuck loop freezes the whole machine (2026-10-16: an unbounded wait here hung the TT) */
	for (spin = 200000; spin && REG16(0xFF8924) == 0x07FF; spin--)
		;
	if (!spin)
		return 0;                     /* the mask never rotated: no shift happened */
	for (spin = 200000; spin && REG16(0xFF8924) != 0x07FF; spin--)
		;
	return spin != 0;
}

static int mw_ok;
static unsigned char conterm;
static long peek_conterm(void) { conterm = *(volatile unsigned char *)0x484; return 0; }
static unsigned long cnt[3];

static long dma_peek(void)             /* DMA sound address counter + control */
{
	cnt[0] = (unsigned long)REG8(0xFF8909) << 16 | REG8(0xFF890B) << 8 | REG8(0xFF890D);
	cnt[1] = REG8(0xFF8901);
	return 0;
}

static long mixer(void)
{
	mw_ok = mw(0x0C0 | 40)            /* master volume 0 dB */
		& mw(0x140 | 20)          /* left 0 dB */
		& mw(0x100 | 20)          /* right 0 dB */
		& mw(0x040 | 6)           /* bass flat */
		& mw(0x080 | 6)           /* treble flat */
		& mw(0x000 | 1);          /* mix: DMA + YM2149 */
	return 0;
}

static long dma_play(void)
{
	unsigned long s = (unsigned long)buf, e = s + len;
	REG8(0xFF8901) = 0;               /* stop */
	REG8(0xFF8903) = s >> 16; REG8(0xFF8905) = s >> 8; REG8(0xFF8907) = s;
	REG8(0xFF890F) = e >> 16; REG8(0xFF8911) = e >> 8; REG8(0xFF8913) = e;
	REG8(0xFF8921) = 0x02;            /* stereo, 25 kHz */
	REG8(0xFF8901) = 1;               /* play once */
	return 0;
}

int main(int argc, char **argv)
{
	long i;
	char ch = argc > 2 ? argv[2][0] : 'b';
	if (argc > 1 && !strcmp(argv[1], "info")) {
		Supexec(peek_conterm);
		printf("ttsound: conterm %02x (keyclick %s, bell %s), YM port A %02x, mixer reg7 %02x\n", conterm,
			conterm & 1 ? "on" : "off", conterm & 4 ? "on" : "off", Giaccess(0, 14) & 0xff, Giaccess(0, 7) & 0xff);
		return 0;
	}
	Supexec(mixer);
	printf("ttsound: LMC1992 volume/mix %s\n", mw_ok ? "set" : "NOT acknowledged (Microwire timeout)");
	if (argc > 2 && !strcmp(argv[1], "psg") && (argv[2][0] == '0' || argv[2][0] == '1')) {
		/* port A bit 6 = internal speaker control on Ataris that have one; try the given level */
		if (argv[2][0] == '0') Offgibit(~0x40); else Ongibit(0x40);
		printf("ttsound: port A bit 6 = %c (port A now %02x)\n", argv[2][0], Giaccess(0, 14) & 0xff);
	}
	if (argc > 1 && !strcmp(argv[1], "psg")) {
		/* Dosound program: channel A period 284 (2 MHz / 16 / 284 = 440 Hz), mixer tone A only, volume 15 */
		static unsigned char snd[] = {0, 284 & 0xff, 1, 284 >> 8, 7, 0xFE, 8, 15, 0xFF, 0};
		Dosound(snd);
		usleep(SECS * 1000000L);
		Giaccess(0, 8 | 0x80);        /* volume A off */
		printf("ttsound: psg 440 Hz for %d s\n", SECS);
		return 0;
	}
	if (argc > 1 && !strcmp(argv[1], "dma")) {
		len = RATE * SECS * 2;
		if (!(buf = (signed char *)Mxalloc(len, 0))) { printf("ttsound: no ST-RAM\n"); return 1; }
		for (i = 0; i < len / 2; i++) {
			signed char v = (signed char)(100 * sin(2 * M_PI * 440 * i / RATE));
			buf[2 * i] = ch != 'r' ? v : 0;
			buf[2 * i + 1] = ch != 'l' ? v : 0;
		}
		Supexec(dma_play);
		usleep(500000L);
		Supexec(dma_peek);
		printf("ttsound: buffer %08lx-%08lx, after 0.5 s counter %08lx, control %lu\n",
			(unsigned long)buf, (unsigned long)buf + len, cnt[0], cnt[1]);
		usleep(SECS * 1000000L - 300000L);
		Mfree(buf);
		printf("ttsound: dma 440 Hz %s for %d s\n", ch == 'l' ? "left" : ch == 'r' ? "right" : "both", SECS);
		return 0;
	}
	printf("usage: ttsound psg | dma [l|r|b]\n");
	return 2;
}
