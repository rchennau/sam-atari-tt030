/* ttmon — FR-5: publish the TT's health to SAM the way every SAM node does.
 *
 * Topic sam/node/telemetry/atari-tt030 (retained, QoS 0) on the main broker, same JSON fields as
 * src/sam/ops/maintenance/sam_telemetry.py (node_id, vram_free_mb, cpu_load, status) plus TT extras.
 * A retained MQTT "will" makes the broker publish status OFFLINE if the TT drops off the network.
 *
 * Broker address is the LAN IP: the router's DNS answers mqtt.sam.int with CT104's Tailscale address,
 * which the TT (not on the tailnet) cannot reach; 192.168.0.20:1883 is the same broker (2026-09-13).
 * # sam.int-exception: the TT resolves sam.int names to tailnet-only addresses.
 *
 * Usage: ttmon [broker-ip] [interval-seconds]      (defaults 192.168.0.20 60)
 * Build: m68k-atari-mint-gcc -m68020-60 -Os -s -Wall -o ttmon src/ttmon.c
 */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define NODE "atari-tt030"
#define TOPIC "sam/node/telemetry/" NODE

/* MQTT: string = 2-byte length + bytes */
static int put_str(unsigned char *p, const char *s)
{
    int n = (int)strlen(s);
    p[0] = n >> 8;
    p[1] = n & 0xff;
    memcpy(p + 2, s, n);
    return n + 2;
}

/* MQTT fixed header: type byte + variable-length "remaining length" */
static int put_hdr(unsigned char *p, int type, int len)
{
    int i = 0;
    p[i++] = type;
    do {
        unsigned char b = len % 128;
        len /= 128;
        p[i++] = b | (len ? 0x80 : 0);
    } while (len);
    return i;
}

static int send_all(int s, const unsigned char *b, int n)
{
    while (n > 0) {
        int w = write(s, b, n);
        if (w <= 0)
            return -1;
        b += w;
        n -= w;
    }
    return 0;
}

static int mqtt_connect(const char *ip)
{
    static const char *will = "{\"node_id\":\"" NODE "\",\"status\":\"OFFLINE\"}";
    unsigned char body[256], pkt[300];
    int s = socket(AF_INET, SOCK_STREAM, 0), n = 0, h;
    struct sockaddr_in a;

    if (s < 0)
        return -1;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons(1883);
    a.sin_addr.s_addr = inet_addr(ip);
    if (connect(s, (struct sockaddr *)&a, sizeof a) < 0) {
        close(s);
        return -1;
    }
    n += put_str(body + n, "MQTT");
    body[n++] = 4;                     /* protocol level 3.1.1 */
    body[n++] = 0x02 | 0x04 | 0x20;    /* clean session, will flag, will retain; will QoS 0 */
    body[n++] = 0;
    body[n++] = 180;                   /* keep-alive 180 s (we publish well within it) */
    n += put_str(body + n, NODE);
    n += put_str(body + n, TOPIC);
    n += put_str(body + n, will);
    h = put_hdr(pkt, 0x10, n);         /* CONNECT */
    memcpy(pkt + h, body, n);
    if (send_all(s, pkt, h + n) < 0 || read(s, pkt, 4) != 4 || pkt[0] != 0x20 || pkt[3] != 0) {
        close(s);                      /* no CONNACK, or CONNACK refused */
        return -1;
    }
    return s;
}

static int mqtt_publish(int s, const char *payload)
{
    unsigned char pkt[1024];
    int t = 2 + (int)strlen(TOPIC), p = (int)strlen(payload);
    int h = put_hdr(pkt, 0x31, t + p); /* PUBLISH, QoS 0, retain */

    put_str(pkt + h, TOPIC);
    memcpy(pkt + h + t, payload, p);
    return send_all(s, pkt, h + t + p);
}

/* first number after `key` in a /kern file (0 if missing) */
static long kern_val(const char *file, const char *key)
{
    char line[160];
    long v = 0;
    FILE *f = fopen(file, "r");

    if (!f)
        return 0;
    while (fgets(line, sizeof line, f))
        if (!key[0] || strncmp(line, key, strlen(key)) == 0) {
            v = strtol(line + strlen(key), NULL, 10);
            break;
        }
    fclose(f);
    return v;
}

/* CPU busy % since the previous call, from /kern/stat "cpu user nice system idle" */
static double cpu_busy(void)
{
    static unsigned long pu, pn, ps, pi;
    unsigned long u = 0, n = 0, s = 0, i = 0;
    double busy = 0;
    FILE *f = fopen("/kern/stat", "r");

    if (f) {
        if (fscanf(f, "cpu %lu %lu %lu %lu", &u, &n, &s, &i) == 4 && (u + n + s + i) > (pu + pn + ps + pi)) {
            unsigned long work = (u - pu) + (n - pn) + (s - ps), all = work + (i - pi);
            busy = all ? 100.0 * work / all : 0;
        }
        fclose(f);
    }
    pu = u; pn = n; ps = s; pi = i;
    return busy;
}

int main(int argc, char **argv)
{
    const char *ip = argc > 1 ? argv[1] : "192.168.0.20";
    int interval = argc > 2 ? atoi(argv[2]) : 60, s = -1;
    char msg[512], load[32] = "0";
    FILE *f;

    cpu_busy();                        /* prime the delta */
    for (;;) {
        if (s < 0 && (s = mqtt_connect(ip)) < 0) {
            sleep(30);
            continue;
        }
        if ((f = fopen("/kern/loadavg", "r"))) {
            if (fscanf(f, "%31s", load) != 1)
                strcpy(load, "0");
            fclose(f);
        }
        snprintf(msg, sizeof msg,
                 "{\"node_id\":\"" NODE "\",\"vram_free_mb\":0,\"cpu_load\":%.1f,\"status\":\"READY\","
                 "\"load1\":%s,\"uptime_s\":%ld,\"mem_free_kb\":%ld,\"tt_ram_free_kb\":%ld,"
                 "\"st_ram_free_kb\":%ld,\"os\":\"FreeMiNT 1.19\",\"cpu\":\"68030\"}",
                 cpu_busy(), load, kern_val("/kern/uptime", ""), kern_val("/kern/meminfo", "MemFree:"),
                 kern_val("/kern/meminfo", "FastFree:"), kern_val("/kern/meminfo", "CoreFree:"));
        if (mqtt_publish(s, msg) < 0) {  /* broker gone: reconnect next round */
            close(s);
            s = -1;
            continue;
        }
        sleep(interval);
    }
}
