#include "vf2/platform.h"

#include <stdlib.h>
#include <string.h>

vf2_status vf2_platform_initialize(
    vf2_platform *platform,
    uint32_t width,
    uint32_t height
)
{
    size_t pixel_count = 0u;

    if (platform == NULL || width == 0u || height == 0u ||
        (size_t)width > SIZE_MAX / (size_t)height) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    pixel_count = (size_t)width * (size_t)height;
    if (pixel_count > SIZE_MAX / sizeof(uint32_t)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(platform, 0, sizeof(*platform));
    platform->pixels = (uint32_t *)calloc(pixel_count, sizeof(uint32_t));
    if (platform->pixels == NULL) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    platform->depth = (float *)calloc(pixel_count, sizeof(float));
    if (platform->depth == NULL) {
        free(platform->pixels);
        platform->pixels = NULL;
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    platform->width = width;
    platform->height = height;
    platform->pixel_count = pixel_count;
    platform->depth_count = pixel_count;
    return VF2_OK;
}

void vf2_platform_shutdown(vf2_platform *platform)
{
    if (platform == NULL) {
        return;
    }
    free(platform->pixels);
    free(platform->depth);
    memset(platform, 0, sizeof(*platform));
}

vf2_status vf2_platform_set_input(vf2_platform *platform, uint32_t input)
{
    if (platform == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    platform->input = input;
    return VF2_OK;
}

uint32_t vf2_platform_get_input(const vf2_platform *platform)
{
    return platform == NULL ? 0u : platform->input;
}

vf2_status vf2_platform_begin_frame(vf2_platform *platform, uint32_t color)
{
    if (platform == NULL || platform->pixels == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (platform->frame_open) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    for (size_t index = 0u; index < platform->pixel_count; ++index) {
        platform->pixels[index] = color;
        platform->depth[index] = 1.0f;
    }
    platform->frame_open = 1;
    return VF2_OK;
}

vf2_status vf2_platform_put_pixel(
    vf2_platform *platform,
    uint32_t x,
    uint32_t y,
    uint32_t color
)
{
    if (platform == NULL || platform->pixels == NULL ||
        !platform->frame_open || x >= platform->width || y >= platform->height) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    platform->pixels[(size_t)y * platform->width + x] = color;
    platform->depth[(size_t)y * platform->width + x] = 0.0f;
    return VF2_OK;
}

vf2_status vf2_platform_fill_rect(
    vf2_platform *platform,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    uint32_t color
)
{
    int32_t start_x;
    int32_t start_y;
    int32_t end_x;
    int32_t end_y;

    if (platform == NULL || platform->pixels == NULL ||
        !platform->frame_open || width < 0 || height < 0) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    start_x = x < 0 ? 0 : x;
    start_y = y < 0 ? 0 : y;
    end_x = x > (int32_t)platform->width ? (int32_t)platform->width : x;
    end_y = y > (int32_t)platform->height ? (int32_t)platform->height : y;
    if (width < INT32_MAX - x) {
        const int32_t requested_end = x + width;
        if (requested_end < end_x) {
            end_x = requested_end;
        }
    }
    if (height < INT32_MAX - y) {
        const int32_t requested_end = y + height;
        if (requested_end < end_y) {
            end_y = requested_end;
        }
    }
    for (int32_t row = start_y; row < end_y; ++row) {
        for (int32_t column = start_x; column < end_x; ++column) {
            platform->pixels[(size_t)row * platform->width +
                             (size_t)column] = color;
        }
    }
    return VF2_OK;
}

static int64_t edge_function(
    int32_t ax,
    int32_t ay,
    int32_t bx,
    int32_t by,
    int32_t px,
    int32_t py
)
{
    return ((int64_t)px - ax) * ((int64_t)by - ay) -
           ((int64_t)py - ay) * ((int64_t)bx - ax);
}

static int triangle_pixel_inside(
    int64_t area,
    int64_t *weight0,
    int64_t *weight1,
    int64_t *weight2
)
{
    if (area < 0) {
        *weight0 = -*weight0;
        *weight1 = -*weight1;
        *weight2 = -*weight2;
        area = -area;
    }
    (void)area;
    return *weight0 >= 0 && *weight1 >= 0 && *weight2 >= 0;
}

vf2_status vf2_platform_fill_triangle(
    vf2_platform *platform,
    int32_t x0,
    int32_t y0,
    int32_t x1,
    int32_t y1,
    int32_t x2,
    int32_t y2,
    uint32_t color
)
{
    int32_t min_x;
    int32_t min_y;
    int32_t max_x;
    int32_t max_y;
    int64_t area;

    if (platform == NULL || platform->pixels == NULL ||
        !platform->frame_open) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    min_x = x0 < x1 ? x0 : x1;
    min_x = min_x < x2 ? min_x : x2;
    min_y = y0 < y1 ? y0 : y1;
    min_y = min_y < y2 ? min_y : y2;
    max_x = x0 > x1 ? x0 : x1;
    max_x = max_x > x2 ? max_x : x2;
    max_y = y0 > y1 ? y0 : y1;
    max_y = max_y > y2 ? max_y : y2;
    if (max_x < 0 || max_y < 0 || min_x >= (int32_t)platform->width ||
        min_y >= (int32_t)platform->height) {
        return VF2_OK;
    }
    if (min_x < 0) {
        min_x = 0;
    }
    if (min_y < 0) {
        min_y = 0;
    }
    if (max_x >= (int32_t)platform->width) {
        max_x = (int32_t)platform->width - 1;
    }
    if (max_y >= (int32_t)platform->height) {
        max_y = (int32_t)platform->height - 1;
    }
    area = edge_function(x0, y0, x1, y1, x2, y2);
    if (area == 0) {
        return VF2_OK;
    }
    for (int32_t row = min_y; row <= max_y; ++row) {
        for (int32_t column = min_x; column <= max_x; ++column) {
            int64_t w0 = edge_function(x1, y1, x2, y2, column, row);
            int64_t w1 = edge_function(x2, y2, x0, y0, column, row);
            int64_t w2 = edge_function(x0, y0, x1, y1, column, row);
            if (area < 0) {
                w0 = -w0;
                w1 = -w1;
                w2 = -w2;
            }
            if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                platform->pixels[(size_t)row * platform->width +
                                 (size_t)column] = color;
            }
        }
    }
    return VF2_OK;
}

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
)
{
    int32_t min_x;
    int32_t min_y;
    int32_t max_x;
    int32_t max_y;
    int64_t area;
    int64_t area_absolute;

    if (platform == NULL || platform->pixels == NULL ||
        platform->depth == NULL || !platform->frame_open ||
        depth0 != depth0 || depth1 != depth1 || depth2 != depth2) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    min_x = x0 < x1 ? x0 : x1;
    min_x = min_x < x2 ? min_x : x2;
    min_y = y0 < y1 ? y0 : y1;
    min_y = min_y < y2 ? min_y : y2;
    max_x = x0 > x1 ? x0 : x1;
    max_x = max_x > x2 ? max_x : x2;
    max_y = y0 > y1 ? y0 : y1;
    max_y = max_y > y2 ? max_y : y2;
    if (max_x < 0 || max_y < 0 || min_x >= (int32_t)platform->width ||
        min_y >= (int32_t)platform->height) {
        return VF2_OK;
    }
    if (min_x < 0) {
        min_x = 0;
    }
    if (min_y < 0) {
        min_y = 0;
    }
    if (max_x >= (int32_t)platform->width) {
        max_x = (int32_t)platform->width - 1;
    }
    if (max_y >= (int32_t)platform->height) {
        max_y = (int32_t)platform->height - 1;
    }
    area = edge_function(x0, y0, x1, y1, x2, y2);
    if (area == 0) {
        return VF2_OK;
    }
    area_absolute = area < 0 ? -area : area;
    for (int32_t row = min_y; row <= max_y; ++row) {
        for (int32_t column = min_x; column <= max_x; ++column) {
            int64_t weight0 = edge_function(
                x1, y1, x2, y2, column, row
            );
            int64_t weight1 = edge_function(
                x2, y2, x0, y0, column, row
            );
            int64_t weight2 = edge_function(
                x0, y0, x1, y1, column, row
            );
            const size_t pixel = (size_t)row * platform->width +
                                 (size_t)column;
            float depth;

            if (!triangle_pixel_inside(
                    area, &weight0, &weight1, &weight2)) {
                continue;
            }
            depth = ((float)weight0 * depth0 +
                     (float)weight1 * depth1 +
                     (float)weight2 * depth2) / (float)area_absolute;
            if (depth < platform->depth[pixel]) {
                platform->depth[pixel] = depth;
                platform->pixels[pixel] = color;
            }
        }
    }
    return VF2_OK;
}

vf2_status vf2_platform_end_frame(vf2_platform *platform)
{
    if (platform == NULL || !platform->frame_open) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    platform->frame_open = 0;
    ++platform->frame_index;
    return VF2_OK;
}

vf2_status vf2_platform_read_pixels(
    const vf2_platform *platform,
    uint32_t *destination,
    size_t destination_pixels
)
{
    if (platform == NULL || destination == NULL ||
        destination_pixels < platform->pixel_count ||
        platform->pixels == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memcpy(destination, platform->pixels,
           platform->pixel_count * sizeof(uint32_t));
    return VF2_OK;
}
