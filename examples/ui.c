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

unsigned char memory[2000*2000*4];

uint32_t drawcmd_count;
UIR_DrawCmd drawcmds[256];

unsigned char ttf[1<<20];
unsigned char bitmap[512*512];
stbtt_bakedchar chardata[96];

void draw_button(UIR_Rect *rect, float mouse_x, float mouse_y, const char *text) {
    drawcmds[drawcmd_count] = (UIR_DrawCmd) { .shape = {
        .type = UIR_DRAW_SHAPE_RECT,
        .outline_colour = { 150, 150, 150, 255 },
        .outline_radius = 3,
        .rect = *rect,
        .border_radius = 5,
    }};
    
    if (
        rect->x0 <= mouse_x && mouse_x <= rect->x1
        && rect->y0 <= mouse_y && mouse_y <= rect->y1
    ) {
        memcpy(drawcmds[drawcmd_count].shape.fill_colour, (uint8_t[4]) {30, 30, 30, 255}, 4);
    } else {
        memcpy(drawcmds[drawcmd_count].shape.fill_colour, (uint8_t[4]) {100, 100, 100, 255}, 4);
    }

    drawcmd_count++;

    float x = rect->x0 + 10.f;
    float y = rect->y1 - 15.f;
    for (; *text; text++) {
        char c = *text;
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

    rect->y0 += 60.f;
    rect->y1 += 60.f;
}

bool draw(UIR *uir, float mouse_x, float mouse_y) {
    drawcmd_count = 0;

    UIR_Rect rect = { 20, 20, 200, 70 };

    draw_button(&rect, mouse_x, mouse_y, "Test Button");
    draw_button(&rect, mouse_x, mouse_y, "Testing");
    draw_button(&rect, mouse_x, mouse_y, "Play");
    draw_button(&rect, mouse_x, mouse_y, "GO HERE");
    draw_button(&rect, mouse_x, mouse_y, "This cool");
    draw_button(&rect, mouse_x, mouse_y, "Goodbye");
    bool drawn = UIR_draw(uir, drawcmds, drawcmd_count);
    return drawn;
}

int main(void) {
    uint32_t width = W;
    uint32_t height = H;

    // --------------------------
    // Set up UIR

    UIR *uir = UIR_new(width, height, memory, sizeof(memory));
    if (!uir || uir->error_flags) {
        printf("err\n");
        exit(1);
    }
    memcpy(uir->clear_colour, (uint8_t[4]){ 73, 70, 70, 255 }, 4);

    // --------------------------
    // Load font

    fread(ttf, 1, 1<<20, fopen("Vera.ttf", "rb"));
    stbtt_BakeFontBitmap(ttf, 0, 32.f, bitmap, 512,512, 32,96, chardata);
    
    // --------------------------
    // Setup window
    
    RGFW_window* win = RGFW_createWindow("UI", 0, 0, (int)width, (int)height, 0);
    RGFW_window_setExitKey(win, RGFW_escape);
    RGFW_monitor mon = RGFW_window_getMonitor(win);

    #ifdef RGFW_WAYLAND
        mon.mode.w = width;
        mon.mode.h = height;
    #endif

    uint8_t *buffer = (uint8_t*)RGFW_ALLOC((u32)(mon.mode.w * mon.mode.h * 4));
    RGFW_surface* surface = RGFW_createSurface(buffer, mon.mode.w, mon.mode.h, RGFW_formatRGBA8);

    bool running = true;

    RGFW_event event;
    while (running) {
        bool blit = false;
        while (RGFW_window_checkEvent(win, &event)) {
            if (event.type == RGFW_quit)
                running = false;
            else if (event.type ==	RGFW_windowRefresh)
                blit = true;
            else if (event.type ==	RGFW_windowMoved)
                blit = true;
        }

        if (RGFW_window_isKeyPressed(win, RGFW_escape))
            running = false;

        int32_t w, h;
        RGFW_window_getSize(win, &w, &h);
        
        UIR_resize(uir, (uint32_t)w, (uint32_t)h);
        
        int32_t mouse_x, mouse_y;
        RGFW_window_getMouse(win, &mouse_x, &mouse_y);
        
        if (draw(uir, (float)mouse_x, (float)mouse_y)) {
            UIR_write_buffer_rgba(uir, buffer, (uint32_t)mon.mode.w * 4);
            blit = true;
        }
        
        if (blit)
            RGFW_window_blitSurface(win, surface);

        usleep(8000);
    }

    RGFW_surface_free(surface);
    RGFW_FREE(buffer);

    RGFW_window_close(win);
            
    return 0;
}
