#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vf2/scsp.h"
#include "vf2/sound_board.h"
#include "vf2/status.h"
#include "../../src/hardware/sound_l2_ad.h"

/* ROM-FREE original-shaped 12-packet B/CF AD 21 00 regression.
 * The original MC68000 owner-ROM comparison is a separate stronger
 * --probe-ad21-l2 fixture in test_frame342_dispatch.c. No actual
 * proprietary ROM bytes, audio samples, or M68000 checkpoint here. */
static vf2_sound_board board;
static uint8_t rom[0x0c000u];
static uint8_t before[VF2_SOUND_RAM_BYTES];
static vf2_scsp scsp_before;
static int failed;

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "ad21-l2 test line %u: %s\n", (unsigned)__LINE__, #x); \
    ++failed; } } while (0)

static void setup(void)
{
    size_t i;
    uint8_t *record, *desc;
    memset(&board, 0, sizeof(board));
    memset(rom, 0, sizeof(rom));
    board.audio_rom = rom;
    board.audio_rom_size = sizeof(rom);
    vf2_scsp_initialize(&board.scsp);
    record = rom + 0xa6b2u;
    for (i = 0u; i < 12u; ++i) {
        record[i * 4u] = (uint8_t)(0xb9u + (i % 6u));
        record[i * 4u + 1u] = i == 0u ? 0xcfu : 0u;
        record[i * 4u + 2u] = i < 6u ? 0u : 0x7fu;
        record[i * 4u + 3u] = i < 6u ? 0x2au : 7u;
    }
    record[48u] = 0xffu;
    record[49u] = 0u;
    record[50u] = 0u;
    record[51u] = 0x2fu;
    desc = board.sound_ram + VF2_SOUND_DESCRIPTOR_BASE +
           VF2_SOUND_STREAM_DESCRIPTOR_BYTES;
    desc[0] = 0x80u;
    desc[3] = 1u;
    desc[4] = 0u;
    desc[5] = 0x60u;
    desc[6] = 0xa6u;
    desc[7] = 0xb3u;
    memset(board.sound_ram + VF2_SOUND_COMMAND_RING_BASE, 0xcc, 0x70u);
    board.sound_ram[VF2_SOUND_COMMAND_WRITE_CURSOR] = 0u;
    board.sound_ram[VF2_SOUND_COMMAND_WRITE_CURSOR + 1u] = 0x14u;
    board.sound_ram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    board.sound_ram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0x14u;
    board.sound_ram[VF2_SOUND_COMMAND_COUNT] = 0u;
}

static void positive(void)
{
    vf2_sound_stream_maintenance_report report;
    uint8_t *desc, *ring;
    size_t i;
    uint8_t actual[36u];
    setup();
    memset(actual, 0u, sizeof(actual));
    CHECK(vf2_sound_l2_ad21_expand(rom, sizeof(rom),
          UINT32_C(0x60a6b3), actual));
    for (i = 0u; i < 12u; ++i) {
        CHECK(actual[i * 3u] == (uint8_t)(0xb9u + i % 6u));
        CHECK(actual[i * 3u + 1u] == (uint8_t)(i < 6u ? 0x2au : 7u));
        CHECK(actual[i * 3u + 2u] == (uint8_t)(i < 6u ? 0u : 0x7fu));
    }
    memset(&report, 0, sizeof(report));
    CHECK(vf2_sound_board_maintain_streams(&board, &report) == VF2_OK);
    CHECK(report.descriptors_scanned == 8u);
    CHECK(report.refused_stage == 0u);
    CHECK(report.packets_emitted == 12u);
    CHECK(report.write_cursor_before == 0x14u);
    CHECK(report.write_cursor_after == 0x44u);
    CHECK(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 12u);
    CHECK(board.sound_ram[VF2_SOUND_COMMAND_WRITE_CURSOR] == 0u);
    CHECK(board.sound_ram[VF2_SOUND_COMMAND_WRITE_CURSOR + 1u] == 0x44u);
    ring = board.sound_ram + VF2_SOUND_COMMAND_RING_BASE;
    for (i = 0u; i < 12u; ++i) {
        size_t at = 0x14u + 4u * i;
        CHECK(ring[at] == actual[i * 3u]);
        CHECK(ring[at + 1u] == actual[i * 3u + 1u]);
        CHECK(ring[at + 2u] == actual[i * 3u + 2u]);
        CHECK(ring[at + 3u] == 0xccu);
    }
    desc = board.sound_ram + VF2_SOUND_DESCRIPTOR_BASE +
           VF2_SOUND_STREAM_DESCRIPTOR_BYTES;
    CHECK(desc[0] == 0u);
    CHECK(desc[1] == 0xffu);
    CHECK(desc[2] == 0u);
    CHECK(desc[3] == 0u);
    CHECK(desc[4] == 0u && desc[5] == 0x60u &&
          desc[6] == 0xa6u && desc[7] == 0xb3u);
    CHECK(board.scsp.registers[0x418u] == 0u);
    CHECK(board.scsp.registers[0x419u] == 0xceu);
    CHECK(board.scsp.registers[0x422u] == 0u);
    CHECK(board.scsp.registers[0x423u] == 0x40u);
}

static void refuse(unsigned kind)
{
    vf2_sound_stream_maintenance_report report;
    uint8_t *record, *desc;
    vf2_status status;
    setup();
    record = rom + 0xa6b2u;
    desc = board.sound_ram + VF2_SOUND_DESCRIPTOR_BASE +
           VF2_SOUND_STREAM_DESCRIPTOR_BYTES;
    switch (kind) {
    case 0u: record[1u] ^= 1u; break; /* Unknown B/CF header */
    case 1u: record[51u] ^= 1u; break; /* Bad FF 2F terminator */
    case 2u: ++desc[7u]; break; /* Unapproved adjacent record */
    case 3u: board.sound_ram[VF2_SOUND_COMMAND_COUNT] = 244u; break;
    case 4u: ++board.sound_ram[VF2_SOUND_COMMAND_WRITE_CURSOR + 1u]; break;
    case 5u: {
        uint8_t *other = desc + VF2_SOUND_STREAM_DESCRIPTOR_BYTES;
        memcpy(other, desc, VF2_SOUND_STREAM_DESCRIPTOR_BYTES);
        break;
    }
    case 6u: record[4u] ^= 1u; break; /* Incorrect B9..BE body */
    default: CHECK(0); return;
    }
    memcpy(before, board.sound_ram, sizeof(before));
    memcpy(&scsp_before, &board.scsp, sizeof(scsp_before));
    memset(&report, 0, sizeof(report));
    status = vf2_sound_board_maintain_streams(&board, &report);
    CHECK(status == VF2_ERROR_UNSUPPORTED);
    CHECK(report.refused_stage == 2u);
    CHECK(memcmp(before, board.sound_ram, sizeof(before)) == 0);
    CHECK(memcmp(&scsp_before, &board.scsp, sizeof(scsp_before)) == 0);
}

int main(void)
{
    unsigned i;
    positive();
    for (i = 0u; i < 7u; ++i) refuse(i);
    if (failed == 0) {
        puts("AD21 B/CF 12 original-shaped commands and seven rollback refusals PASS");
    }
    return failed != 0;
}
