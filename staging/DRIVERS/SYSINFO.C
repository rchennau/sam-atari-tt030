#include <stdio.h>
#include <osbind.h>

typedef struct {
    unsigned long tag;
    unsigned long val;
} COOKIE;

int main() {
    printf("=== ATARI TT030 HARDWARE PROBE ===\r\n");
    COOKIE *cookie = *(COOKIE **)0x5A0;
    if (!cookie) {
        printf("No Cookie Jar found.\r\n");
        return 0;
    }
    printf("Cookie Jar Entries:\r\n");
    while (cookie->tag != 0) {
        char tag_str[5] = {0};
        *(unsigned long*)tag_str = cookie->tag;
        printf("  %s : 0x%08LX\r\n", tag_str, cookie->val);
        cookie++;
    }
    return 0;
}
