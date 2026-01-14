#ifndef UIR_H
#define UIR_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#define UIR_TILE_SIZE 16
#define UIR_COPY_COLOUR(dst, src) memcpy(dst, src, 4)

enum {
    UIR_ERROR_NO_MEM = (1u << 0),
};

typedef uint8_t UIR_Tile[UIR_TILE_SIZE*UIR_TILE_SIZE][4];
typedef uint32_t UIR_Hash;

typedef struct UIR_TileInfo {
    UIR_Hash hash_old;
    UIR_Hash hash_new;
} UIR_TileInfo;

typedef struct UIR {
    // ----------------------
    // Read Only!

    unsigned char *memory;
    size_t memory_size;

    uint32_t width_in_px;
    uint32_t height_in_px;
    uint32_t width_in_tiles;
    uint32_t height_in_tiles;

    UIR_TileInfo *tile_info;
    UIR_Tile *tiles;
    uint32_t tile_count;

    // ----------------------
    // Read/Write

    uint32_t error_flags;
    uint8_t clear_colour[4];
} UIR;

// Returns minimum memory size that can fit this panel.
size_t UIR_minimum_memory_size(
    uint32_t width_in_px,
    uint32_t height_in_px
);

// !!!You must ensure that the memory is zeroed!!!
// Returns NULL if memory is too small for a UIR.
// Sets NO_MEM error_flag if memory is too small to fit the panel.
UIR *UIR_new(
    uint32_t width_in_px,
    uint32_t height_in_px,
    unsigned char *memory,
    size_t memory_size
);

// Returns true if the size has changed.
// Sets NO_MEM error_flag if memory is too small to fit the panel.
bool UIR_resize(
    UIR *uir,
    uint32_t width_in_px,
    uint32_t height_in_px
);

typedef enum UIR_DrawCmdType {
    UIR_DRAW_SHAPE_RECT,
    UIR_DRAW_SHAPE_CIRCLE,
    UIR_DRAW_IMAGE_A,
    UIR_DRAW_IMAGE_RGBA,

    UIR_DRAW_INTERNAL_FILL,
} UIR_DrawCmdType;

typedef struct UIR_Rect {
    float x0, y0, x1, y1;
} UIR_Rect;

typedef struct UIR_DrawCmd_Shape {
    uint32_t type;
    UIR_Rect rect;
    uint8_t fill_colour[4];
    uint8_t outline_colour[4];
    float outline_radius;
    float border_radius;
} UIR_DrawCmd_Shape;

typedef struct UIR_DrawCmd_Image {
    uint32_t type;
    UIR_Rect rect;
    uint8_t tint_colour[4];
    uint8_t *data;
    uint32_t data_stride;
    float scale;
} UIR_DrawCmd_Image;

typedef union UIR_DrawCmd {
    struct {
        uint32_t type;
        UIR_Rect rect;
    } common;
    UIR_DrawCmd_Shape shape;
    UIR_DrawCmd_Image image;
} UIR_DrawCmd;

// returns the number of tiles redrawn
uint32_t UIR_draw(
    UIR *uir,
    UIR_DrawCmd *draw_cmds,
    uint32_t draw_cmd_count
);

void UIR_write_buffer_rgba(
    UIR *uir,
    unsigned char *rgba_buffer,
    size_t row_stride_in_bytes
);

#endif
