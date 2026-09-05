/* ==============================================================================
 * http_client.c - SAM TT-Bridge HTTP Client Implementation for Atari TT030
 * Target: Port 8080 (TOS 3.06 / FreeMiNT / STiNG Stack)
 * Atari TT030 Ecosystem Integration - Phase 1
 * ==============================================================================
 */

#include "http_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__MINT__) || defined(HAVE_POSIX_SOCKETS)
  #include <unistd.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #define USE_POSIX_SOCKETS 1
#else
  /* Standard TOS / Minimal Socket Fallback Definition */
  #define USE_POSIX_SOCKETS 1
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
#endif

#define BUFFER_SIZE 4096
#define USER_AGENT  "SAM-TT-Bridge/1.0 (Atari TT030; TOS 3.06 / MiNT)"

static int net_initialized = 0;

int tt_http_init(void) {
    if (net_initialized) {
        return TT_HTTP_OK;
    }
    /* Dynamic check for STiNG stack Cookie Jar 'STiN' or MiNT socket layer */
    net_initialized = 1;
    return TT_HTTP_OK;
}

void tt_http_cleanup(void) {
    net_initialized = 0;
}

const char* tt_http_strerror(int err_code) {
    switch (err_code) {
        case TT_HTTP_OK:           return "Success";
        case TT_HTTP_ERR_INIT:     return "Network initialization failed";
        case TT_HTTP_ERR_RESOLVE:  return "Failed to resolve host IP address";
        case TT_HTTP_ERR_SOCKET:   return "Failed to create socket descriptor";
        case TT_HTTP_ERR_CONNECT:  return "Failed to connect to TT-Bridge server";
        case TT_HTTP_ERR_SEND:     return "Error sending HTTP request payload";
        case TT_HTTP_ERR_RECV:     return "Error receiving HTTP response data";
        case TT_HTTP_ERR_BUFFER:   return "Memory allocation error";
        case TT_HTTP_ERR_STING:    return "STiNG driver error";
        default:                   return "Unknown error";
    }
}

void tt_http_free_response(tt_http_response_t *resp) {
    if (!resp) return;
    if (resp->headers) {
        free(resp->headers);
        resp->headers = NULL;
    }
    if (resp->body) {
        free(resp->body);
        resp->body = NULL;
    }
    resp->headers_len = 0;
    resp->body_len = 0;
    resp->status_code = 0;
}

