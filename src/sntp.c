/* sntp — set the TT's clock from an NTP server (one SNTP query), for FreeMiNT.
 *
 * Why: the TT booted with its clock at 22 Jun 1989 (2026-09-12). No LAN host answers NTP (router and
 * fractal's chrony were probed) and SpareMiNT's rdate speaks RFC 868 (port 37), which nothing on the
 * LAN serves. The TT has a default route, so this asks a public pool. Accuracy: +/- network delay.
 *
 * Usage: sntp [server]          (default pool.ntp.org; prints the time set, exits 1 on failure)
 * Build: m68k-atari-mint-gcc -m68020-60 -Os -s -o sntp src/sntp.c
 */
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define NTP_UNIX_OFFSET 2208988800UL

int main(int argc, char **argv)
{
    const char *server = argc > 1 ? argv[1] : "pool.ntp.org";
    /* Hard deadline: mint.cnf `exec`s this and waits, so a hang here stops the boot before inetd,
     * dropbear and the serial console (TT, 2026-09-13: stuck after the ext2 mount). The resolver has
     * no timeout, and MiNT may ignore SO_RCVTIMEO. SIGALRM's default action ends the process. */
    alarm(30);
    struct hostent *h = gethostbyname(server);
    if (!h) {
        fprintf(stderr, "sntp: cannot resolve %s\n", server);
        return 1;
    }
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons(123);
    memcpy(&a.sin_addr, h->h_addr_list[0], sizeof a.sin_addr);

    unsigned char pkt[48];
    for (int attempt = 0; attempt < 3; attempt++) {
        memset(pkt, 0, sizeof pkt);
        pkt[0] = 0x1b; /* LI 0, version 3, mode 3 (client) */
        struct timeval tv = { 5, 0 };
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
        if (sendto(s, pkt, sizeof pkt, 0, (struct sockaddr *)&a, sizeof a) != sizeof pkt)
            continue;
        if (recv(s, pkt, sizeof pkt, 0) < 48 || (pkt[0] & 7) != 4 || pkt[1] == 0)
            continue; /* not a server reply, or stratum 0 (kiss-of-death) */
        unsigned long secs = ((unsigned long)pkt[40] << 24) | (pkt[41] << 16) | (pkt[42] << 8) | pkt[43];
        struct timeval now = { (time_t)(secs - NTP_UNIX_OFFSET), 0 };
        if (settimeofday(&now, NULL) != 0) {
            perror("sntp: settimeofday");
            return 1;
        }
        printf("sntp: %s -> %s", server, ctime(&now.tv_sec));
        return 0;
    }
    fprintf(stderr, "sntp: no reply from %s\n", server);
    return 1;
}
