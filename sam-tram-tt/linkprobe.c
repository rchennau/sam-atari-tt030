/* tramprobe: is an unbooted transputer (TRAM) waiting on T425 links 1-3?
   Sends the INMOS boot-protocol PEEK (control byte 1 + address) and waits for the word back.
   ponytail: first hop only; a TRAM behind another TRAM needs a booted worm to see. */
#include <stdio.h>
#include <channel.h>
#include <process.h>

static Channel *out[4] = { LINK0OUT, LINK1OUT, LINK2OUT, LINK3OUT };
static Channel *in[4]  = { LINK0IN,  LINK1IN,  LINK2IN,  LINK3IN };

int main(void)
{
  int l, a;
  static int addrs[2] = { (int)0x80000000, (int)0x80000070 };
  for (l = 1; l <= 3; l++) {
    printf("link %d:", l);
    for (a = 0; a < 2; a++) {
      unsigned char cmd[5];
      int w = 0, t;
      cmd[0] = 1;                                   /* PEEK */
      cmd[1] = addrs[a] & 0xff; cmd[2] = (addrs[a] >> 8) & 0xff;
      cmd[3] = (addrs[a] >> 16) & 0xff; cmd[4] = (addrs[a] >> 24) & 0xff;
      t = ProcTimePlus(ProcTime(), 3000);            /* ~0.2 s at low priority */
      if (ChanOutTimeFail(out[l], cmd, 5, t)) { printf(" no-accept (nothing attached)"); ChanReset(out[l]); break; }
      t = ProcTimePlus(ProcTime(), 3000);
      if (ChanInTimeFail(in[l], &w, 4, t)) { printf(" accepted, no reply"); ChanReset(in[l]); break; }
      printf(" peek %08x=%08x", addrs[a], w);
    }
    printf("\n");
  }
  return 0;
}
