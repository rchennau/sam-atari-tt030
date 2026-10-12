/* tramp (TRAM): peek each link except the one to the T425 for an unbooted transputer. */
#include <misc.h>
#include <channel.h>
#include <process.h>
static Channel *out[4] = { LINK0OUT, LINK1OUT, LINK2OUT, LINK3OUT };
static Channel *in[4]  = { LINK0IN,  LINK1IN,  LINK2IN,  LINK3IN };
int main(void)
{
  Channel *up = (Channel *)get_param(1);
  int uplink = *(int *)get_param(2);
  int res[8], l;
  for (l = 0; l < 4; l++) {
    unsigned char cmd[5] = { 1, 0, 0, 0, 0x80 };
    int w = 0, t;
    res[2*l+1] = 0;
    if (l == uplink) { res[2*l] = 0; continue; }
    t = ProcTimePlus(ProcTime(), 3000);
    if (ChanOutTimeFail(out[l], cmd, 5, t)) { res[2*l] = 1; ChanReset(out[l]); continue; }
    t = ProcTimePlus(ProcTime(), 3000);
    if (ChanInTimeFail(in[l], &w, 4, t)) { res[2*l] = 2; ChanReset(in[l]); continue; }
    res[2*l] = 3; res[2*l+1] = w;
  }
  ChanOut(up, res, sizeof res);
  return 0;
}
