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
    /* CPU-visible Motorola 68000 order, i.e. the audio region
     * after ROM word-byte assembly, NOT the raw EPR chip bytes. */
    record[0u] = 0xcfu;
    for (i = 0u; i < 12u; ++i) {
        record[1u + i * 4u] = (uint8_t)(0xb9u + (i % 6u));
        record[2u + i * 4u] = i < 6u ? 0x2au : 7u;
        record[3u + i * 4u] = i < 6u ? 0u : 0x7fu;
        record[4u + i * 4u] = 0u;
    }
    record[49u] = 0xffu;
    record[50u] = 0x2fu;
    record[51u] = 0u;
    record[52u] = 0u;
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
    /* The old 8F AD23/24 two-packet grammar MUST reject this
     * new B/CF record; it cannot silently claim parity. */
    {
        uint8_t other[6] = {0u};
        CHECK(!vf2_sound_l2_ad_expand(
            rom, sizeof(rom), UINT32_C(0x60a6b3), other));
    }
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


/* The terminator is exactly three bytes long.  The byte after it
 * belongs to an adjacent record and is deliberately unconstrained.
 * A truncated view must fail without touching the caller's packets. */
static void boundary(void)
{
    uint8_t packets[36u], expected[36u];
    size_t i, minimum;
    setup();
    minimum = 0xa6b3u + 51u;
    CHECK(minimum <= sizeof(rom));
    memset(expected, 0xa5, sizeof(expected));
    for (i = 0u; i < 51u; ++i) {
        memset(packets, 0xa5, sizeof(packets));
        CHECK(!vf2_sound_l2_ad21_expand(rom, 0xa6b3u + i,
              UINT32_C(0x60a6b3), packets));
        CHECK(memcmp(packets, expected, sizeof(packets)) == 0);
    }
    rom[minimum] = 0xcfu; /* Next record's header, not the terminator. */
    CHECK(vf2_sound_l2_ad21_expand(rom, minimum,
          UINT32_C(0x60a6b3), packets));
    CHECK(packets[0] == 0xb9u && packets[35] == 0x7fu);
    rom[minimum] = 0x5au; /* Next record must not affect decoding. */
    CHECK(vf2_sound_l2_ad21_expand(rom, minimum + 1u,
          UINT32_C(0x60a6b3), packets));
    CHECK(packets[0] == 0xb9u && packets[35] == 0x7fu);
    memset(packets, 0xa5, sizeof(packets));
    CHECK(!vf2_sound_l2_ad21_expand(rom, sizeof(rom),
          UINT32_C(0x60a6b4), packets));
    CHECK(memcmp(packets, expected, sizeof(packets)) == 0);
}


/* AD21 CF stream -> 12 ring packets -> original-shaped B0 table doors.
 * The BA command byte is NOT the upload selector: its second byte 2a/07
 * selects the 0x194a / 0x16ba table entries, not uploader 0x1ac0.
 * Even with valid synthetic sample rows 42 and 7, the original-shaped
 * packets MUST NOT stage ROM bytes or copy PCM samples. No ROM bytes
 * are committed here; the owner-ROM differential is separate.
 */
