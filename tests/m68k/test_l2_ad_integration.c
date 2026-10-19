#include "vf2/sound_board.h"
#include "vf2/status.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ROM-FREE functional test of the recovered A0->L2 two-packet path.
 * The 12-byte data is a tiny synthetic record fixture, not a dump.
 * The Motorola 68000 comparison with the original ROM remains a
 * separate, stronger ROM-dependent CTest. */
static vf2_sound_board board;
static uint8_t synthetic_rom[0x0c000u];
static int failures;

#define CHECK(cond) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %u: %s\n", (unsigned)__LINE__, #cond); \
    ++failures; \
} } while (0)

static void setup(uint32_t record, int ad24, uint8_t embedded)
{
    uint8_t *desc;
    uint8_t *ring;
    size_t off = (size_t)(record - UINT32_C(0x600000));
    memset(&board, 0, sizeof(board));
    memset(synthetic_rom, 0, sizeof(synthetic_rom));
    board.audio_rom = synthetic_rom;
    board.audio_rom_size = sizeof(synthetic_rom);
    vf2_scsp_initialize(&board.scsp);
    synthetic_rom[off] = 0x8fu;
    synthetic_rom[off + 1u] = ad24 ? 0xbau : 0xbbu;
    synthetic_rom[off + 2u] = 0x21u;
    synthetic_rom[off + 3u] = ad24 ? 0x07u : 0x06u;
    synthetic_rom[off + 4u] = 0u;
    synthetic_rom[off + 5u] = ad24 ? 0x9au : 0x9bu;
    synthetic_rom[off + 6u] = embedded;
    synthetic_rom[off + 7u] = ad24 || embedded >= 0x44u ? 0x7fu : 0x78u;
    synthetic_rom[off + 8u] = 0u;
    synthetic_rom[off + 9u] = 0xffu;
    synthetic_rom[off + 10u] = 0x2fu;
    synthetic_rom[off + 11u] = 0u;

    desc = board.sound_ram + VF2_SOUND_DESCRIPTOR_BASE +
           3u * VF2_SOUND_STREAM_DESCRIPTOR_BYTES;
    desc[0] = 0x80u;
    desc[2] = 0u;
    desc[3] = 1u;
    ++record; /* A0 MOVE.B (A2)+ consumed the 0x8f header. */
    desc[4] = (uint8_t)(record >> 24u);
    desc[5] = (uint8_t)(record >> 16u);
    desc[6] = (uint8_t)(record >> 8u);
    desc[7] = (uint8_t)record;

    /* One prior L3 packet had occupied the first sound ring slot. */
    ring = board.sound_ram + VF2_SOUND_COMMAND_RING_BASE;
    memset(ring, 0xcc, 16u);
    ring[0] = 0xadu; ring[1] = ad24 ? 0x24u : 0x23u;
    ring[2] = embedded;
    board.sound_ram[VF2_SOUND_COMMAND_WRITE_CURSOR] = 0u;
    board.sound_ram[VF2_SOUND_COMMAND_WRITE_CURSOR + 1u] = 4u;
    board.sound_ram[VF2_SOUND_COMMAND_COUNT] = 0u;
}

