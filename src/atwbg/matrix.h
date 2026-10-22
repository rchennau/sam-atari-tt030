/* matrix — falling-glyph animation for ttsaver -x matrix (PMS tt030-atw-display-tuning FR-9).
 * Pure drawing into an 8-bit chunky or a 1-bit framebuffer, so the host test (scripts/test_matrix.py) runs it on a RAM
 * buffer. 1 bit (mode 37): bg is the background byte (0x00 or 0xFF), glyphs are drawn inverted from it, and the tail
 * fades by ordered dither instead of colour (head and bands are dither levels 0..MX_BANDS, set by mx_mono). */
#ifndef MATRIX_H
#define MATRIX_H

#define MX_BANDS 4          /* tail colour bands after the head */
#define MX_BAND_LEN 3       /* rows per band; tail = MX_BANDS * MX_BAND_LEN rows */

struct mx {
	unsigned char *fb;              /* top-left pixel */
	long stride;                    /* bytes per line */
	int cols, rows;                 /* 8x16 cells */
	const unsigned char *font;      /* 8x16 font form: row r of char c at font[r * fwidth + c] */
	int fwidth;
	int bpp;                        /* 8 (default when 0) or 1 */
	unsigned char bg, head, band[MX_BANDS];   /* hardware colour indices */
	short *pos;                     /* per column: head row, or < 0 = idle (counts up to a restart) */
	unsigned char *speed, *tick;    /* per column: frames per step, frame counter */
	unsigned char *glyph;           /* rows * cols: the char drawn in each cell */
	unsigned long seed;
};

/* Pick bg (darkest), head (brightest) and the bands (evenly spaced in between) from a 256-entry RGB565 LUT. */
void mx_colours(struct mx *m, const unsigned short *lut565);
/* 1-bit framebuffer: set bpp = 1, the background byte and the dither levels. */
void mx_mono(struct mx *m, unsigned char bg);
/* cols/rows/fb/stride/font/fwidth set by the caller; allocates the per-column state. 0 ok, -1 no memory. */
int mx_init(struct mx *m, unsigned long seed);
void mx_free(struct mx *m);
/* Fill the whole area with bg. */
void mx_clear(struct mx *m);
/* Advance one frame; returns the number of cells drawn. */
int mx_frame(struct mx *m);

#endif
