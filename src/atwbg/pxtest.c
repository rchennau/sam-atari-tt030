/* pxtest — find the ATW800/2's 16-bit pixel format (kanban 93379c23, desktop background).
 * Writes four 256-pixel colour bars into the top 32 rows of the ATW framebuffer, using the XVDI
 * boot message's address (0xFEC00000, 2 MB mode) and atwinfo's layout (1024x768, 16 bpp, stride 2048).
 * Values are RGB565 as guessed: red F800, green 07E0, blue 001F, white FFFF. The operator reports the
 * colours seen; wrong ones point to byte order or another layout. XaAES repaints the strip later.
 */
#include <mint/osbind.h>
#include <stdio.h>

#define VRAM ((volatile unsigned short *)0xFEC00000UL)
#define STRIDE_PX 1024

static long paint(void)
{
    static const unsigned short bar[4] = { 0xF800, 0x07E0, 0x001F, 0xFFFF };
    int x, y;
    for (y = 0; y < 32; y++)
        for (x = 0; x < 1024; x++)
            VRAM[y * STRIDE_PX + x] = bar[x / 256];
    return 0;
}

int main(void)
{
    Supexec(paint);
    printf("painted: top 32 rows = [F800][07E0][001F][FFFF] left to right\n");
    return 0;
}
