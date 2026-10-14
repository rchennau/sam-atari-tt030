/* atwshot — save the ATW800/2 screen to a file (the inverse of atwbg), so the TT's display can be checked remotely.
 * Framebuffer 0xFEC00000, 1024x768, 16-bit RGB565 little-endian, stride 2048 bytes (atwbg.c, measured 2026-09-13).
 * Read-only: copies video memory in supervisor mode. Convert on fractal: src/atwbg/atwshot2png.py FILE.raw OUT.png
 * Usage: atwshot FILE.raw */
#include <mint/osbind.h>
#include <stdio.h>
#include <stdlib.h>

#define W 1024
#define H 768
#define VRAM ((volatile unsigned short *)0xFEC00000UL)

static unsigned short *img;

static long grab(void)
{
    long i;
    for (i = 0; i < (long)W * H; i++)
        img[i] = VRAM[i];
    return 0;
}

int main(int argc, char **argv)
{
    FILE *f;
    if (argc != 2) { fprintf(stderr, "usage: atwshot FILE.raw\n"); return 2; }
    if (!(img = malloc((size_t)W * H * 2))) { fprintf(stderr, "atwshot: no memory\n"); return 1; }
    Supexec(grab);
    if (!(f = fopen(argv[1], "wb")) || fwrite(img, 2, (size_t)W * H, f) != (size_t)W * H || fclose(f)) {
        perror(argv[1]);
        return 1;
    }
    return 0;
}
