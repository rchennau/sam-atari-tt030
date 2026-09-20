#!/usr/bin/env python3
"""Split a TT ROM image into its four byte lanes, or join four dumps back.

The TT030 has four 27C010 (128 KB) EPROMs, one per byte lane of the 68030's 32-bit bus:

    U601 = D31..24  (most significant byte of the long word)  -> bytes 0, 4, 8 ...
    U602 = D23..16                                            -> bytes 1, 5, 9 ...
    U603 = D15..8                                             -> bytes 2, 6, 10 ...
    U604 = D7..0    (least significant)                       -> bytes 3, 7, 11 ...

The 68030 is big-endian, so the most significant byte is the first byte of each long word. This
mapping is the documented Atari layout (confirmed 2026-09-20 against the a2ff/splitrom conventions
and Atari-Forum hardware threads), not a guess -- files are named by socket for that reason.

  interleave.py split image.img outprefix   -> outprefix.U601 .. .U604
  interleave.py join  outprefix joined.img
"""
import sys

SOCKETS = ('U601', 'U602', 'U603', 'U604')   # D31..24, D23..16, D15..8, D7..0


def split(img, prefix):
    d = open(img, 'rb').read()
    if len(d) % 4:
        sys.exit('image size %d is not a multiple of 4' % len(d))
    for n, sock in enumerate(SOCKETS):
        open('%s.%s' % (prefix, sock), 'wb').write(d[n::4])
        print('%s.%s  %d bytes  (lane %d)' % (prefix, sock, len(d) // 4, n))

def join(prefix, out):
    lanes = [open('%s.%s' % (prefix, sock), 'rb').read() for sock in SOCKETS]
    if len({len(x) for x in lanes}) != 1:
        sys.exit('dumps differ in size: %s' % [len(x) for x in lanes])
    d = bytearray(len(lanes[0]) * 4)
    for n, lane in enumerate(lanes):
        d[n::4] = lane
    open(out, 'wb').write(d)
    print('%s  %d bytes' % (out, len(d)))

if __name__ == '__main__':
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    (split if sys.argv[1] == 'split' else join)(sys.argv[2], sys.argv[3])
