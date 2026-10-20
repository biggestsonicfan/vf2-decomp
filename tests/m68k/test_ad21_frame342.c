#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vf2/scsp.h"
#include "vf2/sound_board.h"
#include "vf2/status.h"

/* ROM-free original-shaped fixture for the PRECISE AD 21 00
 * high-bit-header allocation path measured from frame 342.
 * Full original-owner-ROM C/68000 RAM parity is separately pinned
 * by vf2_m68k_frame342_dispatch_tests; no ROM is distributed here. */
static vf2_sound_board board;
static uint8_t fake_audio_rom[0x80000u];
static int failures;

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "ad21 guard line %u: %s\n", (unsigned)__LINE__, #x); \
    ++failures; } } while (0)

static void be32(size_t offset, uint32_t value)
{
    fake_audio_rom[offset] = (uint8_t)(value >> 24u);
    fake_audio_rom[offset + 1u] = (uint8_t)(value >> 16u);
    fake_audio_rom[offset + 2u] = (uint8_t)(value >> 8u);
    fake_audio_rom[offset + 3u] = (uint8_t)value;
}

static void setup(uint8_t third, uint8_t record_header)
{
    uint8_t *ring;
    memset(&board, 0, sizeof(board));
    memset(fake_audio_rom, 0, sizeof(fake_audio_rom));
    board.audio_rom = fake_audio_rom;
    board.audio_rom_size = sizeof(fake_audio_rom);
    vf2_scsp_initialize(&board.scsp);

    /* Actual original directory AD21 -> 0x6091A8;
     * row.max=0x007B, row[0] -> 0x60A6B2. */
    be32(0x8e12u + 0x21u * 4u, UINT32_C(0x6091a8));
    fake_audio_rom[0x91a8u] = 0x00u;
    fake_audio_rom[0x91a9u] = 0x7bu;
    be32(0x91aau, UINT32_C(0x60a6b2));
    fake_audio_rom[0xa6b2u] = record_header;

    ring = board.sound_ram + VF2_SOUND_COMMAND_RING_BASE;
    ring[0] = 0xadu;
    ring[1] = 0x21u;
    ring[2] = third;
    board.sound_ram[VF2_SOUND_COMMAND_COUNT] = 1u;
    /* The original 0x7DC dispatcher scans *active command
     * descriptors* before switching by class. An all-zero test
     * board consumes every command as a no-op; frame 342 starts
     * with a live descriptor matching channel 0xD. Synthesize
     * precisely that prerequisite without staging any sample or
     * SCSP state. A0's own first-vacant stream descriptor bank
     * remains empty, as in the independently proven owner-ROM case. */
    {
        uint8_t *match = board.sound_ram +
            VF2_SOUND_COMMAND_DESCRIPTOR_BASE +
            13u * VF2_SOUND_COMMAND_DESCRIPTOR_BYTES;
        match[0] = 0x80u;
        match[1] = 0x0du;
    }
}

static void check_valid(void)
{
    vf2_sound_dispatch_report report = {0};
    vf2_status status;
    setup(0u, 0xcfu);
    status = vf2_sound_board_dispatch_next(&board, &report);
    CHECK(status == VF2_OK);
    CHECK(report.handled == 1);
    CHECK(report.handler_address == UINT32_C(0x1f02));
    CHECK(report.descriptors_updated == 1u);
    CHECK(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
    CHECK(board.sound_ram[VF2_SOUND_COMMAND_CURSOR] == 0u);
    CHECK(board.sound_ram[VF2_SOUND_COMMAND_CURSOR + 1u] == 4u);
}

static void check_neighbor_held(void)
{
    vf2_sound_dispatch_report report = {0};
    vf2_status status;
    /* AD21 01 may have another valid record in the real ROM.
     * It has not been independently recovered: refuse at join. */
    setup(1u, 0xcfu);
    status = vf2_sound_board_dispatch_next(&board, &report);
    CHECK(status == VF2_ERROR_UNSUPPORTED);
    CHECK(report.handler_address == UINT32_C(0x1e98));
    CHECK(report.handled == 0);
    CHECK(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 1u);
}

static void check_invalid_header_held(void)
{
    vf2_sound_dispatch_report report = {0};
    vf2_status status;
    setup(0u, 0x4fu);
    status = vf2_sound_board_dispatch_next(&board, &report);
    CHECK(status == VF2_ERROR_UNSUPPORTED);
    CHECK(report.handler_address == UINT32_C(0x1e98));
    CHECK(report.handled == 0);
    CHECK(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 1u);
}

int main(void)
{
    check_valid();
    check_neighbor_held();
    check_invalid_header_held();
    if (failures == 0) {
        puts("AD 21 00: ROM-shaped descriptor, exact-only routing and fail-closed guards PASS");
    }
    return failures != 0;
}
