#ifndef VF2_PLATFORM_H
#define VF2_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/status.h"

enum {
    VF2_PLATFORM_BUTTON_UP = UINT32_C(1) << 0,
    VF2_PLATFORM_BUTTON_DOWN = UINT32_C(1) << 1,
    VF2_PLATFORM_BUTTON_LEFT = UINT32_C(1) << 2,
    VF2_PLATFORM_BUTTON_RIGHT = UINT32_C(1) << 3,
    VF2_PLATFORM_BUTTON_PUNCH = UINT32_C(1) << 4,
    VF2_PLATFORM_BUTTON_KICK = UINT32_C(1) << 5,
    VF2_PLATFORM_BUTTON_GUARD = UINT32_C(1) << 6,
    VF2_PLATFORM_BUTTON_START = UINT32_C(1) << 7,
    VF2_PLATFORM_BUTTON_COIN = UINT32_C(1) << 8,
    VF2_PLATFORM_BUTTON_SERVICE = UINT32_C(1) << 9,
    VF2_PLATFORM_BUTTON_P2_UP = UINT32_C(1) << 10,
    VF2_PLATFORM_BUTTON_P2_DOWN = UINT32_C(1) << 11,
    VF2_PLATFORM_BUTTON_P2_LEFT = UINT32_C(1) << 12,
    VF2_PLATFORM_BUTTON_P2_RIGHT = UINT32_C(1) << 13,
    VF2_PLATFORM_BUTTON_P2_PUNCH = UINT32_C(1) << 14,
    VF2_PLATFORM_BUTTON_P2_KICK = UINT32_C(1) << 15,
    VF2_PLATFORM_BUTTON_P2_GUARD = UINT32_C(1) << 16,
    VF2_PLATFORM_BUTTON_TEST = UINT32_C(1) << 17
};

typedef struct vf2_platform {
    uint32_t width;
    uint32_t height;
    uint32_t *pixels;
    size_t pixel_count;
    float *depth;
    size_t depth_count;
    uint32_t input;
    uint64_t frame_index;
    int frame_open;
} vf2_platform;

vf2_status vf2_platform_initialize(
    vf2_platform *platform,
    uint32_t width,
    uint32_t height
);
void vf2_platform_shutdown(vf2_platform *platform);

vf2_status vf2_platform_set_input(vf2_platform *platform, uint32_t input);
uint32_t vf2_platform_get_input(const vf2_platform *platform);

vf2_status vf2_platform_begin_frame(vf2_platform *platform, uint32_t color);
vf2_status vf2_platform_put_pixel(
    vf2_platform *platform,
    uint32_t x,
    uint32_t y,
    uint32_t color
);
vf2_status vf2_platform_fill_rect(
    vf2_platform *platform,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    uint32_t color
);
vf2_status vf2_platform_fill_triangle(
    vf2_platform *platform,
    int32_t x0,
    int32_t y0,
    int32_t x1,
    int32_t y1,
    int32_t x2,
    int32_t y2,
    uint32_t color
);
vf2_status vf2_platform_fill_triangle_depth(
    vf2_platform *platform,
    int32_t x0,
    int32_t y0,
    float depth0,
    int32_t x1,
    int32_t y1,
    float depth1,
    int32_t x2,
    int32_t y2,
    float depth2,
    uint32_t color
);
vf2_status vf2_platform_end_frame(vf2_platform *platform);

vf2_status vf2_platform_read_pixels(
    const vf2_platform *platform,
    uint32_t *destination,
    size_t destination_pixels
);

#endif
