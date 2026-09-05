/* ==============================================================================
 * main.c - SAM TT-Bridge CLI Utility for Atari TT030
 * Target: SAM Host Bridge (Port 8080) over TOS / FreeMiNT / STiNG
 * Atari TT030 Ecosystem Integration - Phase 1
 * ==============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "http_client.h"

#ifdef __TOS__
  #include <osbind.h>
#endif

static void print_usage(const char *progname) {
    printf("==================================================================\n");
    printf("SAM TT-Bridge HTTP Client for Atari TT030 (Port 8080)\n");
    printf("==================================================================\n");
    printf("Usage: %s [OPTIONS]\n\n", progname);
    printf("Options:\n");
    printf("  -h, --host HOST      Target host IP address (default: 192.168.1.1)\n");
    printf("  -p, --port PORT      Target TCP port (default: 8080)\n");
    printf("  -u, --url PATH       API URL path (default: /api/v1/status)\n");
    printf("  -m, --method METHOD  HTTP method GET/POST (default: GET)\n");
    printf("  -d, --data DATA      POST payload data (JSON/string)\n");
    printf("  -o, --out FILE       Save HTTP response body to output file\n");
    printf("  --help               Display this help text\n\n");
    printf("Example:\n");
    printf("  %s -h 192.168.1.1 -p 8080 -u /api/v1/ping\n", progname);
    printf("==================================================================\n");
}

int main(int argc, char *argv[]) {
    tt_http_request_t req;
    tt_http_response_t resp;
    const char *host = "192.168.1.1";
    int port = 8080;
    const char *path = "/api/v1/status";
    const char *method = "GET";
    const char *post_data = NULL;
    const char *out_file = NULL;
    int i;

    memset(&req, 0, sizeof(req));
    memset(&resp, 0, sizeof(resp));

    for (i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--host") == 0) && i + 1 < argc) {
            host = argv[++i];
        } else if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "-u") == 0 || strcmp(argv[i], "--url") == 0) && i + 1 < argc) {
            path = argv[++i];
        } else if ((strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--method") == 0) && i + 1 < argc) {
            method = argv[++i];
        } else if ((strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--data") == 0) && i + 1 < argc) {
            post_data = argv[++i];
        } else if ((strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--out") == 0) && i + 1 < argc) {
            out_file = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    printf("[SAM TT-Bridge] Connecting to http://%s:%d%s (%s)...\n", host, port, path, method);

    req.host = host;
    req.port = port;
    req.path = path;
    req.method = method;
    req.post_data = post_data;
    if (post_data) req.post_data_len = strlen(post_data);
    req.timeout_sec = 10;

    int ret = tt_http_execute(&req, &resp);
    if (ret != TT_HTTP_OK) {
        fprintf(stderr, "[ERROR] TT-Bridge request failed: %s (%d)\n", tt_http_strerror(ret), ret);
        #ifdef __TOS__
          printf("\nPress any key to exit...");
          Cnecin();
        #endif
        return 1;
    }

    printf("\n[SAM TT-Bridge] HTTP Status: %d\n", resp.status_code);
    if (resp.headers) {
        printf("--- Response Headers ---\n%s\n------------------------\n", resp.headers);
    }

    if (resp.body && resp.body_len > 0) {
        if (out_file) {
            FILE *fp = fopen(out_file, "wb");
            if (fp) {
                fwrite(resp.body, 1, resp.body_len, fp);
                fclose(fp);
                printf("[SAM TT-Bridge] Saved %zu bytes to %s\n", resp.body_len, out_file);
            } else {
                fprintf(stderr, "[ERROR] Could not open output file %s for writing\n", out_file);
            }
        } else {
            printf("--- Response Body (%zu bytes) ---\n", resp.body_len);
            fwrite(resp.body, 1, resp.body_len, stdout);
            printf("\n---------------------------------\n");
        }
    }

    tt_http_free_response(&resp);

#ifdef __TOS__
    if (argc <= 1) {
        printf("\nPress any key to exit TOS desktop execution...");
        Cnecin();
    }
#endif

    return 0;
}