static void verify_positive(uint32_t record, int ad24, uint8_t embedded)
{
    vf2_sound_stream_maintenance_report report;
    vf2_status status;
    uint8_t *ring;
    uint8_t *desc;
    setup(record, ad24, embedded);
    memset(&report, 0, sizeof(report));
    status = vf2_sound_board_maintain_streams(&board, &report);
    ring = board.sound_ram + VF2_SOUND_COMMAND_RING_BASE;
    desc = board.sound_ram + VF2_SOUND_DESCRIPTOR_BASE +
           3u * VF2_SOUND_STREAM_DESCRIPTOR_BYTES;
    CHECK(status == VF2_OK);
    CHECK(report.descriptors_scanned == 8u);
    CHECK(report.timers_expired == 1u);
    CHECK(report.packets_emitted == 2u);
    CHECK(report.bytes_consumed == 10u);
    CHECK(report.refused_stage == 0u);
    CHECK(report.refused_record == 0xffu);
    CHECK(report.write_cursor_before == 4u);
    CHECK(report.write_cursor_after == 12u);
    CHECK(board.sound_ram[VF2_SOUND_COMMAND_WRITE_CURSOR] == 0u);
    CHECK(board.sound_ram[VF2_SOUND_COMMAND_WRITE_CURSOR + 1u] == 12u);
    CHECK(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 2u);
    CHECK(ring[0] == 0xadu);
    CHECK(ring[4] == (uint8_t)(ad24 ? 0xbau : 0xbbu));
    CHECK(ring[5] == 0x21u);
    CHECK(ring[6] == (uint8_t)(ad24 ? 7u : 6u));
    CHECK(ring[7] == 0xccu); /* Fourth byte remains untouched. */
    CHECK(ring[8] == (uint8_t)(ad24 ? 0x9au : 0x9bu));
    CHECK(ring[9] == embedded); /* AD24 0x7c alias stays 0x70. */
    CHECK(ring[10] == (uint8_t)(ad24 || embedded >= 0x44u ? 0x7fu : 0x78u));
    CHECK(ring[11] == 0xccu);
    CHECK(desc[0] == 0u); /* FF/2F terminator releases the descriptor. */
    CHECK(desc[1] == 0xffu);
    CHECK(desc[2] == 0u && desc[3] == 0u);
    CHECK(board.scsp.registers[0x418u] == 0u);
    CHECK(board.scsp.registers[0x419u] == 0xceu);
    CHECK(board.scsp.registers[0x422u] == 0u);
    CHECK(board.scsp.registers[0x423u] == 0x40u);
}

static void verify_refusal(unsigned variant)
{
    vf2_sound_stream_maintenance_report report;
    vf2_status status;
    uint8_t pre_ram[VF2_SOUND_RAM_BYTES];
    vf2_scsp pre_scsp;
    uint8_t *desc;
    setup(UINT32_C(0x60b4fe), 0, 0x42u);
    desc = board.sound_ram + VF2_SOUND_DESCRIPTOR_BASE +
           3u * VF2_SOUND_STREAM_DESCRIPTOR_BYTES;
    switch (variant) {
        case 0u:
            board.sound_ram[VF2_SOUND_COMMAND_COUNT] = 254u;
            break;
        case 1u:
            board.sound_ram[VF2_SOUND_COMMAND_WRITE_CURSOR + 1u] = 5u;
            break;
        case 2u:
            synthetic_rom[0xb4feu + 9u] = 0xfeu;
            break;
        case 3u:
            ++desc[7]; /* Unaligned pointer is not a sound record. */
            break;
        case 4u: {
            uint8_t *other = board.sound_ram + VF2_SOUND_DESCRIPTOR_BASE +
                             VF2_SOUND_STREAM_DESCRIPTOR_BYTES;
            memcpy(other, desc, VF2_SOUND_STREAM_DESCRIPTOR_BYTES);
            break;
        }
        default:
            CHECK(0);
            return;
    }
    memcpy(pre_ram, board.sound_ram, sizeof(pre_ram));
    memcpy(&pre_scsp, &board.scsp, sizeof(pre_scsp));
    memset(&report, 0, sizeof(report));
    status = vf2_sound_board_maintain_streams(&board, &report);
    CHECK(status == VF2_ERROR_UNSUPPORTED);
    CHECK(report.refused_stage == 2u);
    CHECK(memcmp(pre_ram, board.sound_ram, sizeof(pre_ram)) == 0);
    CHECK(memcmp(&pre_scsp, &board.scsp, sizeof(pre_scsp)) == 0);
}

int main(void)
{
    verify_positive(UINT32_C(0x60b4fe), 0, 0x42u);
    verify_positive(UINT32_C(0x60b7f2), 1, 0x01u);
    verify_positive(UINT32_C(0x60bde6) - 12u * 4u, 1, 0x70u);
    verify_positive(UINT32_C(0x60b1e6) + 12u * 0x44u, 0, 0x44u);
    verify_refusal(0u);
    verify_refusal(1u);
    verify_refusal(2u);
    verify_refusal(3u);
    verify_refusal(4u);
    if (failures == 0) {
        puts("A0->L2 B/90 packets: four positive, five rollback/refusal cases PASS");
    }
    return failures == 0 ? 0 : 1;
}
