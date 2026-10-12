/* tramc (Slot-1 TRAM): report own device id, loop ticks and RAM size; size the next transputer's RAM over
   link 1 with boot-protocol POKE/PEEK (it stays unbooted, so no up-link guess is needed).
   RAM sizing: write a marker at base+S; if it shows up at base the address decode wrapped -> RAM = S;
   if it does not read back there is no RAM at base+S -> RAM <= S (reported as "ends below"). */
#include <misc.h>
#include <channel.h>
#include "charcommon.h"

static int scratch[4];

static int peek(int a, int *w)
{
  unsigned char c[5]; int t;
  c[0] = 1; c[1] = a; c[2] = a >> 8; c[3] = a >> 16; c[4] = a >> 24;
  t = ProcTimePlus(ProcTime(), 3000);
  if (ChanOutTimeFail(LINK1OUT, c, 5, t)) return -1;
  t = ProcTimePlus(ProcTime(), 3000);
  if (ChanInTimeFail(LINK1IN, w, 4, t)) return -1;
  return 0;
}
static int poke(int a, int v)
{
  unsigned char c[9]; int t;
  c[0] = 0; c[1] = a; c[2] = a >> 8; c[3] = a >> 16; c[4] = a >> 24;
  c[5] = v; c[6] = v >> 8; c[7] = v >> 16; c[8] = v >> 24;
  t = ProcTimePlus(ProcTime(), 3000);
  return ChanOutTimeFail(LINK1OUT, c, 9, t) ? -1 : 0;
}

/* result: S index found (0..NSIZE-1) as positive code: 100+i = wrapped at size i, 200+i = no RAM at size i,
   300 = RAM beyond the largest size, -1 = link failure */
static int size_self(void)
{
  int i;
  volatile int *b = (volatile int *)scratch;
  for (i = 0; i < NSIZE; i++) {
    volatile int *p = (volatile int *)((char *)scratch + (0x100000 << i));
    int save = *p, got;
    *b = 0x11111111; *p = 0x22222222; got = *p;
    if (*b == 0x22222222) { *b = 0; return 100 + i; }
    *p = save;
    if (got != 0x22222222) return 200 + i;
  }
  return 300;
}
static int size_next(void)
{
  int i, base = (int)0x80001000, got, bw;
  for (i = 0; i < NSIZE; i++) {
    int a = base + (0x100000 << i);
    if (poke(base, 0x11111111) || poke(a, 0x22222222) || peek(a, &got) || peek(base, &bw)) return -1;
    if (bw == 0x22222222) return 100 + i;
    if (got != 0x22222222) return 200 + i;
  }
  return 300;
}

int main(void)
{
  Channel *up = (Channel *)get_param(1);
  int r[5];
  r[0] = devid(); r[1] = looptime(); r[2] = size_self();
  r[3] = peek((int)0x80000000, &r[4]);   /* is the next transputer still answering? */
  r[3] = r[3] ? -1 : size_next();
  ChanOut(up, r, 4 * 4);
  return 0;
}
