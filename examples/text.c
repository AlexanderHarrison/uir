#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

#include "../src/uir.h"
#include "../vendor/stb_truetype.h"

#define W 1280
#define H 720

unsigned char memory[2000*2000*4];
unsigned char image[W*H*4];

uint32_t drawcmd_count;
UIR_DrawCmd drawcmds[16];

unsigned char ttf[1<<20];
unsigned char bitmap[512*512];
stbtt_bakedchar chardata[96];

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
    UIR *uir = UIR_new(W, H, memory, sizeof(memory));
    if (!uir || uir->error_flags) {
        printf("err\n");
        exit(1);
    }
    
    fread(ttf, 1, 1<<20, fopen("Vera.ttf", "rb"));
    stbtt_BakeFontBitmap(ttf, 0, 32.f, bitmap, 512,512, 32,96, chardata);
    
    char text[] = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', '!'};
    float x = 50.f;
    float y = 100.f;
    for (uint32_t i = 0; i < sizeof(text); ++i) {
        char c = text[i];
        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(chardata, 512,512, c-32, &x,&y,&q,1);
        
        drawcmds[drawcmd_count++] = (UIR_DrawCmd) { .image = {
            .type = UIR_DRAW_IMAGE_A,
            .tint_colour = { 255, 255, 255, 255 },
            .rect = { q.x0, q.y0, q.x1, q.y1 },
            .data = &bitmap[(uint32_t)(q.t0 * 512*512 + q.s0 * 512)],
            .data_stride = 512,
            .scale = 1,
        }};
    }
    

    memcpy(uir->clear_colour, (uint8_t[4]){ 255, 100, 100, 255 }, 4);

    Timer t = timer_start();
    UIR_draw(uir, drawcmds, sizeof(drawcmds)/sizeof(drawcmds[0]));
    double elapsed = timer_elapsed_us(&t); 
    printf("draw: %f\n", elapsed);
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
