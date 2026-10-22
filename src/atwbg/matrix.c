/* matrix — see matrix.h. A column's drop moves one cell per step; each step draws at most 1 + MX_BANDS cells and
 * erases 1, recolouring only the rows where the tail changes band, so the cost per step is fixed (6 cells, 768 bytes). */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "matrix.h"

static const char glyphs[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ$+-*=<>:;|";
static unsigned char rev[256];          /* bit-reversed byte: glyphs are drawn mirrored, as in the film */
static uint32_t mask4[16];           /* 4 font bits -> 4 bytes of 0xFF/0x00 in memory order (endian-neutral) */
/* 1 bit: ink kept per dither level, even/odd rows; head and first band solid, then 50 %, 25 %, 12.5 % */
static const unsigned char dither[MX_BANDS + 1][2] = {{0xFF, 0xFF}, {0xFF, 0xFF}, {0xAA, 0x55}, {0x88, 0x22}, {0x80, 0x08}};

static unsigned long rnd(struct mx *m)
{
	m->seed ^= m->seed << 13;
	m->seed ^= m->seed >> 17;
	m->seed ^= m->seed << 5;
	return m->seed;
}

static long lum(unsigned short c)        /* RGB565 -> luminance, integer Rec.601 weights */
{
	return ((c >> 11) & 31) * 8L * 299 + ((c >> 5) & 63) * 4L * 587 + (c & 31) * 8L * 114;
}

void mx_colours(struct mx *m, const unsigned short *lut)
{
	long lo, hi, want, best, d;
	int i, b;
	m->bg = m->head = 0;
	for (i = 1; i < 256; i++) {
		if (lum(lut[i]) < lum(lut[m->bg])) m->bg = i;
		if (lum(lut[i]) > lum(lut[m->head])) m->head = i;
	}
	lo = lum(lut[m->bg]);
	hi = lum(lut[m->head]);
	for (b = 0; b < MX_BANDS; b++) {        /* 70 % of the range for the first band down to ~20 % for the last */
		want = lo + (hi - lo) * (70 - b * 50 / (MX_BANDS - 1)) / 100;
		best = -1;
		for (i = 0; i < 256; i++) {
			if (i == m->bg || i == m->head) continue;
			d = labs(lum(lut[i]) - want);
			if (best < 0 || d < best) { best = d; m->band[b] = i; }
		}
	}
}

void mx_mono(struct mx *m, unsigned char bg)
{
	int b;
	m->bpp = 1;
	m->bg = bg;
	m->head = 0;
	for (b = 0; b < MX_BANDS; b++) m->band[b] = b + 1;
}

int mx_init(struct mx *m, unsigned long seed)
{
	int i, j;
	for (i = 0; i < 256; i++)
		for (rev[i] = 0, j = 0; j < 8; j++)
			if (i & (1 << j)) rev[i] |= 0x80 >> j;
	for (i = 0; i < 16; i++) {
		unsigned char b[4];
		for (j = 0; j < 4; j++) b[j] = (i & (8 >> j)) ? 0xFF : 0;
		memcpy(&mask4[i], b, 4);
	}
	m->seed = seed ? seed : 1;
	m->pos = malloc(m->cols * sizeof *m->pos);
	m->speed = malloc(m->cols);
	m->tick = malloc(m->cols);
	m->glyph = calloc((size_t)m->rows * m->cols, 1);
	if (!m->pos || !m->speed || !m->tick || !m->glyph) { mx_free(m); return -1; }
	for (i = 0; i < m->cols; i++) {         /* staggered starts: the screen fills over the first few seconds */
		m->pos[i] = -1 - (short)(rnd(m) % (m->rows * 2));
		m->speed[i] = 1 + rnd(m) % 3;
		m->tick[i] = 0;
	}
	return 0;
}

void mx_free(struct mx *m)
{
	free(m->pos); free(m->speed); free(m->tick); free(m->glyph);
	m->pos = 0; m->speed = m->tick = m->glyph = 0;
}

static void cell1(struct mx *m, int r, int c, int level)    /* 1 bit; level < 0: erase */
{
	unsigned char *p = m->fb + (long)r * 16 * m->stride + c, ch = m->glyph[r * m->cols + c];
	int y;
	for (y = 0; y < 16; y++, p += m->stride)
		*p = m->bg ^ (level < 0 ? 0 : rev[m->font[y * m->fwidth + ch]] & dither[level][y & 1]);
}

static void cell(struct mx *m, int r, int c, int colour)    /* colour < 0: erase */
{
	uint32_t *p = (uint32_t *)(m->fb + (long)r * 16 * m->stride + c * 8);
	uint32_t fg = 0x01010101U * (colour < 0 ? m->bg : colour), bg = 0x01010101U * m->bg, k;
	unsigned char ch = m->glyph[r * m->cols + c];
	int y;
	if (m->bpp == 1) { cell1(m, r, c, colour); return; }
	for (y = 0; y < 16; y++, p += m->stride / 4) {
		k = colour < 0 ? 0 : rev[m->font[y * m->fwidth + ch]];
		p[0] = (mask4[k >> 4] & fg) | (~mask4[k >> 4] & bg);
		p[1] = (mask4[k & 15] & fg) | (~mask4[k & 15] & bg);
	}
}

void mx_clear(struct mx *m)
{
	int y;
	for (y = 0; y < m->rows * 16; y++) memset(m->fb + y * m->stride, m->bg, m->cols * (m->bpp == 1 ? 1 : 8));
}

int mx_frame(struct mx *m)
{
	int c, b, h, r, n = 0;
	for (c = 0; c < m->cols; c++) {
		if (m->pos[c] < 0) {                     /* idle column: count up to its start */
			if (++m->pos[c] == 0) { m->speed[c] = 1 + rnd(m) % 3; m->tick[c] = 0; }
			continue;
		}
		if (++m->tick[c] < m->speed[c]) continue;
		m->tick[c] = 0;
		h = m->pos[c];
		if (h < m->rows) {
			m->glyph[h * m->cols + c] = glyphs[rnd(m) % (sizeof glyphs - 1)];
			cell(m, h, c, m->head); n++;
		}
		for (b = 0; b < MX_BANDS; b++) {         /* the row entering band b */
			r = h - 1 - b * MX_BAND_LEN;
			if (r >= 0 && r < m->rows) { cell(m, r, c, m->band[b]); n++; }
		}
		r = h - 1 - MX_BANDS * MX_BAND_LEN;      /* the row leaving the tail */
		if (r >= 0 && r < m->rows) { cell(m, r, c, -1); n++; }
		if (r >= m->rows - 1) m->pos[c] = -1 - (short)(rnd(m) % m->rows);
		else m->pos[c]++;
	}
	return n;
}
