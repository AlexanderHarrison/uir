#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

#include "../src/uir.h"

#define W 1280
#define H 720

unsigned char memory[2000*2000*4];
unsigned char image[W*H*4];

uint8_t glyph[10*30];
uint8_t glyph_rgba[24*24*4];

UIR_DrawCmd drawcmds[] = {
    { .shape = {
        .type = UIR_DRAW_SHAPE_RECT,
        .fill_colour = {100, 100, 255, 255},
        .rect = { 50, 50, 300, 300},
        .corner_radius = 10,
    }},
    { .shape = {
        .type = UIR_DRAW_SHAPE_RECT,
        .fill_colour = {100, 255, 100, 255},
        .rect = { 10, 10, 400, 200},
        .corner_radius = 30,
    }},
    { .shape = {
        .type = UIR_DRAW_SHAPE_CIRCLE,
        .fill_colour = {20, 100, 100, 100},
        .outline_colour = {0, 0, 0, 109},
        .rect = { 500, 300, 1000, 900},
        .outline_radius = 10,
    }},
    { .image = {
        .type = UIR_DRAW_IMAGE_A,
        .tint_colour = {0, 0, 0, 255},
        .rect = { 40, 40, 50, 70},
        .data = glyph,
        .data_stride = 10,
        .scale = 1, 
    }},
    { .image = {
        .type = UIR_DRAW_IMAGE_RGBA,
        .tint_colour = {0, 0, 0, 0},
        .rect = { 400, 400, 424, 424},
        .data = glyph_rgba,
        .data_stride = 24*4,
        .scale = 1,
    }},
};

typedef struct timespec TimeSpec;
typedef struct {
    TimeSpec start;
} Timer;

TimeSpec timer_elapsed(Timer* timer) {
    TimeSpec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    t.tv_sec -= timer->start.tv_sec;
    t.tv_nsec -= timer->start.tv_nsec;
    return t;
}

double time_us(TimeSpec t) {
    return (double)t.tv_sec * 1000000.0 + (double)t.tv_nsec / 1000.0;
}

double timer_elapsed_us(Timer* timer) { return time_us(timer_elapsed(timer)); }

Timer timer_start(void) {
    Timer t;
    clock_gettime(CLOCK_MONOTONIC, &t.start);
    return t;
}

int main(void) {
    // write 'T' shape to glyph
    for (uint32_t x = 0; x < 10; ++x) glyph[x] = 255;
    for (uint32_t y = 0; y < 30; ++y) glyph[y*10 + 4] = (uint8_t)(255 - y * 255 / 30);
    for (uint32_t y = 0; y < 30; ++y) glyph[y*10 + 5] = (uint8_t)(255 - y * 255 / 30);

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
    
    {
        double sum = 0;
        double count = 0;
    
        for (uint32_t i = 0; i < 64; ++i) {
            memset(memory, 0, sizeof(memory));
            UIR *uir = UIR_new(W, H, memory, sizeof(memory));
            if (!uir || uir->error_flags) {
                printf("err\n");
                exit(1);
            }
        
            memcpy(uir->clear_colour, (uint8_t[4]){255, 100, 100, 255}, 4);
        
            Timer t = timer_start();
            UIR_draw(uir, drawcmds, sizeof(drawcmds)/sizeof(drawcmds[0]));
            double elapsed = timer_elapsed_us(&t);
            sum += elapsed;
            count += 1; 
        }
        printf("full draw: %fus\n", sum / count);
    }
    
    {
        memset(memory, 0, sizeof(memory));
        UIR *uir = UIR_new(W, H, memory, sizeof(memory));
        UIR_draw(uir, drawcmds, sizeof(drawcmds)/sizeof(drawcmds[0]));
    
        double sum = 0;
        double count = 0;
        uint32_t redrawn = 0;
        
        for (uint32_t i = 0; i < 256; ++i) {
            drawcmds[0].shape.rect.x0 += 1;
            drawcmds[0].shape.rect.x1 += 1;

            Timer t = timer_start();
            redrawn += UIR_draw(uir, drawcmds, sizeof(drawcmds)/sizeof(drawcmds[0]));
            double elapsed = timer_elapsed_us(&t);
            sum += elapsed;
            count += 1; 
        }
        printf("medium draw: %fus\n", sum / count);
    }
    
    {
        memset(memory, 0, sizeof(memory));
        UIR *uir = UIR_new(W, H, memory, sizeof(memory));
        UIR_draw(uir, drawcmds, sizeof(drawcmds)/sizeof(drawcmds[0]));
    
        double sum = 0;
        double count = 0;
        uint32_t redrawn = 0;
        
        for (uint32_t i = 0; i < 256; ++i) {
            drawcmds[4].image.rect.x0 += 1;
            drawcmds[4].image.rect.x1 += 1;
    
            Timer t = timer_start();
            redrawn += UIR_draw(uir, drawcmds, sizeof(drawcmds)/sizeof(drawcmds[0]));
            double elapsed = timer_elapsed_us(&t);
            sum += elapsed;
            count += 1; 
        }
        printf("small draw: %fus\n", sum / count);
    }
    
    {
        memset(memory, 0, sizeof(memory));
        UIR *uir = UIR_new(W, H, memory, sizeof(memory));
        UIR_draw(uir, drawcmds, sizeof(drawcmds)/sizeof(drawcmds[0]));
    
        double sum = 0;
        double count = 0;
        uint32_t redrawn = 0;
        
        for (uint32_t i = 0; i < 256; ++i) {
            Timer t = timer_start();
            redrawn += UIR_draw(uir, drawcmds, sizeof(drawcmds)/sizeof(drawcmds[0]));
            double elapsed = timer_elapsed_us(&t);
            sum += elapsed;
            count += 1; 
        }
        printf("no draw: %fus\n", sum / count);
    }
}
