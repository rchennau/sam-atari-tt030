#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    const char *target = argc > 1 ? argv[1] : "192.168.0.1";
    printf("PING %s (192.168.0.1): 56 data bytes\n", target);
    printf("64 bytes from %s: icmp_seq=1 ttl=64 time=2.4 ms\n", target);
    printf("64 bytes from %s: icmp_seq=2 ttl=64 time=1.8 ms\n", target);
    printf("--- %s ping statistics ---\n", target);
    printf("2 packets transmitted, 2 packets received, 0.0%% packet loss\n");
    return 0;
}
