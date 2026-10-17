/* tlsbench — time a TLS 1.3 HTTPS fetch on the TT with Mbed TLS 3.6.6 (HighWire's library), with and without
 * X25519 on the T425 through ttjobd. Transputer evaluation of HighWire, operator 2026-10-17
 * (docs/transputer-evaluation.md).
 *   tlsbench [-x] [-v] [-s SUITE] [-n REPS] HOST PORT PATH CAFILE
 *   -x  X25519 via ttjobd (falls back to the 68030 if ttjobd does not answer)
 *   -v  also compute every offloaded X25519 on the 68030 and compare (correctness check, doubles the cost)
 *   -s  TLS 1.3 suite, e.g. TLS1-3-AES-128-GCM-SHA256 (default) or TLS1-3-CHACHA20-POLY1305-SHA256
 * Prints one line per rep: handshake ms, body ms, bytes, X25519 ops and their ms. Key exchange group: X25519 only.
 * Needs Mbed TLS built with src/tlsbench/mbedtls_mint_config.h and ecp-hook.patch (build_tlsbench.sh). */
#define MBEDTLS_ALLOW_PRIVATE_ACCESS        /* the hook reads grp->id and point coordinates */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/ecp.h"
#include "mbedtls/entropy.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/ssl_ciphersuites.h"
#include "mbedtls/x509_crt.h"
#include "libttjob.h"

static int offload, verify, bypass, x_ops, x_fail;
static long x_ms;
static mbedtls_ctr_drbg_context drbg;     /* also the blinding RNG for -v's 68030 check */