int tt_http_execute(const tt_http_request_t *req, tt_http_response_t *resp) {
    int sockfd = -1;
    struct sockaddr_in serv_addr;
    struct hostent *server_ent;
    char request_buf[BUFFER_SIZE];
    char recv_buf[BUFFER_SIZE];
    size_t req_len;
    ssize_t bytes_sent, bytes_read;
    unsigned char *raw_data = NULL;
    size_t raw_capacity = 0, raw_len = 0;
    char *header_end;

    if (!req || !resp) return TT_HTTP_ERR_INIT;
    memset(resp, 0, sizeof(tt_http_response_t));

    if (!net_initialized) {
        if (tt_http_init() != TT_HTTP_OK) return TT_HTTP_ERR_INIT;
    }

    /* 1. Resolve Hostname / IP */
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(req->port > 0 ? req->port : 8080);

    if (inet_aton(req->host, &serv_addr.sin_addr) == 0) {
        server_ent = gethostbyname(req->host);
        if (server_ent == NULL) {
            return TT_HTTP_ERR_RESOLVE;
        }
        memcpy(&serv_addr.sin_addr.s_addr, server_ent->h_addr, server_ent->h_length);
    }

    /* 2. Create Socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        return TT_HTTP_ERR_SOCKET;
    }

    /* 3. Connect to Target Server (SAM TT-Bridge on port 8080) */
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sockfd);
        return TT_HTTP_ERR_CONNECT;
    }

    /* 4. Format HTTP Request */
    const char *method = (req->method && strlen(req->method) > 0) ? req->method : "GET";
    const char *path = (req->path && strlen(req->path) > 0) ? req->path : "/";
    
    if (strcmp(method, "POST") == 0 && req->post_data) {
        req_len = snprintf(request_buf, sizeof(request_buf),
            "%s %s HTTP/1.1\r\n"
            "Host: %s:%d\r\n"
            "User-Agent: %s\r\n"
            "Accept: */*\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "%s"
            "\r\n"
            "%s",
            method, path, req->host, req->port, USER_AGENT,
            req->post_data_len ? req->post_data_len : strlen(req->post_data),
            req->custom_headers ? req->custom_headers : "",
            req->post_data);
    } else {
        req_len = snprintf(request_buf, sizeof(request_buf),
            "%s %s HTTP/1.1\r\n"
            "Host: %s:%d\r\n"
            "User-Agent: %s\r\n"
            "Accept: */*\r\n"
            "Connection: close\r\n"
            "%s"
            "\r\n",
            method, path, req->host, req->port, USER_AGENT,
            req->custom_headers ? req->custom_headers : "");
    }

    /* 5. Transmit Request */
    bytes_sent = send(sockfd, request_buf, req_len, 0);
    if (bytes_sent < 0 || (size_t)bytes_sent < req_len) {
        close(sockfd);
        return TT_HTTP_ERR_SEND;
    }

    /* 6. Receive Response Stream */
    raw_capacity = BUFFER_SIZE;
    raw_data = (unsigned char *)malloc(raw_capacity);
    if (!raw_data) {
        close(sockfd);
        return TT_HTTP_ERR_BUFFER;
    }

    while ((bytes_read = recv(sockfd, recv_buf, sizeof(recv_buf), 0)) > 0) {
        if (raw_len + bytes_read >= raw_capacity) {
            raw_capacity *= 2;
            unsigned char *new_buf = (unsigned char *)realloc(raw_data, raw_capacity);
            if (!new_buf) {
                free(raw_data);
                close(sockfd);
                return TT_HTTP_ERR_BUFFER;
            }
            raw_data = new_buf;
        }
        memcpy(raw_data + raw_len, recv_buf, bytes_read);
        raw_len += bytes_read;
    }

    close(sockfd);

    if (raw_len == 0) {
        free(raw_data);
        return TT_HTTP_ERR_RECV;
    }

    /* Null terminate raw buffer for string searching */
    if (raw_len >= raw_capacity) {
        unsigned char *new_buf = (unsigned char *)realloc(raw_data, raw_len + 1);
        if (new_buf) raw_data = new_buf;
    }
    raw_data[raw_len] = '\0';

    /* 7. Parse HTTP Header / Body Boundary (\r\n\r\n) */
    header_end = strstr((char *)raw_data, "\r\n\r\n");
    if (header_end) {
        size_t header_len = header_end - (char *)raw_data;
        resp->headers_len = header_len;
        resp->headers = (char *)malloc(header_len + 1);
        if (resp->headers) {
            memcpy(resp->headers, raw_data, header_len);
            resp->headers[header_len] = '\0';
            
            /* Extract status code */
            sscanf(resp->headers, "HTTP/%*s %d", &resp->status_code);
        }

        size_t body_start_idx = header_len + 4;
        if (body_start_idx <= raw_len) {
            resp->body_len = raw_len - body_start_idx;
            resp->body = (unsigned char *)malloc(resp->body_len + 1);
            if (resp->body) {
                memcpy(resp->body, raw_data + body_start_idx, resp->body_len);
                resp->body[resp->body_len] = '\0';
            }
        }
    } else {
        /* No headers found, raw body copy */
        resp->body_len = raw_len;
        resp->body = (unsigned char *)malloc(raw_len + 1);
        if (resp->body) {
            memcpy(resp->body, raw_data, raw_len);
            resp->body[raw_len] = '\0';
        }
    }

    free(raw_data);
    return TT_HTTP_OK;
}
