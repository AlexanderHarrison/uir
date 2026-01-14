#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

#include "../src/uir.h"

#define W 1280
#define H 720

unsigned char memory[2000*2000*4];
unsigned char image[W*H*4];

uint8_t glyph_rgba[24*24*4];

UIR_DrawCmd drawcmds[] = {
    { .shape = {
        .type = UIR_DRAW_SHAPE_RECT,
        .fill_colour = {100, 100, 255, 255},
        .rect = { 50, 50, 300, 300},
        .border_radius = 10,
    }},
    { .shape = {
        .type = UIR_DRAW_SHAPE_RECT,
        .fill_colour = {100, 255, 100, 255},
        .rect = { 10, 10, 400, 200},
        .border_radius = 30,
    }},
    { .image = {
        .type = UIR_DRAW_IMAGE_RGBA,
        .tint_colour = {0, 0, 0, 0},
        .rect = { 400, 400, 448, 448},
        .data = glyph_rgba,
        .data_stride = 24*4,
        .scale = 2.f,
    }},
};

int main(void) {
    // write gradient glyph_rgba
    for (uint32_t y = 0; y < 24; ++y) {
        for (uint32_t x = 0; x < 24; ++x) {
            uint32_t i = (y*24 + x)*4;
            glyph_rgba[i + 0] = 255;
            glyph_rgba[i + 1] = (uint8_t)y * 10;
            glyph_rgba[i + 2] = (uint8_t)x * 10;
            glyph_rgba[i + 3] = 255;
        }
    }
    
    UIR *uir = UIR_new(W, H, memory, sizeof(memory));
    if (!uir || uir->error_flags) {
        printf("err\n");
        exit(1);
    }

    memcpy(uir->clear_colour, (uint8_t[4]){255, 100, 100, 255}, 4);
    UIR_draw(uir, drawcmds, sizeof(drawcmds)/sizeof(drawcmds[0]));
    UIR_write_buffer_rgba(uir, image, W*4);

    FILE *f = fopen("test.ppm", "wb+");
    fprintf(f, "P6\n");
    fprintf(f, "%u %u\n", W, H);
    fprintf(f, "255\n");
    for (size_t i = 0; i < W*H; i++) {
        fputc(image[i*4+0], f);
        fputc(image[i*4+1], f);
        fputc(image[i*4+2], f);
    }
    fclose(f);
}
