#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vf2/game.h"

static int failures = 0;

#define EXPECT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "expectation failed: %s:%d: %s\n", \
                    __FILE__, __LINE__, #condition); \
            ++failures; \
        } \
    } while (0)

static uint32_t float_bits(float value)
{
    uint32_t bits = 0u;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

int main(void)
{
    static uint8_t tables[VF2_TGP_TABLE_WORD_COUNT * sizeof(uint32_t)];
    vf2_game game = {0};
    vf2_tgp_geometry_stream_report report;
    static uint8_t audio_rom[4u];
    static uint8_t sample_rom[4u];
    int16_t audio_left[8u];
    int16_t audio_right[8u];
    uint32_t pixels[16u * 16u];
    uint32_t stream[] = {
        UINT32_C(0x01000000), 0u, 0u,
        0u, 0u, 0u, 0u, 0u, 0u,
        UINT32_C(2), 0u, 0u,
        0u, 0u, 0u, 0u,
        UINT32_C(0x07800000)
    };

    stream[3] = float_bits(-1.0f);
    stream[4] = float_bits(-1.0f);
    stream[6] = float_bits(1.0f);
    stream[7] = float_bits(-1.0f);
    stream[12] = float_bits(-1.0f);
    stream[13] = float_bits(1.0f);
    EXPECT_TRUE(vf2_game_initialize(&game) == VF2_OK);
    EXPECT_TRUE(vf2_game_set_input(&game, 1u) != VF2_OK);
    EXPECT_TRUE(vf2_game_attach_graphics(
        &game, 16u, 16u, tables, sizeof(tables), NULL, 0u, NULL, 0u
    ) == VF2_OK);
    EXPECT_TRUE(vf2_game_set_input(&game, 0x24u) == VF2_OK);
    EXPECT_TRUE(vf2_game_render_audio(
        &game, audio_left, audio_right, 1u
    ) != VF2_OK);
    EXPECT_TRUE(vf2_game_attach_audio(
        &game, audio_rom, sizeof(audio_rom), sample_rom, sizeof(sample_rom)
    ) == VF2_OK);
    EXPECT_TRUE(vf2_game_attach_audio(
        &game, audio_rom, sizeof(audio_rom), sample_rom, sizeof(sample_rom)
    ) != VF2_OK);
    EXPECT_TRUE(vf2_game_render_audio(
        &game, audio_left, audio_right,
        sizeof(audio_left) / sizeof(audio_left[0])
    ) == VF2_OK);
    EXPECT_TRUE(vf2_platform_get_input(game.platform) == 0x24u);
    EXPECT_TRUE(vf2_game_begin_frame(&game, 0u) == VF2_OK);
    EXPECT_TRUE(vf2_game_submit_geometry(
        &game, stream, sizeof(stream) / sizeof(stream[0]),
        UINT32_C(0x12345678), &report
    ) == VF2_OK);
    EXPECT_TRUE(report.rendered_triangles == 1u);
    EXPECT_TRUE(vf2_game_end_frame(&game) == VF2_OK);
    EXPECT_TRUE(game.frame_number == 1u);
    memset(pixels, 0, sizeof(pixels));
    EXPECT_TRUE(vf2_game_read_pixels(
        &game, pixels, sizeof(pixels) / sizeof(pixels[0])
    ) == VF2_OK);
    EXPECT_TRUE(pixels[0] == UINT32_C(0x12345678));
    EXPECT_TRUE(vf2_game_update(&game) == VF2_OK);
    EXPECT_TRUE(game.frame_number == 2u);
    vf2_game_shutdown(&game);
    EXPECT_TRUE(game.platform == NULL && game.tgp == NULL && game.sound == NULL);
    if (failures != 0) {
        fprintf(stderr, "%d game test(s) failed\n", failures);
        return 1;
    }
    puts("Game tests passed");
    return 0;
}
