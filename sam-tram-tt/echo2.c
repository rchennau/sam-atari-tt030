/* echo2 (2nd transputer): echo every message back to Slot 1. */
#include <misc.h>
#include <channel.h>
static char buf[16384];
int main(void)
{
  Channel *in = (Channel *)get_param(1), *out = (Channel *)get_param(2);
  int hdr[2];
  for (;;) {
    ChanIn(in, hdr, 8);
    if (hdr[0] == 0) return 0;
    ChanIn(in, buf, hdr[0]); ChanOut(out, buf, hdr[0]);
  }
}