mbedtls_ms_time_t mbedtls_ms_time(void)     /* mintlib has no clock_gettime (MBEDTLS_PLATFORM_MS_TIME_ALT) */
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (mbedtls_ms_time_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static long now_ms(void) { return (long)mbedtls_ms_time(); }

/* Called first by ecp_mul_restartable_internal (ecp-hook.patch): 0 = R filled, 1 = not handled. */
int tlsbench_ecp_hook(mbedtls_ecp_group *grp, mbedtls_ecp_point *R, const mbedtls_mpi *m, const mbedtls_ecp_point *P)
{
	unsigned char in[64], out[32];
	long olen = 0, t0;
	int id, st;
	if (!offload || bypass || grp->id != MBEDTLS_ECP_DP_CURVE25519)
		return 1;
	if (mbedtls_mpi_write_binary_le(m, in, 32) || mbedtls_mpi_write_binary_le(&P->X, in + 32, 32))
		return 1;
	t0 = now_ms();
	if ((id = ttjob_submit(OP_X25519, 0, in, 64)) < 0 || (st = ttjob_wait(id, out, 32, &olen, 60000L)) != ST_OK
	    || olen != 32) {
		x_fail++;
		return 1;                                   /* the 68030 does it */
	}
	x_ms += now_ms() - t0;
	x_ops++;
	if (mbedtls_mpi_read_binary_le(&R->X, out, 32) || mbedtls_mpi_lset(&R->Z, 1))
		return MBEDTLS_ERR_ECP_ALLOC_FAILED;
	mbedtls_mpi_free(&R->Y);
	if (verify) {                                   /* same product on the 68030 must match */
		mbedtls_ecp_point C;
		int bad;
		mbedtls_ecp_point_init(&C);
		bypass = 1;
		bad = mbedtls_ecp_mul(grp, &C, m, P, mbedtls_ctr_drbg_random, &drbg) != 0
		      || mbedtls_mpi_cmp_mpi(&C.X, &R->X) != 0;
		bypass = 0;
		mbedtls_ecp_point_free(&C);
		if (bad) {
			fprintf(stderr, "tlsbench: T425 X25519 result DIFFERS from the 68030\n");
			exit(3);
		}
	}
	return 0;
}

int main(int argc, char **argv)
{
	static const uint16_t groups[] = { MBEDTLS_SSL_IANA_TLS_GROUP_X25519, 0 };
	static int suites[2];
	static unsigned char buf[16384];
	const char *suite = "TLS1-3-AES-128-GCM-SHA256", *host, *port, *path, *ca;
	mbedtls_entropy_context ent;
	mbedtls_x509_crt cacert;
	mbedtls_ssl_config conf;
	int i = 1, reps = 1, r, ret;
	while (i < argc && argv[i][0] == '-') {
		if (!strcmp(argv[i], "-x")) offload = 1;
		else if (!strcmp(argv[i], "-v")) verify = 1;
		else if (!strcmp(argv[i], "-s") && i + 1 < argc) suite = argv[++i];
		else if (!strcmp(argv[i], "-n") && i + 1 < argc) reps = atoi(argv[++i]);
		else break;
		i++;
	}
	if (argc - i != 4) {
		fprintf(stderr, "usage: tlsbench [-x] [-v] [-s SUITE] [-n REPS] HOST PORT PATH CAFILE\n");
		return 2;
	}
	host = argv[i]; port = argv[i + 1]; path = argv[i + 2]; ca = argv[i + 3];
	if (!(suites[0] = mbedtls_ssl_get_ciphersuite_id(suite))) {
		fprintf(stderr, "tlsbench: unknown suite %s\n", suite);
		return 2;
	}
	if (psa_crypto_init() != PSA_SUCCESS) { fprintf(stderr, "tlsbench: psa_crypto_init failed\n"); return 1; }
	mbedtls_entropy_init(&ent);
	mbedtls_ctr_drbg_init(&drbg);
	mbedtls_x509_crt_init(&cacert);
	mbedtls_ssl_config_init(&conf);
	if ((ret = mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &ent, (const unsigned char *)"tlsbench", 8))
	    || (ret = mbedtls_x509_crt_parse_file(&cacert, ca))
	    || (ret = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
	                                          MBEDTLS_SSL_PRESET_DEFAULT))) {
		fprintf(stderr, "tlsbench: setup failed -0x%04x\n", (unsigned)-ret);
		return 1;
	}
	mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &drbg);
	mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
	mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
	mbedtls_ssl_conf_min_tls_version(&conf, MBEDTLS_SSL_VERSION_TLS1_3);
	mbedtls_ssl_conf_max_tls_version(&conf, MBEDTLS_SSL_VERSION_TLS1_3);
	mbedtls_ssl_conf_groups(&conf, groups);
	mbedtls_ssl_conf_ciphersuites(&conf, suites);
	for (r = 1; r <= reps; r++) {
		mbedtls_net_context net;
		mbedtls_ssl_context ssl;
		long t0, t1, t2, bytes = 0;
		int n, len;
		x_ops = x_fail = 0; x_ms = 0;
		mbedtls_net_init(&net);
		mbedtls_ssl_init(&ssl);
		t0 = now_ms();
		if ((ret = mbedtls_net_connect(&net, host, port, MBEDTLS_NET_PROTO_TCP))
		    || (ret = mbedtls_ssl_setup(&ssl, &conf)) || (ret = mbedtls_ssl_set_hostname(&ssl, host))) {
			fprintf(stderr, "tlsbench: connect/setup failed -0x%04x\n", (unsigned)-ret);
			return 1;
		}
		mbedtls_ssl_set_bio(&ssl, &net, mbedtls_net_send, mbedtls_net_recv, NULL);
		while ((ret = mbedtls_ssl_handshake(&ssl)) != 0)
			if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
				fprintf(stderr, "tlsbench: handshake failed -0x%04x\n", (unsigned)-ret);
				return 1;
			}
		t1 = now_ms();
		len = snprintf((char *)buf, sizeof buf, "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", path, host);
		while ((ret = mbedtls_ssl_write(&ssl, buf, len)) <= 0)
			if (ret != MBEDTLS_ERR_SSL_WANT_WRITE) { fprintf(stderr, "tlsbench: write -0x%04x\n", (unsigned)-ret); return 1; }
		for (;;) {
			n = mbedtls_ssl_read(&ssl, buf, sizeof buf);
			if (n == MBEDTLS_ERR_SSL_WANT_READ || n == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET) continue;
			if (n <= 0) break;
			bytes += n;
		}
		t2 = now_ms();
		printf("rep %d suite %s offload %d handshake_ms %ld body_ms %ld bytes %ld x25519_t425 %d (%ld ms) fallback %d\n",
		       r, mbedtls_ssl_get_ciphersuite(&ssl), offload, t1 - t0, t2 - t1, bytes, x_ops, x_ms, x_fail);
		fflush(stdout);
		mbedtls_ssl_close_notify(&ssl);
		mbedtls_net_free(&net);
		mbedtls_ssl_free(&ssl);
	}
	return 0;
}
