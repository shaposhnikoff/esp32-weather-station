#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

enum { WIDTH = 260, HEIGHT = 420 };
static uint8_t pixels[HEIGHT][WIDTH][3];

static void paint(int x, int y, int r, int g, int b)
{
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
    pixels[y][x][0] = (uint8_t)r;
    pixels[y][x][1] = (uint8_t)g;
    pixels[y][x][2] = (uint8_t)b;
}

static void mountain(int peak_x, int peak_y, int bottom_y, int half_width, int shade, int night)
{
    for (int y = peak_y; y < bottom_y; ++y) {
        const double fraction = (double)(y - peak_y) / (bottom_y - peak_y);
        const int width = (int)(half_width * fraction);
        for (int x = peak_x - width; x <= peak_x + width; ++x) {
            int r = shade, g = shade + 28, b = shade + 48;
            if (x < peak_x) { r += 14; g += 14; b += 14; }
            if (fraction < 0.24 + 0.06 * sin(x * 0.1)) {
                r = 209; g = 229; b = 239;
            }
            if (night) { r /= 3; g /= 3; b = b / 3 + 12; }
            paint(x, y, r, g, b);
        }
    }
}

static void landscape(int night)
{
    for (int y = 0; y < HEIGHT; ++y)
        for (int x = 0; x < WIDTH; ++x) {
            const double f = (double)y / HEIGHT;
            paint(x, y, night ? 7 : 32 + (int)(90 * f),
                night ? 18 + (int)(18 * f) : 101 + (int)(75 * f),
                night ? 43 + (int)(25 * f) : 178 + (int)(45 * f));
        }
    if (night) {
        for (int i = 0; i < 35; ++i)
            paint((i * 73 + 17) % WIDTH, (i * 37 + 11) % 155, 177, 198, 212);
    } else {
        for (int y = 30; y < 145; ++y)
            for (int x = 0; x < WIDTH; ++x)
                if (sin(x * .027 + y * .09) + sin(x * .04 - y * .07) > 1.75)
                    paint(x, y, 194, 218, 231);
    }
    mountain(70, 153, 306, 154, 105, night);
    mountain(220, 173, 313, 130, 80, night);
    mountain(142, 203, 324, 113, 59, night);
    for (int y = 305; y < HEIGHT; ++y)
        for (int x = 0; x < WIDTH; ++x) {
            const int ripple = (int)(5 * sin(y * .7 + x * .04));
            paint(x, y, night ? 6 : 19, night ? 20 : 71 + ripple, night ? 33 : 89 + ripple);
        }
    for (int i = 0; i < 22; ++i) {
        const int x = (i & 1) ? 235 + i % 22 : i % 30;
        const int base = 295 + (i * 17) % 125;
        const int height = 40 + (i * 19) % 90;
        for (int y = base - height; y < base; ++y) {
            const int width = (y - base + height) / 4 + (y % 12) / 3;
            for (int px = x - width; px <= x + width; ++px)
                paint(px, y, night ? 3 : 8, night ? 13 : 40, night ? 19 : 39);
        }
    }
}

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    FILE *file = fopen(argv[1], "w");
    if (!file) return 1;
    fputs("/* Generated original artwork. Regenerate with make assets. */\n#include \"lvgl.h\"\n", file);
    for (int night = 0; night <= 1; ++night) {
        landscape(night);
        const char *name = night ? "night" : "day";
        fprintf(file, "static const uint8_t landscape_%s_pixels[] = {\n", name);
        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                const uint16_t color = (uint16_t)((pixels[y][x][0] >> 3) << 11) |
                    (uint16_t)((pixels[y][x][1] >> 2) << 5) | (pixels[y][x][2] >> 3);
                fprintf(file, "0x%02x,0x%02x,", color & 255, color >> 8);
            }
            fputc('\n', file);
        }
        fprintf(file, "};\nconst lv_img_dsc_t landscape_%s = {"
            ".header={.cf=LV_IMG_CF_TRUE_COLOR,.w=260,.h=420},"
            ".data_size=218400,.data=landscape_%s_pixels};\n", name, name);
    }
    return fclose(file) == 0 ? 0 : 1;
}