static void drain_ad21_ba_no_upload(void)
{
    static const uint8_t expected_first[2] = {0x17u, 0x42u};
    static const uint8_t expected_second[2] = {0x59u, 0x2du};
    vf2_sound_stream_maintenance_report l2;
    vf2_sound_dispatch_report dispatch;
    uint8_t original_scsp[VF2_SCSP_REGISTER_BYTES];
    uint8_t *table;
    size_t i;
    setup();

    /* Upload staging: one byte. Entry 0 -> 0x607400, 2 bytes. */
    rom[0x5b26u] = 0u; rom[0x5b27u] = 0u;
    rom[0x5b28u] = 0u; rom[0x5b29u] = 0x60u;
    rom[0x5b2au] = 0x74u; rom[0x5b2bu] = 0u;
    rom[0x5b2cu] = 0u; rom[0x5b2du] = 0u;
    rom[0x5b2eu] = 0xfeu; rom[0x5b2fu] = 2u;
    /* Entry 1 is 0x14 bytes after entry 0, source 0x607500. */
    rom[0x5b3cu] = 0u; rom[0x5b3du] = 0x60u;
    rom[0x5b3eu] = 0x75u; rom[0x5b3fu] = 0u;
    rom[0x5b40u] = 0u; rom[0x5b41u] = 0u;
    rom[0x5b42u] = 0xfeu; rom[0x5b43u] = 2u;
    rom[0x7400u] = expected_first[0];
    rom[0x7401u] = expected_first[1];
    rom[0x7500u] = expected_second[0];
    rom[0x7501u] = expected_second[1];

    /* Directory max=42. Pointer offset is base+2+index*4. */
    rom[0x8df0u] = 0u; rom[0x8df1u] = 0x2au;
    rom[0x8e9au] = 0u; rom[0x8e9bu] = 0x60u;
    rom[0x8e9cu] = 0x78u; rom[0x8e9du] = 0u; /* row 42 */
    rom[0x8e0eu] = 0u; rom[0x8e0fu] = 0x60u;
    rom[0x8e10u] = 0x78u; rom[0x8e11u] = 0x10u; /* row 7 */
    rom[0x7800u] = 0u; rom[0x7801u] = 0u;
    rom[0x7802u] = 0u; rom[0x7803u] = 0u; /* selector 0 */
    rom[0x7810u] = 0u; rom[0x7811u] = 0u;
    rom[0x7812u] = 0u; rom[0x7813u] = 1u; /* selector 1 */

    /* One live command descriptor matching BA's channel A. */
    table = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    table[0] = 0x80u;
    table[1] = 0x0au;
    memset(board.sound_ram + 0x50000u, 0x88, 4u);
    memset(board.sound_ram + 0x5000u, 0x55, 16u);
    CHECK(vf2_sound_board_maintain_streams(&board, &l2) == VF2_OK);
    CHECK(l2.packets_emitted == 12u);
    CHECK(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 12u);
    CHECK(l2.write_cursor_after == 0x44u);
    memcpy(original_scsp, board.scsp.registers, sizeof(original_scsp));

    for (i = 0u; i < 12u; ++i) {
        uint16_t cursor = (uint16_t)(0x14u + i * 4u);
        uint8_t command = (uint8_t)(0xb9u + i % 6u);
        uint8_t payload = (uint8_t)(i < 6u ? 0x2au : 7u);
        const uint8_t *ring = board.sound_ram + VF2_SOUND_COMMAND_RING_BASE;
        CHECK(ring[cursor] == command);
        CHECK(ring[cursor + 1u] == payload);
        CHECK(ring[cursor + 2u] == (uint8_t)(i < 6u ? 0u : 0x7fu));
        CHECK(ring[cursor + 3u] == 0xccu);
        memset(&dispatch, 0, sizeof(dispatch));
        CHECK(vf2_sound_board_dispatch_next(&board, &dispatch) == VF2_OK);
        CHECK(dispatch.command == command);
        CHECK(dispatch.payload == payload);
        CHECK(dispatch.cursor_before == cursor);
        CHECK(dispatch.cursor_after == (uint16_t)(cursor + 4u));
        CHECK(board.sound_ram[VF2_SOUND_COMMAND_COUNT] ==
              (uint8_t)(11u - i));
        if (command == 0xbau) {
            CHECK(dispatch.descriptors_matched == 1u);
            CHECK(dispatch.handler_address == UINT32_C(0x13cc));
            CHECK(dispatch.selected_value == payload);
        } else {
            CHECK(dispatch.descriptors_matched == 0u);
        }
        /* Neither original-shaped BA packet enters the uploader. */
        CHECK(board.sound_ram[0x5000u] == 0x55u);
        CHECK(board.sound_ram[0x50000u] == 0x88u);
        CHECK(board.sound_ram[0x50001u] == 0x88u);
        CHECK(memcmp(board.scsp.registers, original_scsp,
                     sizeof(original_scsp)) == 0);
    }
    CHECK(board.sound_ram[VF2_SOUND_COMMAND_CURSOR] == 0u);
    CHECK(board.sound_ram[VF2_SOUND_COMMAND_CURSOR + 1u] == 0x44u);
    CHECK(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
    CHECK(board.sound_ram[0x5000u] == 0x55u);
    CHECK(board.sound_ram[0x50000u] == 0x88u);
    CHECK(board.sound_ram[0x50001u] == 0x88u);
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
    case 1u: record[50u] ^= 1u; break; /* Bad FF 2F terminator */
    case 2u: ++desc[7u]; break; /* Unapproved adjacent record */
    case 3u: board.sound_ram[VF2_SOUND_COMMAND_COUNT] = 244u; break;
    case 4u: ++board.sound_ram[VF2_SOUND_COMMAND_WRITE_CURSOR + 1u]; break;
    case 5u: {
        uint8_t *other = desc + VF2_SOUND_STREAM_DESCRIPTOR_BYTES;
        memcpy(other, desc, VF2_SOUND_STREAM_DESCRIPTOR_BYTES);
        break;
    }
    case 6u: record[5u] ^= 1u; break; /* Incorrect BA body */
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
    boundary();
    drain_ad21_ba_no_upload();
    for (i = 0u; i < 7u; ++i) refuse(i);
    if (failed == 0) {
        puts("AD21 B/CF 12 original-shaped commands and seven rollback refusals PASS");
    }
    return failed != 0;
}
