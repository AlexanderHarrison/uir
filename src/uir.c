
#include "uir.h"

#include <stdalign.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#define ALIGN_UP(p, align) (void*)(((uintptr_t)(p) + ((uintptr_t)align) - 1) & ~(((uintptr_t)align) - 1))
#define ALIGN_DOWN(p, align) (void*)((uintptr_t)(p) & ~((align)-1))

static void UIR_check_tiles_fit(UIR *uir) {
    uint32_t required_tile_count = uir->width_in_tiles * uir->height_in_tiles;
    if (required_tile_count > uir->tile_count)
        uir->error_flags |= UIR_ERROR_NO_MEM;
}

static UIR_Hash UIR_murmur32_scramble(uint32_t k) {
    k *= 0xcc9e2d51;
    k = (k << 15) | (k >> 17);
    k *= 0x1b873593;
    return k;
}

static UIR_Hash UIR_hash(
    unsigned char *data,
    size_t size
) {
    uint32_t h = 0x1b873593;
    uint32_t k;
    for (size_t i = size >> 2; i; i--) {
        memcpy(&k, data, sizeof(uint32_t));
        data += sizeof(uint32_t);
        h ^= UIR_murmur32_scramble(k);
        h = (h << 13) | (h >> 19);
        h = h * 5 + 0xe6546b64;
    }
    k = 0;
    for (size_t i = size & 3; i; i--) {
        k <<= 8;
        k |= data[i - 1];
    }
    h ^= UIR_murmur32_scramble(k);
    h ^= (uint32_t)size;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

static UIR_Hash UIR_hash_draw_cmd(
    UIR_DrawCmd *cmd
) {
    return UIR_hash((unsigned char*)cmd, sizeof(*cmd));
}

size_t UIR_minimum_memory_size(
    uint32_t width_in_px,
    uint32_t height_in_px
) {
    uint32_t width_in_tiles = (width_in_px + UIR_TILE_SIZE - 1) / UIR_TILE_SIZE;
    uint32_t height_in_tiles = (height_in_px + UIR_TILE_SIZE - 1) / UIR_TILE_SIZE;
    uint32_t tile_count = width_in_tiles * height_in_tiles;

    return sizeof(UIR) * 2 // double to ensure we can align UIR upwards
        + sizeof(UIR_TileInfo) // add to ensure we can align UIR_TileInfo upwards
        + (size_t)tile_count * (sizeof(UIR_Tile) + sizeof(UIR_TileInfo));
}

UIR *UIR_new(
    uint32_t width_in_px,
    uint32_t height_in_px,
    unsigned char *memory,
    size_t memory_size
) {
    unsigned char *memory_end = memory + memory_size;

    // -----------------------------
    // allocate UIR at start of memory

    unsigned char *uir_addr = ALIGN_UP(memory, alignof(UIR));
    if (uir_addr + sizeof(UIR) > memory_end)
        return NULL;
        
    UIR *uir = (UIR*)uir_addr;
    *uir = (UIR) {
        .memory = memory,
        .memory_size = memory_size,
    };
    
    // ----------------------------
    // split remaining memory between tiles and tile hashes  

    unsigned char *memory_left = ALIGN_UP(uir_addr + sizeof(UIR), alignof(UIR_TileInfo));
    size_t memory_size_left = (size_t)(memory_end - memory_left);

    size_t tile_total_size = sizeof(UIR_TileInfo) + sizeof(UIR_Tile);
    uir->tile_count = (uint32_t)(memory_size_left / tile_total_size);

    uir->tile_info = (UIR_TileInfo*)memory_left;
    memory_left += sizeof(UIR_TileInfo) * (size_t)uir->tile_count;
    uir->tiles = (UIR_Tile*)memory_left;
    
    // ---------------------------
    // resize

    UIR_resize(uir, width_in_px, height_in_px);

    return uir;
}

bool UIR_resize(
    UIR *uir,
    uint32_t width_in_px,
    uint32_t height_in_px
) {
    bool changed = uir->width_in_px != width_in_px || uir->height_in_px != height_in_px;

    uir->width_in_px = width_in_px;
    uir->height_in_px = height_in_px;
    uir->width_in_tiles = (width_in_px + UIR_TILE_SIZE - 1) / UIR_TILE_SIZE;
    uir->height_in_tiles = (height_in_px + UIR_TILE_SIZE - 1) / UIR_TILE_SIZE;

    UIR_check_tiles_fit(uir);
    
    return changed;
}

static inline bool UIR_rect_inside(
    UIR_Rect *a,
    UIR_Rect *b
) {
    bool x_ab = a->x1 <= b->x1;
    bool x_ba = b->x0 <= a->x0;
    bool y_ab = a->y1 <= b->y1;
    bool y_ba = b->y0 <= a->y0;
    return x_ab & x_ba & y_ab & y_ba;
}

static inline bool UIR_rect_intersect(
    UIR_Rect *a,
    UIR_Rect *b
) {
    bool x_ab = a->x0 < b->x1;
    bool x_ba = b->x0 < a->x1;
    bool y_ab = a->y0 < b->y1;
    bool y_ba = b->y0 < a->y1;
    return x_ab & x_ba & y_ab & y_ba;
}

static inline float UIR_abs(float n) { return fabsf(n); }
static inline float UIR_min(float n, float m) { return n < m ? n : m; }
static inline float UIR_max(float n, float m) { return n > m ? n : m; }
static inline float UIR_clamp(float n, float min, float max) { return UIR_min(UIR_max(n, min), max); }
static inline float UIR_length(float n, float m) { return sqrtf(n*n + m*m); }

static inline float UIR_rounded_rect(
    float px, float py,
    float bx, float by,
    float r
) {
    float qx = UIR_abs(px) - bx + r;
    float qy = UIR_abs(py) - by + r;
    return UIR_min(UIR_max(qx, qy), 0) + UIR_length(UIR_max(qx, 0), UIR_max(qy, 0)) - r;
}

static inline float UIR_circle(
    float px, float py,
    float radius
) {
    return UIR_length(px, py) - radius;
}

// Performs premultiplied blending
static inline void UIR_blend(
    uint8_t dst[4],
    uint8_t c[4],
    float c_alpha
) {
    float r255 = 0.00392156862745098f;
    float dst_factor = 1.f - (float)c[3] * r255 * c_alpha;
    
    for (uint32_t i = 0; i < 4; ++i) {
        float dst_f = (float)dst[i];
        float c_f = (float)c[i];
        float dst_c_f = c_f * c_alpha + dst_f * dst_factor;
        dst[i] = (uint8_t)dst_c_f;
    }
}

// Performs 2 consecutive premultiplied blends
static inline void UIR_blend2(
    uint8_t dst[4],
    uint8_t c1[4],
    uint8_t c2[4],
    float c1_alpha,
    float c2_alpha
) {
    float r255 = 0.00392156862745098f;
    float dst_factor_1 = 1.f - (float)c1[3] * r255 * c1_alpha;
    float dst_factor_2 = 1.f - (float)c2[3] * r255 * c2_alpha;
    
    for (uint32_t i = 0; i < 4; ++i) {
        float dst_f = (float)dst[i];
        float c1_f = (float)c1[i];
        float c2_f = (float)c2[i];

        float dst_c1_f = c1_f * c1_alpha + dst_f * dst_factor_1;
        float dst_c1_c2_f = c2_f * c2_alpha + dst_c1_f * dst_factor_2;

        dst[i] = (uint8_t)dst_c1_c2_f;
    }
}

static inline void UIR_pick_colour(
    uint8_t dst[4],
    uint8_t outline[4],
    uint8_t fill[4],
    float outline_radius,
    float r
) {
    // https://www.desmos.com/calculator/hpskoyrzwl 
    float outline_factor = UIR_clamp(outline_radius - UIR_abs(r + outline_radius), 0, 1);
    float fill_factor = UIR_clamp(UIR_min(1, outline_radius) - outline_radius * 2.f - r, 0, 1);
    UIR_blend2(dst, outline, fill, outline_factor, fill_factor);
}

static void UIR_tile_draw_cmd(
    UIR_Tile tile,
    UIR_Rect *rect,
    UIR_DrawCmd *cmd
) {
    switch (cmd->common.type) {
        case UIR_DRAW_SHAPE_RECT: {
            UIR_DrawCmd_Shape *shape = &cmd->shape;
        
            float w2 = (shape->rect.x1 - shape->rect.x0) * 0.5f;
            float h2 = (shape->rect.y1 - shape->rect.y0) * 0.5f;

            uint32_t i = 0;
            for (float y = rect->y0; y < rect->y1; y += 1.f) {
                for (float x = rect->x0; x < rect->x1; x += 1.f) {
                    float r = UIR_rounded_rect(
                        x - (shape->rect.x0 + w2),
                        y - (shape->rect.y0 + h2),
                        w2, h2,
                        shape->border_radius
                    );

                    UIR_pick_colour(tile[i], shape->outline_colour, shape->fill_colour, shape->outline_radius, r);

                    i++;
                }
            }
        } break;
        case UIR_DRAW_SHAPE_CIRCLE: {
            UIR_DrawCmd_Shape *shape = &cmd->shape;
        
            float w2 = (shape->rect.x1 - shape->rect.x0) * 0.5f;
            float h2 = (shape->rect.y1 - shape->rect.y0) * 0.5f;
            float radius = UIR_min(w2, h2);
        
            uint32_t i = 0;
            for (float y = rect->y0; y < rect->y1; y += 1.f) {
                for (float x = rect->x0; x < rect->x1; x += 1.f) {
                    float r = UIR_circle(
                        x - (shape->rect.x0 + w2),
                        y - (shape->rect.y0 + h2),
                        radius
                    );
    
                    UIR_pick_colour(tile[i], shape->outline_colour, shape->fill_colour, shape->outline_radius, r);

                    i++;
                }
            }
        } break;
        case UIR_DRAW_IMAGE_A: {
            UIR_DrawCmd_Image *image = &cmd->image;
            
            float x0 = UIR_max(image->rect.x0, rect->x0);
            float y0 = UIR_max(image->rect.y0, rect->y0);
            float x1 = UIR_min(image->rect.x1, rect->x1);
            float y1 = UIR_min(image->rect.y1, rect->y1);
            
            float w = x1 - x0;
            float h = y1 - y0;
            
            uint32_t tile_x_start = (uint32_t)(x0 - rect->x0);
            uint32_t tile_y = (uint32_t)(y0 - rect->y0);
            
            float image_x_start = x0 - image->rect.x0;
            float image_y_start = y0 - image->rect.y0;

            float recip_scale = 1.f / image->scale;

            for (float y = 0; y < h; y += 1.f) {
                uint32_t tile_x = tile_x_start;

                for (float x = 0; x < w; x += 1.f) {
                    float image_x = (image_x_start + x) * recip_scale;
                    float image_y = (image_y_start + y) * recip_scale;
                    
                    uint32_t image_xi = (uint32_t)image_x;
                    uint32_t image_yi = (uint32_t)image_y;
                    uint32_t image_i = image_yi * image->data_stride + image_xi;

                    uint8_t alpha = image->data[image_i];
                    float r255 = 0.00392156862745098f;
                    UIR_blend(tile[tile_y*UIR_TILE_SIZE + tile_x], image->tint_colour, (float)alpha * r255);

                    tile_x++;
                }

                tile_y++;
            }
        } break;
        case UIR_DRAW_IMAGE_RGBA: {
            UIR_DrawCmd_Image *image = &cmd->image;
            
            float x0 = UIR_max(image->rect.x0, rect->x0);
            float y0 = UIR_max(image->rect.y0, rect->y0);
            float x1 = UIR_min(image->rect.x1, rect->x1);
            float y1 = UIR_min(image->rect.y1, rect->y1);
            
            float w = x1 - x0;
            float h = y1 - y0;
            
            uint32_t tile_x_start = (uint32_t)(x0 - rect->x0);
            uint32_t tile_y = (uint32_t)(y0 - rect->y0);
            
            float image_x_start = x0 - image->rect.x0;
            float image_y_start = y0 - image->rect.y0;
            
            float recip_scale = 1.f / image->scale;

            for (float y = 0; y < h; y += 1.f) {
                uint32_t tile_x = tile_x_start;

                for (float x = 0; x < w; x += 1.f) {
                    float image_x = (image_x_start + x) * recip_scale;
                    float image_y = (image_y_start + y) * recip_scale;
                    
                    uint32_t image_xi = (uint32_t)image_x;
                    uint32_t image_yi = (uint32_t)image_y;
                    uint32_t image_i = image_yi * image->data_stride + image_xi*4;

                    UIR_blend2(
                        tile[tile_y*UIR_TILE_SIZE + tile_x],
                        &image->data[image_i],
                        image->tint_colour,
                        1.f,
                        1.f
                    );

                    tile_x++;
                }

                tile_y++;
            }
        } break;
    }
}

// static void UIR_fill_over_tile(
//     UIR_Tile tile,
//     uint8_t colour[4]
// ) {
//     for (uint32_t i = 0; i < UIR_TILE_SIZE*UIR_TILE_SIZE; ++i)
//         UIR_blend(tile[i], colour, 1);
// }

static void UIR_fill_tile(
    UIR_Tile tile,
    uint8_t colour[4]
) {
    // Loop is manually unrolled for better codegen. It's equivalent to this code:
    // for (uint32_t i = 0; i < UIR_TILE_SIZE*UIR_TILE_SIZE; ++i)
    //     memcpy(uir->tiles[tile_idx][i], uir->clear_colour, 4);

    uint8_t colour_x4[16];
    memcpy(&colour_x4[0], colour, 4);
    memcpy(&colour_x4[4], colour, 4);
    memcpy(&colour_x4[8], colour, 4);
    memcpy(&colour_x4[12], colour, 4);

    uint8_t *tile_ptr = tile[0];
    for (uint32_t i = 0; i < UIR_TILE_SIZE*UIR_TILE_SIZE*4; i += 16)
        memcpy(&tile_ptr[i], colour_x4, 16);
}

static bool UIR_draw_cmd_is_fill(
    uint8_t fill_colour[4],
    UIR_Rect *tile_rect,
    UIR_DrawCmd *cmd
) {
    switch (cmd->common.type) {
        case UIR_DRAW_SHAPE_RECT: {
            UIR_DrawCmd_Shape *shape = &cmd->shape;
            
            float nonfill_size = shape->outline_radius * 2.f + shape->border_radius + 2.f;
            UIR_Rect fill_rect = {
                shape->rect.x0 + nonfill_size,
                shape->rect.y0 + nonfill_size,
                shape->rect.x1 - nonfill_size,
                shape->rect.y1 - nonfill_size,
            };

            memcpy(fill_colour, shape->fill_colour, 4);
            return UIR_rect_inside(tile_rect, &fill_rect);
        }

        case UIR_DRAW_SHAPE_CIRCLE: {
            UIR_DrawCmd_Shape *shape = &cmd->shape;
            
            float nonfill_radius = shape->outline_radius * 2.f + 2.f;
            
            float w2 = (shape->rect.x1 - shape->rect.x0) * 0.5f;
            float h2 = (shape->rect.y1 - shape->rect.y0) * 0.5f;
            float radius = UIR_min(w2, h2);
            float fill_radius = radius - nonfill_radius;
            float fill_radius_sq = fill_radius * fill_radius;
            
            float x = shape->rect.x0 + w2;
            float y = shape->rect.y0 + h2;

            float corners_x[4] = { tile_rect->x0, tile_rect->x0, tile_rect->x1, tile_rect->x1 };
            float corners_y[4] = { tile_rect->y0, tile_rect->y1, tile_rect->y0, tile_rect->y1 };
            float corners_dist_sq[4];

            for (uint32_t i = 0; i < 4; ++i) {
                corners_x[i] -= x;
                corners_y[i] -= y;
                corners_x[i] *= corners_x[i];
                corners_y[i] *= corners_y[i];
                corners_dist_sq[i] = corners_x[i] + corners_y[i];
            }
            
            bool all_inside = true;
            for (uint32_t i = 0; i < 4; ++i)
                all_inside &= corners_dist_sq[i] <= fill_radius_sq;

            memcpy(fill_colour, shape->fill_colour, 4);
            return all_inside;
        } break;

        default:
            return false;
    }
}

static void UIR_tile_draw(
    UIR *uir,
    UIR_DrawCmd *draw_cmds,
    uint32_t draw_cmd_count,
    uint32_t tile_x,
    uint32_t tile_y
) {
    uint32_t tile_idx = tile_y * uir->width_in_tiles + tile_x;

    UIR_Rect tile_rect = {
        .x0 = (float)(tile_x * UIR_TILE_SIZE),
        .y0 = (float)(tile_y * UIR_TILE_SIZE),
        .x1 = (float)((tile_x + 1) * UIR_TILE_SIZE),
        .y1 = (float)((tile_y + 1) * UIR_TILE_SIZE),
    };

    UIR_DrawCmd *draw_cmds_start = draw_cmds;
    UIR_DrawCmd *draw_cmds_end = draw_cmds_start + draw_cmd_count;

    // Find clear colour
    uint8_t clear_colour[4] = {0, 0, 0, 0};
    memcpy(clear_colour, uir->clear_colour, 4);

    for (; draw_cmds != draw_cmds_end; draw_cmds++) {
        uint8_t fill_colour[4];
        if (UIR_draw_cmd_is_fill(fill_colour, &tile_rect, draw_cmds)) {
            UIR_blend(clear_colour, fill_colour, 1.f);
        } else if (UIR_rect_intersect(&tile_rect, &draw_cmds->common.rect)) {
            break;
        }
    }

    // Clear tile
    UIR_fill_tile(uir->tiles[tile_idx], clear_colour);
    
    // Draw!
    for (; draw_cmds != draw_cmds_end; draw_cmds++) {
        if (UIR_rect_intersect(&tile_rect, &draw_cmds->common.rect))
            UIR_tile_draw_cmd(uir->tiles[tile_idx], &tile_rect, draw_cmds);
    }
}

uint32_t UIR_draw(
    UIR *uir,
    UIR_DrawCmd *draw_cmds,
    uint32_t draw_cmd_count
) {
    // ------------------------------
    // easy optimization prepass

    for (uint32_t i = 0; i < draw_cmd_count; ++i) {
        UIR_DrawCmd *cmd = &draw_cmds[i];
        switch (cmd->common.type) {

            // tighten circle bounding box
            case UIR_DRAW_SHAPE_CIRCLE: {
                UIR_DrawCmd_Shape *shape = &cmd->shape;

                float w2 = (shape->rect.x1 - shape->rect.x0) * 0.5f;
                float h2 = (shape->rect.y1 - shape->rect.y0) * 0.5f;
                float radius = UIR_min(w2, h2);

                float x = shape->rect.x0 + w2;
                float y = shape->rect.y0 + h2;
                
                shape->rect.x0 = x - radius;
                shape->rect.y0 = y - radius;
                shape->rect.x1 = x + radius;
                shape->rect.y1 = y + radius;
            } break;

            default:
                break;
        }
    }
    
    // ------------------------------
    // reset hashes
    
    uint32_t init_hash = UIR_hash(uir->clear_colour, sizeof(uir->clear_colour));
    for (uint32_t y = 0; y < uir->height_in_tiles; ++y)
        for (uint32_t x = 0; x < uir->width_in_tiles; ++x)
            uir->tile_info[y*uir->width_in_tiles + x].hash_new = init_hash ^ x ^ (y << 16);

    // ------------------------------
    // hash draw_cmds for tiles

    for (uint32_t i = 0; i < draw_cmd_count; ++i) {
        UIR_DrawCmd *cmd = &draw_cmds[i];

        uint32_t draw_cmd_hash = UIR_hash_draw_cmd(cmd);

        UIR_Rect *bb = &cmd->common.rect;
        uint32_t x0 = (uint32_t)(bb->x0 / UIR_TILE_SIZE);
        uint32_t y0 = (uint32_t)(bb->y0 / UIR_TILE_SIZE);
        uint32_t x1 = (uint32_t)((bb->x1 + (UIR_TILE_SIZE - 1)) / UIR_TILE_SIZE);
        uint32_t y1 = (uint32_t)((bb->y1 + (UIR_TILE_SIZE - 1)) / UIR_TILE_SIZE);
        
        for (uint32_t y = y0; y < y1; ++y) {
            for (uint32_t x = x0; x < x1; ++x) {
                uint32_t tile_idx = y * uir->width_in_tiles + x;
                uir->tile_info[tile_idx].hash_new ^= draw_cmd_hash;
            }
        }
    }

    // ------------------------------
    // draw tiles that have changed

    uint32_t redrawn = 0;

    uint32_t width_in_tiles = uir->width_in_tiles;
    uint32_t height_in_tiles = uir->height_in_tiles;
    for (uint32_t y = 0; y < height_in_tiles; ++y) {
        for (uint32_t x = 0; x < width_in_tiles; ++x) {
            uint32_t tile_idx = y * width_in_tiles + x;
            UIR_TileInfo *tile_info = &uir->tile_info[tile_idx];

            if (tile_info->hash_old != tile_info->hash_new) {
                redrawn++;
                tile_info->hash_old = tile_info->hash_new;
                UIR_tile_draw(uir, draw_cmds, draw_cmd_count, x, y);
            }
        }
    }
    
    return redrawn;
}

void UIR_write_buffer_rgba(
    UIR *uir,
    unsigned char *rgba_buffer,
    size_t row_stride_in_bytes
) {
    for (uint32_t y = 0; y < uir->height_in_px; ++y) {
        for (uint32_t x = 0; x < uir->width_in_px; ++x) {
            uint32_t tile_x = x / UIR_TILE_SIZE;
            uint32_t tile_y = y / UIR_TILE_SIZE;
            uint32_t px_x = x % UIR_TILE_SIZE;
            uint32_t px_y = y % UIR_TILE_SIZE;
            uint32_t tile_idx = tile_y * uir->width_in_tiles + tile_x;
            uint32_t px_idx = px_y * UIR_TILE_SIZE + px_x;

            memcpy(
                &rgba_buffer[y * (uint32_t)row_stride_in_bytes + x*4],
                uir->tiles[tile_idx][px_idx],
                4
            );
        }
    }
}
