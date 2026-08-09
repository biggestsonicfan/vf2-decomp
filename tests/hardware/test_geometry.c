#include "vf2/geometry.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define EXPECT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            ++failures; \
        } \
    } while (0)

static void put_le32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
    data[2] = (uint8_t)(value >> 16u);
    data[3] = (uint8_t)(value >> 24u);
}

int main(void)
{
    vf2_model2a machine;
    vf2_geometry_commit_report report;
    uint8_t main_rom[0x8000];
    uint32_t value = 0u;
    uint32_t command = 0u;

    memset(main_rom, 0, sizeof(main_rom));
    EXPECT_TRUE(vf2_model2a_initialize(&machine));
    EXPECT_TRUE(vf2_model2a_attach_main_rom(&machine, main_rom, sizeof(main_rom)) == VF2_OK);

    EXPECT_TRUE(vf2_geometry_pack_command((int16_t)-2, 0x12u) ==
                UINT32_C(0x897ffe00));
    EXPECT_TRUE(vf2_geometry_setup_command(
        &machine, VF2_GEOMETRY_BASE, 0x1008u, (int16_t)-2, 0x12u,
        &command) == VF2_OK);
    EXPECT_TRUE(command == UINT32_C(0x897ffe00));
    EXPECT_TRUE(vf2_model2a_read_u32(&machine, VF2_GEOMETRY_BASE + 0x1008u,
                                     &value) == VF2_OK);
    EXPECT_TRUE(value == command);

    put_le32(main_rom + 0x7a00u + 4u, UINT32_C(0x11223344));
    EXPECT_TRUE(vf2_model2a_write_u32(
        &machine, VF2_GEOMETRY_COMMAND_STATE, UINT32_C(0x00001000)) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write_u32(
        &machine, VF2_GEOMETRY_BASE + VF2_GEOMETRY_READ_OFFSET,
        UINT32_C(0x00001020)) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write_u32(
        &machine, VF2_GEOMETRY_MAX_DISTANCE, UINT32_C(0x10)) == VF2_OK);
    {
        uint8_t index = 0u;
        EXPECT_TRUE(vf2_model2a_write(
            &machine, VF2_GEOMETRY_RING_INDEX, &index, sizeof(index)) == VF2_OK);
    }
    EXPECT_TRUE(vf2_geometry_commit_frame(
        &machine, VF2_GEOMETRY_BASE, &report) == VF2_OK);
    EXPECT_TRUE(report.distance == 0x20);
    EXPECT_TRUE(report.maximum_distance == 0x20u);
    EXPECT_TRUE(report.maximum_updated != 0);
    EXPECT_TRUE(report.ring_index == 1u);
    EXPECT_TRUE(report.next_command == UINT32_C(0x11223344));
    EXPECT_TRUE(vf2_model2a_read_u32(
        &machine, VF2_GEOMETRY_COMMAND_STATE, &value) == VF2_OK);
    EXPECT_TRUE(value == report.next_command);
    EXPECT_TRUE(vf2_model2a_read_u32(
        &machine, VF2_GEOMETRY_BASE + VF2_GEOMETRY_PREVIOUS_OFFSET,
        &value) == VF2_OK);
    EXPECT_TRUE(value == UINT32_C(0x00001000));

    EXPECT_TRUE(vf2_model2a_write_u32(
        &machine, VF2_GEOMETRY_BASE + VF2_GEOMETRY_WRITE_OFFSET,
        VF2_BUFFER_RAM_BASE) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write_u32(
        &machine, VF2_VIDEO_CONTROL_BASE + UINT32_C(8), 0u) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write_u32(
        &machine, VF2_GEOMETRY_BASE + UINT32_C(0x4000),
        UINT32_C(0x01000000)) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_read_u32(
        &machine, VF2_BUFFER_RAM_BASE, &value) == VF2_OK);
    EXPECT_TRUE(value == UINT32_C(0x01000000));
    EXPECT_TRUE(vf2_model2a_write_u32(
        &machine, VF2_VIDEO_CONTROL_BASE + UINT32_C(8),
        UINT32_C(0x80000000)) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write_u32(
        &machine, VF2_GEOMETRY_BASE + UINT32_C(0x4000),
        UINT32_C(0x02000000)) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_read_u32(
        &machine, VF2_BUFFER_RAM_BASE + sizeof(uint32_t), &value) == VF2_OK);
    EXPECT_TRUE(value == 0u);

    EXPECT_TRUE(vf2_model2a_write_u32(
        &machine, VF2_VIDEO_CONTROL_BASE + UINT32_C(8), 0u) == VF2_OK);
    vf2_model2a_shutdown(&machine);
    return failures == 0 ? 0 : 1;
}
