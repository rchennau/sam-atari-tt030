/* stdint.h shim for INMOS icc (C89, 1991): T4 int and long are both 32-bit. No 64-bit type exists.
 * ponytail: only the types c25519's f25519/c25519 use; add more if another source needs them. */
#ifndef ICC_STDINT_H
#define ICC_STDINT_H
typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned long uint32_t;
typedef long int32_t;
/* c25519.h/f25519.h declare `static inline` helpers; icc is C89. Every c25519 header includes
 * <stdint.h> before those, so dropping the keyword here is enough. */
#define inline
#endif
