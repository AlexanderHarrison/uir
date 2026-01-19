#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "../src/uir.h"
#include "../vendor/stb_truetype.h"
#include "../vendor/RGFW.h"

#define W 1280
#define H 720

unsigned char memory[W*H*8];
unsigned char image_rgb[W*H*3];

uint32_t drawcmd_count;
UIR_DrawCmd drawcmds[W*H*2];

unsigned char ttf[1<<20];
unsigned char bitmap[512*512];
stbtt_bakedchar chardata[96];

int main(void) {
    // --------------------------
    // Load font

    fread(ttf, 1, 1<<20, fopen("Vera.ttf", "rb"));
    stbtt_BakeFontBitmap(ttf, 0, 16.f, bitmap, 512,512, 32,96, chardata);
    
    // --------------------------
    // Make drawcmd array

    static const char chars[36] = "qwertyuiopasdfghjklzxcvbnm1234567890";
    
    uint32_t char_i = 0;
    for (float y = 0; y < H; y += 8.f) {
        for (float x = 0; x < W; x += 3.f) {
            char c = chars[char_i++ % 36];
            
            float px = x; float py = y;
            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(chardata, 512,512, c-32, &px,&py,&q,1);
        
            drawcmds[drawcmd_count++] = (UIR_DrawCmd) { .image = {
                .type = UIR_DRAW_IMAGE_A,
                .tint_colour = {
                    (uint8_t)((x + y) * 12721),
                    (uint8_t)(x * y * 14741),
                    (uint8_t)(x * y * 10619863),
                    255
                },
                .rect = { q.x0 + x, q.y0 + y, q.x1 + x, q.y1 + y },
                .data = &bitmap[(uint32_t)(q.t0 * 512*512 + q.s0 * 512)],
                .data_stride = 512,
                .scale = 1,
            }};
        }
    }
    
    printf("drawcmd_count: %u\n", drawcmd_count);
    
    // --------------------------
    // Run benchmark
    
    double sum = 0;
    double count = 0;
    
    UIR *uir;
    for (uint32_t i = 0; i < 100; ++i) {
        memset(memory, 0, sizeof(memory));
        uir = UIR_new(W, H, memory, sizeof(memory));
        uir->clear_colour = (RGBA) { 73, 70, 70, 255 };
    
        struct timespec t, s;
        clock_gettime(CLOCK_MONOTONIC, &t);
        UIR_draw(uir, drawcmds, sizeof(drawcmds)/sizeof(drawcmds[0]));
        clock_gettime(CLOCK_MONOTONIC, &s);
        sum += (double)(s.tv_sec - t.tv_sec) * 1000.0 + (double)(s.tv_nsec - t.tv_nsec) / 1000000.0;
        count += 1;
    }
    
    printf("render: %fms\n", sum / count);

    UIR_write_buffer_rgb(uir, image_rgb, W*3);
    FILE *f = fopen("test.ppm", "wb+");
    fprintf(f, "P6\n");
    fprintf(f, "%u %u\n", W, H);
    fprintf(f, "255\n");
    for (size_t i = 0; i < W*H*3; i++)
        fputc(image_rgb[i], f);
    fclose(f);
    return 0;
}
