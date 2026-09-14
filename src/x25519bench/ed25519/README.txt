Ed25519 (RFC 8032) port for the ATW800/2 T425 — kanban 3c29ce8a.

Interoperable with OpenSSH/Dropbear: verified against a Python `cryptography` Ed25519 signature
(edsign_verify VALID, edsign_sec_to_pub matches), on both the native and the 64-bit-emulation
SHA-512 path.

Files here (Daniel Beer's c25519, public domain) plus the fe32 field maths and the 32-bit SHA-512:
  edsign.c/.h        RFC 8032 sign/verify
  ed25519.c/.h       Edwards point ops   (ed25519.c: designated initializers removed for INMOS C89)
  fprime.c/.h        arithmetic mod the group order
  morph25519.c/.h    Montgomery<->Edwards
  ../fe32/f25519.c   field mod 2^255-19 with lmul (constant-time)
  ../fe32/sha512_32.c + sha512.h   SHA-512 with a 32-bit-pair uint64 (icc has no 64-bit type)

MEASURED (2026-09-13): edsign_verify on the real T425 = 6546 ms vs 9405 ms on the 68030 (1.44x).
Ed25519 sign on the 68030 is only 2620 ms, so it is not worth offloading.

Build for the T425: icc each .c -t4, ilink with a short ISEARCH, icollect -t; run via iserver -sb.
Diff/interop test on a Linux host: gcc edbench.c edsign.c ed25519.c fprime.c morph25519.c
  ../fe32/f25519.c ../fe32/sha512_32.c  (add -DFORCE_EMU64 to exercise the T425 SHA path).
