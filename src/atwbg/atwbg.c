/* atwbg — show a picture full-screen on the ATW800/2 so XaAES can keep it as the desktop background
 * (press Ctrl+Alt+; while it is on screen). Kanban 93379c23.
 * Input: a raw 1024x768 16-bit file made by src/atwbg/img2atw.py in the ATW's layout, measured
 * 2026-09-13: standard RGB565 with the bytes in little-endian (Intel) order.
 * Framebuffer 0xFEC00000 (XVDI boot message, 2 MB mode), stride 2048 bytes.
 * Usage: atwbg FILE.raw
 */
#include <mint/osbind.h>
#include <stdio.h>
#include <stdlib.h>

#define W 1024
#define H 768
#define VRAM ((volatile unsigned short *)0xFEC00000UL)

static unsigned short *img;

static long blit(void)
{
    long i;
    for (i = 0; i < (long)W * H; i++)
        VRAM[i] = img[i];
    return 0;
}

int main(int argc, char **argv)
{
    FILE *f;
    if (argc != 2) {
        fprintf(stderr, "usage: atwbg FILE.raw   (1024x768, 16-bit, from img2atw.py)\n");
        return 2;
    }
    if (!(img = malloc((long)W * H * 2)) || !(f = fopen(argv[1], "rb"))) {
        perror(argv[1]);
        return 1;
    }
    if (fread(img, 2, (long)W * H, f) != (size_t)W * H) {
        fprintf(stderr, "%s: not a 1024x768 16-bit raw file\n", argv[1]);
        return 1;
    }
    fclose(f);
    Supexec(blit);
    printf("on screen: press Ctrl+Alt+; to keep it as the XaAES desktop background\n");
    return 0;
}
