/* ttmqtt — minimal MQTT 3.1.1 publish/subscribe for the TT (tt030-rpm-pipeline FR-4).
 *
 *   ttmqtt pub [-h host] [-r] [-u user -P pass] TOPIC MESSAGE
 *   ttmqtt sub [-h host] [-W secs] [-k] [-S] [-E substr]... [-u user -P pass] TOPIC_FILTER
 *
 * sub prints "TOPIC PAYLOAD" per message, flushed. By default it exits 0 after the FIRST message;
 * with -E it keeps going until a message whose topic contains one of the -E strings (then exit 0),
 * and with -k until -W expires. No (terminal) message within -W seconds (default 900) exits 1; any
 * connection/protocol failure exits 2. -W restarts on every message, so it bounds silence.
 * -S prints "SUBSCRIBED" once the broker acknowledges, so a caller can publish a request only
 * after its reply subscription is live (a fast reply is otherwise lost as a false timeout).
 * The yum client runs `sub -E /done/ -E /failed/ -E /queued/ sam/tt030/build/+/<request_id>` and
 * reads the verb from the topic, so no JSON is parsed on the 68030. QoS 0 throughout, as ttmon.
 *
 * Host defaults to the NAME mqtt.sam.int: the TT's /etc/hosts pins it to CT104's LAN address
 * (sam.int from the router is tailnet-only), so no IP lives in this binary.
 * Framing helpers are ttmon.c's. Build: m68k-atari-mint-gcc -m68020-60 -Os -s -Wall -o ttmqtt src/ttmqtt.c
 */
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

static int put_str(unsigned char *p, const char *s)
{
    int n = (int)strlen(s);
    p[0] = n >> 8;
    p[1] = n & 0xff;
    memcpy(p + 2, s, n);
    return n + 2;
}

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

/* Wait up to `secs` for data; 1 ready, 0 timeout, -1 error. secs < 0 waits forever. */
static int wait_readable(int s, long secs)
{
    fd_set r;
    struct timeval tv;
    FD_ZERO(&r);
    FD_SET(s, &r);
    tv.tv_sec = secs;
    tv.tv_usec = 0;
    return select(s + 1, &r, NULL, NULL, secs < 0 ? NULL : &tv);
}

static int read_all(int s, unsigned char *b, int n)
{
    while (n > 0) {
        int r = read(s, b, n);
        if (r <= 0)
            return -1;
        b += r;
        n -= r;
    }
    return 0;
}

/* Read one packet: returns the type byte, body in buf (NUL-terminated), length in *len. */
static int read_packet(int s, unsigned char *buf, int cap, int *len)
{
    unsigned char c;
    int type, mult = 1, n = 0, i;
    if (read_all(s, &c, 1) < 0)
        return -1;
    type = c;
    for (i = 0; i < 4; i++) {
        if (read_all(s, &c, 1) < 0)
            return -1;
        n += (c & 0x7f) * mult;
        mult *= 128;
        if (!(c & 0x80))
            break;
    }
    if (n >= cap || read_all(s, buf, n) < 0)
        return -1;
    buf[n] = 0;
    *len = n;
    return type;
}

static int mqtt_connect(const char *host, const char *user, const char *pass)
{
    unsigned char body[512], pkt[520];
    char id[24];
    int s, n = 0, h, len;
    struct sockaddr_in a;
    struct hostent *he = gethostbyname(host);

    if (!he) {
        fprintf(stderr, "ttmqtt: cannot resolve %s\n", host);
        return -1;
    }
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        return -1;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons(1883);
    memcpy(&a.sin_addr, he->h_addr_list[0], sizeof a.sin_addr);
    if (connect(s, (struct sockaddr *)&a, sizeof a) < 0) {
        fprintf(stderr, "ttmqtt: connect %s:1883 failed\n", host);
        close(s);
        return -1;
    }
    sprintf(id, "ttmqtt-%d", (int)getpid());
    n += put_str(body + n, "MQTT");
    body[n++] = 4;                                   /* 3.1.1 */
    body[n++] = 0x02 | (user ? 0x80 : 0) | (pass ? 0x40 : 0);   /* clean session */
    body[n++] = 0;
    body[n++] = 0;                                   /* keep-alive off: -W bounds a sub */
    n += put_str(body + n, id);
    if (user)
        n += put_str(body + n, user);
    if (pass)
        n += put_str(body + n, pass);
    h = put_hdr(pkt, 0x10, n);
    memcpy(pkt + h, body, n);
    if (send_all(s, pkt, h + n) < 0 || read_packet(s, body, sizeof body, &len) != 0x20
        || len != 2 || body[1] != 0) {
        fprintf(stderr, "ttmqtt: CONNACK missing or refused (code %d)\n", len == 2 ? body[1] : -1);
        close(s);
        return -1;
    }
    return s;
}

