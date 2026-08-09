#include "vf2/platform.h"

#include <stdint.h>
#include <stdio.h>

static int failures;

#define EXPECT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            ++failures; \
        } \
    } while (0)

int main(void)
{
    vf2_platform platform;
    uint32_t pixels[6] = {0u};

    EXPECT_TRUE(vf2_platform_initialize(&platform, 3u, 2u) == VF2_OK);
    EXPECT_TRUE(vf2_platform_set_input(
        &platform, VF2_PLATFORM_BUTTON_PUNCH | VF2_PLATFORM_BUTTON_START
    ) == VF2_OK);
    EXPECT_TRUE(vf2_platform_get_input(&platform) ==
                (VF2_PLATFORM_BUTTON_PUNCH | VF2_PLATFORM_BUTTON_START));
    EXPECT_TRUE(vf2_platform_begin_frame(&platform, 0x11223344u) == VF2_OK);
    EXPECT_TRUE(vf2_platform_put_pixel(&platform, 2u, 1u, 0xaabbccddu) == VF2_OK);
    EXPECT_TRUE(vf2_platform_fill_rect(&platform, -1, 0, 2, 1, 0x55667788u) == VF2_OK);
    EXPECT_TRUE(vf2_platform_fill_triangle(
        &platform, 0, 0, 2, 0, 0, 2, 0xff0000ffu
    ) == VF2_OK);
    EXPECT_TRUE(vf2_platform_end_frame(&platform) == VF2_OK);
    EXPECT_TRUE(platform.frame_index == 1u);
    EXPECT_TRUE(platform.depth != NULL);
    EXPECT_TRUE(vf2_platform_read_pixels(&platform, pixels, 6u) == VF2_OK);
    EXPECT_TRUE(pixels[0] == 0xff0000ffu);
    EXPECT_TRUE(pixels[5] == 0xaabbccddu);
    EXPECT_TRUE(vf2_platform_put_pixel(&platform, 0u, 0u, 0u) ==
                VF2_ERROR_INVALID_ARGUMENT);
    vf2_platform_shutdown(&platform);
    EXPECT_TRUE(platform.pixels == NULL);
    EXPECT_TRUE(platform.depth == NULL);
    return failures == 0 ? 0 : 1;
}
