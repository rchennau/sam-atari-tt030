/* Mbed TLS 3.6 user config for FreeMiNT (tlsbench, HighWire transputer evaluation 2026-10-17).
 * mintlib has no clock_gettime: the ms clock comes from tlsbench.c (gettimeofday). */
#define MBEDTLS_PLATFORM_MS_TIME_ALT
#define TLSBENCH_ECP_HOOK
