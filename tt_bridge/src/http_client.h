/* ==============================================================================
 * http_client.h - SAM TT-Bridge HTTP Client Interface for Atari TT030
 * Target: Port 8080 (TOS 3.06 / FreeMiNT / STiNG Stack)
 * Atari TT030 Ecosystem Integration - Phase 1
 * ==============================================================================
 */

#ifndef TT_HTTP_CLIENT_H
#define TT_HTTP_CLIENT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Error Codes */
#define TT_HTTP_OK            0
#define TT_HTTP_ERR_INIT     -1
#define TT_HTTP_ERR_RESOLVE  -2
#define TT_HTTP_ERR_SOCKET   -3
#define TT_HTTP_ERR_CONNECT  -4
#define TT_HTTP_ERR_SEND     -5
#define TT_HTTP_ERR_RECV     -6
#define TT_HTTP_ERR_BUFFER   -7
#define TT_HTTP_ERR_STING    -8

/* Response Structure */
typedef struct {
    int status_code;
    char *headers;
    size_t headers_len;
    unsigned char *body;
    size_t body_len;
    size_t content_length;
} tt_http_response_t;

/* HTTP Client Configuration */
typedef struct {
    const char *host;
    int port;
    const char *path;
    const char *method;       /* "GET" or "POST" */
    const char *post_data;
    size_t post_data_len;
    const char *custom_headers;
    int timeout_sec;
} tt_http_request_t;

/**
 * Initialize network layer (Checks MiNT sockets / STiNG Cookie Jar)
 * Returns TT_HTTP_OK on success.
 */
int tt_http_init(void);

/**
 * Cleanup network layer.
 */
void tt_http_cleanup(void);

/**
 * Execute HTTP Request to TT-Bridge server.
 * Caller must free response using tt_http_free_response().
 */
int tt_http_execute(const tt_http_request_t *req, tt_http_response_t *resp);

/**
 * Free memory allocated in response structure.
 */
void tt_http_free_response(tt_http_response_t *resp);

/**
 * Helper to get human readable error message string.
 */
const char* tt_http_strerror(int err_code);

#ifdef __cplusplus
}
#endif

#endif /* TT_HTTP_CLIENT_H */