static int usage(void)
{
    fprintf(stderr, "usage: ttmqtt pub [-h host] [-r] [-u user -P pass] TOPIC MESSAGE\n"
                    "       ttmqtt sub [-h host] [-W secs] [-k] [-S] [-E substr]... [-u user -P pass] TOPIC_FILTER\n");
    return 2;
}

int main(int argc, char **argv)
{
    const char *host = "mqtt.sam.int", *user = NULL, *pass = NULL;
    long wait = 900;
    int retain = 0, keep = 0, announce = 0, nstop = 0, i, s, sub;
    const char *stop[8];
    static unsigned char pkt[8192];

    if (argc < 2)
        return usage();
    sub = !strcmp(argv[1], "sub");
    if (!sub && strcmp(argv[1], "pub"))
        return usage();
    for (i = 2; i < argc && argv[i][0] == '-'; i++) {
        if (!strcmp(argv[i], "-r"))
            retain = 1;
        else if (!strcmp(argv[i], "-k"))
            keep = 1;
        else if (!strcmp(argv[i], "-S"))
            announce = 1;
        else if (i + 1 >= argc)
            return usage();
        else if (!strcmp(argv[i], "-h"))
            host = argv[++i];
        else if (!strcmp(argv[i], "-W"))
            wait = atol(argv[++i]);
        else if (!strcmp(argv[i], "-u"))
            user = argv[++i];
        else if (!strcmp(argv[i], "-P"))
            pass = argv[++i];
        else if (!strcmp(argv[i], "-E") && nstop < 8)
            stop[nstop++] = argv[++i];
        else
            return usage();
    }
    if (argc - i != (sub ? 1 : 2) || strlen(argv[i]) > 1000 || (!sub && strlen(argv[i + 1]) > 6000))
        return usage();
    if ((s = mqtt_connect(host, user, pass)) < 0)
        return 2;

    if (!sub) {
        int t = 2 + (int)strlen(argv[i]), p = (int)strlen(argv[i + 1]);
        int h = put_hdr(pkt, 0x30 | retain, t + p);  /* PUBLISH QoS 0 */
        put_str(pkt + h, argv[i]);
        memcpy(pkt + h + t, argv[i + 1], p);
        if (send_all(s, pkt, h + t + p) < 0)
            return 2;
        send_all(s, (const unsigned char *)"\xe0\x00", 2);
        close(s);
        return 0;
    } else {
        unsigned char body[1100];
        int n = 0, h, len, type;
        body[n++] = 0;
        body[n++] = 1;                               /* packet id 1 */
        n += put_str(body + n, argv[i]);
        body[n++] = 0;                               /* requested QoS 0 */
        h = put_hdr(pkt, 0x82, n);                   /* SUBSCRIBE */
        memcpy(pkt + h, body, n);
        if (send_all(s, pkt, h + n) < 0 || read_packet(s, pkt, sizeof pkt, &len) != 0x90
            || len != 3 || pkt[2] == 0x80) {
            fprintf(stderr, "ttmqtt: SUBACK missing or refused\n");
            return 2;
        }
        if (announce) {
            puts("SUBSCRIBED");
            fflush(stdout);
        }
        for (;;) {
            int r = wait_readable(s, wait);
            if (r == 0) {
                close(s);
                return 1;                            /* timeout: distinct from success and error */
            }
            if (r < 0 || (type = read_packet(s, pkt, sizeof pkt, &len)) < 0)
                return 2;
            if ((type & 0xf0) == 0x30) {             /* PUBLISH (QoS 0: no packet id) */
                int tl = (pkt[0] << 8) | pkt[1], j, done = !keep && !nstop;
                char save;
                if (tl + 2 > len)
                    return 2;
                fwrite(pkt + 2, 1, tl, stdout);
                putchar(' ');
                fwrite(pkt + 2 + tl, 1, len - 2 - tl, stdout);
                putchar('\n');
                fflush(stdout);
                save = pkt[2 + tl];
                pkt[2 + tl] = 0;                     /* topic as a C string for the -E match */
                for (j = 0; j < nstop; j++)
                    if (strstr((char *)pkt + 2, stop[j]))
                        done = 1;
                pkt[2 + tl] = save;
                if (done) {
                    send_all(s, (const unsigned char *)"\xe0\x00", 2);
                    close(s);
                    return 0;
                }
            }
        }
    }
}
