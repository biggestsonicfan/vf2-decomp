#include "vf2/m68k_step.h"
#include "vf2/rom.h"
#include "vf2/sound_board.h"
#include "vf2/status.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The 0xcb2..0xd72 window is precisely 16 unrolled original MOVEM
 * triples, 48 original instructions = 512 bytes from sample ROM to
 * the 68000 sound RAM. This does NOT establish full copy-engine
 * completion, live game reachability or audible sound. */
static vf2_sound_board board;
static uint8_t reference_ram[VF2_SOUND_RAM_BYTES];
static vf2_scsp reference_scsp;
static unsigned failed;

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "FAIL %s:%u: %s\n", __FILE__, (unsigned)__LINE__, #expr); \
    ++failed; \
} } while (0)

static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24u) |
           ((uint32_t)p[1] << 16u) |
           ((uint32_t)p[2] << 8u) | (uint32_t)p[3];
}

static void test_rom_free(void)
{
    static uint8_t sample[1024];
    uint8_t original[512];
    uint32_t source = VF2_SOUND_SAMPLE_ROM_BASE + UINT32_C(16);
    uint32_t destination = UINT32_C(0x10000);
    uint32_t s, d;
    size_t i;

    memset(&board, 0, sizeof(board));
    for (i = 0u; i < sizeof(sample); ++i) {
        sample[i] = (uint8_t)(i ^ (i >> 3u) ^ 0x59u);
    }
    board.sample_rom = sample;
    board.sample_rom_size = sizeof(sample);
    memset(board.sound_ram + destination, 0xcc, 514u);
    CHECK(vf2_sound_board_copy_engine_page_512(
        &board, &source, &destination) == VF2_OK);
    CHECK(source == VF2_SOUND_SAMPLE_ROM_BASE + 16u + 512u);
    CHECK(destination == UINT32_C(0x10200));
    CHECK(memcmp(board.sound_ram + UINT32_C(0x10000),
                 sample + 16u, 512u) == 0);
    CHECK(board.sound_ram[UINT32_C(0x10200)] == 0xccu);
    CHECK(board.sound_ram[UINT32_C(0x10201)] == 0xccu);

    memcpy(original, board.sound_ram + UINT32_C(0x10000), sizeof(original));
    s = VF2_SOUND_SAMPLE_ROM_BASE + 1u;
    d = UINT32_C(0x10000);
    CHECK(vf2_sound_board_copy_engine_page_512(
        &board, &s, &d) == VF2_ERROR_UNSUPPORTED);
    CHECK(s == VF2_SOUND_SAMPLE_ROM_BASE + 1u && d == UINT32_C(0x10000));

    s = VF2_SOUND_SAMPLE_ROM_BASE;
    d = UINT32_C(0x10001);
    CHECK(vf2_sound_board_copy_engine_page_512(
        &board, &s, &d) == VF2_ERROR_UNSUPPORTED);
    s = VF2_SOUND_SAMPLE_ROM_BASE + 514u;
    d = UINT32_C(0x10000);
    CHECK(vf2_sound_board_copy_engine_page_512(
        &board, &s, &d) == VF2_ERROR_UNSUPPORTED);
    s = VF2_SOUND_SAMPLE_ROM_BASE;
    d = VF2_SOUND_RAM_BYTES - 510u;
    CHECK(vf2_sound_board_copy_engine_page_512(
        &board, &s, &d) == VF2_ERROR_UNSUPPORTED);
    CHECK(memcmp(original, board.sound_ram + UINT32_C(0x10000),
                 sizeof(original)) == 0);
    CHECK(vf2_sound_board_copy_engine_page_512(
        NULL, &s, &d) == VF2_ERROR_INVALID_ARGUMENT);
    CHECK(vf2_sound_board_copy_engine_page_512(
        &board, NULL, &d) == VF2_ERROR_INVALID_ARGUMENT);
    CHECK(vf2_sound_board_copy_engine_page_512(
        &board, &s, NULL) == VF2_ERROR_INVALID_ARGUMENT);
    board.sample_rom = NULL;
    CHECK(vf2_sound_board_copy_engine_page_512(
        &board, &s, &d) == VF2_ERROR_UNSUPPORTED);
}

/* ROM-free d72 tail: pointer files, SCSP KEYON and channel flag.
 * Inputs are an independently completed valid page-transfer fixture. */
static void test_rom_free_tail(void)
{
    static uint8_t sample[1024];
    uint8_t snapshot[16];
    uint8_t key = 0u;
    uint32_t stream = UINT32_C(0x2000);
    memset(&board, 0, sizeof(board));
    board.sample_rom = sample;
    board.sample_rom_size = sizeof(sample);
    board.sound_ram[VF2_SOUND_VOICE_TABLE_BASE] = 0u;
    board.sound_ram[VF2_SOUND_VOICE_TABLE_BASE + 1u] = 0u;
    CHECK(vf2_sound_board_finalize_engine_copy_d72(
        &board, 0u, UINT32_C(0x800200), UINT32_C(0x10200)) == VF2_OK);
    CHECK(board.sound_ram[stream] == 0x80u);
    CHECK(be32(board.sound_ram + stream + 2u) == UINT32_C(0x800200));
    CHECK(be32(board.sound_ram + stream + 6u) == UINT32_C(0x10200));
    CHECK(vf2_scsp_read_u8(&board.scsp, 0u, &key) == VF2_OK);
    CHECK((key & 0x18u) == 0x18u);
    memcpy(snapshot, board.sound_ram + stream, sizeof(snapshot));
    board.sound_ram[VF2_SOUND_VOICE_TABLE_BASE + 1u] = 1u;
    CHECK(vf2_sound_board_finalize_engine_copy_d72(
        &board, 0u, UINT32_C(0x800200), UINT32_C(0x10200)) ==
          VF2_ERROR_UNSUPPORTED);
    CHECK(memcmp(snapshot, board.sound_ram + stream, sizeof(snapshot)) == 0);
    /* Alternate 0xEB8 tail must preserve the exact same guarded
     * state transition with 0x81, not accidentally retain 0x80. */
    board.sound_ram[VF2_SOUND_VOICE_TABLE_BASE + 1u] = 0u;
    CHECK(vf2_sound_board_finalize_engine_copy_eb8(
        &board, 0u, UINT32_C(0x800200), UINT32_C(0x10200)) == VF2_OK);
    CHECK(board.sound_ram[stream] == 0x81u);
    CHECK(be32(board.sound_ram + stream + 2u) == UINT32_C(0x800200));
    CHECK(be32(board.sound_ram + stream + 6u) == UINT32_C(0x10200));
    board.sound_ram[VF2_SOUND_VOICE_TABLE_BASE + 1u] = 1u;
    CHECK(vf2_sound_board_finalize_engine_copy_eb8(
        &board, 0u, UINT32_C(0x800200), UINT32_C(0x10200)) ==
          VF2_ERROR_UNSUPPORTED);
    CHECK(board.sound_ram[stream] == 0x81u);
    CHECK(vf2_sound_board_finalize_engine_copy_d72(
        NULL, 0u, UINT32_C(0x800200), UINT32_C(0x10200)) ==
          VF2_ERROR_INVALID_ARGUMENT);
    CHECK(vf2_sound_board_finalize_engine_copy_d72(
        &board, VF2_SOUND_VOICE_COUNT,
        UINT32_C(0x800200), UINT32_C(0x10200)) ==
          VF2_ERROR_INVALID_ARGUMENT);
}

static void test_rom_case(const uint8_t *rom, size_t rom_size,
                          const uint8_t *samples, size_t sample_size,
                          uint16_t selection, uint32_t expected_source,
                          uint32_t copy_start, uint32_t copy_end,
                          uint32_t tail_end, uint8_t stream_flag)
{
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    uint32_t steps = 0u;
    uint32_t table = UINT32_C(0x5000) +
                     (uint32_t)(selection & 0x07ffu) * 20u;
    uint32_t source;
    uint32_t destination = UINT32_C(0x10000);
    size_t index;
    vf2_status status;
    unsigned failures_before = failed;

    CHECK(vf2_sound_board_initialize(&board, rom, rom_size,
                                     samples, sample_size) == VF2_OK);
    source = be32(board.sound_ram + table);
    if (source != expected_source) {
        fprintf(stderr, "copy: ROM table %04x source %08x expected %08x\n",
                (unsigned)selection, (unsigned)source,
                (unsigned)expected_source);
        ++failed;
        return;
    }
    /* The synthetic isolated tail is pinned to physical SCSP slot zero.
     * Do not conflate this fixture with a live game selection. */
    board.sound_ram[VF2_SOUND_VOICE_TABLE_BASE] = 0u;
    board.sound_ram[VF2_SOUND_VOICE_TABLE_BASE + 1u] = 0u;
    memcpy(reference_ram, board.sound_ram, sizeof(reference_ram));
    memcpy(&reference_scsp, &board.scsp, sizeof(reference_scsp));
    memset(&bus, 0, sizeof(bus));
    bus.ram = reference_ram;
    bus.ram_size = sizeof(reference_ram);
    bus.rom = rom;
    bus.rom_size = rom_size;
    bus.sample_rom = samples;
    bus.sample_rom_size = sample_size;
    bus.scsp = &reference_scsp;
    vf2_m68k_regs_reset(&regs);
    regs.pc = copy_start;
    regs.a[0] = source;
    regs.a[2] = destination;
    regs.a[7] = UINT32_C(0x7f00);

    status = vf2_sound_board_copy_engine_page_512(
        &board, &source, &destination);
    CHECK(status == VF2_OK);
    if (status != VF2_OK) {
        return;
    }

    status = vf2_m68k_run(&bus, &regs, copy_end,
                          0, 49u, &steps);
    CHECK(status == VF2_OK);
    CHECK(regs.pc == copy_end);
    CHECK(steps == 48u);
    CHECK(regs.a[0] == source);
    CHECK(regs.a[2] == destination);
    CHECK(source == expected_source + UINT32_C(0x200));
    CHECK(destination == UINT32_C(0x10200));
    CHECK(memcmp(board.sound_ram, reference_ram,
                 VF2_SOUND_RAM_BYTES) == 0);
    CHECK(memcmp(&board.scsp, &reference_scsp,
                 sizeof(reference_scsp)) == 0);
    /* The final MOVEM (A0)+,D0-D7 must expose the last 32 bytes
     * as eight big-endian longwords; no invented register residues. */
    for (index = 0u; index < 8u; ++index) {
        uint32_t relative = expected_source -
                            VF2_SOUND_SAMPLE_ROM_BASE + 480u +
                            (uint32_t)index * 4u;
        CHECK(relative + 4u <= sample_size);
        if (relative + 4u <= sample_size) {
            CHECK(regs.d[index] == be32(samples + relative));
        }
    }
    /* Continue from the original ROM-aligned tail: 0xD72..0xD90
     * or 0xEB8..0xED6, exactly 7 steps ending before RTS. */
    regs.a[1] = VF2_SOUND_VOICE_TABLE_BASE;
    regs.a[3] = UINT32_C(0x2000);
    regs.a[5] = VF2_SCSP_CPU_BASE;
    status = stream_flag == UINT8_C(0x80) ?
        vf2_sound_board_finalize_engine_copy_d72(
            &board, 0u, source, destination) :
        vf2_sound_board_finalize_engine_copy_eb8(
            &board, 0u, source, destination);
    CHECK(status == VF2_OK);
    if (status != VF2_OK) {
        return;
    }
    status = vf2_m68k_run(&bus, &regs, tail_end,
                          0, 8u, &steps);
    CHECK(status == VF2_OK);
    CHECK(regs.pc == tail_end);
    CHECK(steps == 7u);
    CHECK(memcmp(board.sound_ram, reference_ram,
                 VF2_SOUND_RAM_BYTES) == 0);
    CHECK(memcmp(&board.scsp, &reference_scsp,
                 sizeof(reference_scsp)) == 0);
    CHECK(reference_ram[UINT32_C(0x2000)] == stream_flag);
    CHECK(be32(reference_ram + UINT32_C(0x2002)) == source);
    CHECK(be32(reference_ram + UINT32_C(0x2006)) == destination);
    if (status == VF2_OK && failed == failures_before) {
        printf("MATCH 68000/C %05x..%05x select=%04x "
               "source=%08x bytes=512 instructions=55 flag=%02x\n",
               (unsigned)copy_start, (unsigned)tail_end,
               (unsigned)selection, (unsigned)expected_source,
               (unsigned)stream_flag);
    }
}

/* v0891: bridge ORIGINAL 68000 gameplay-dispatch-style setup,
 * through the recovered native-C d92 admission frontier, into
 * its actual DF8 copy+EB8 key-on branch.
 *
 * This is a STAGED sound-command fixture, not a naturally emitted
 * collision. The ROM original is run unmodified from 0x7A6 until
 * its own 0xDF8 entry and then through exactly 55 real instructions.
 * C code reaches the same 0xDF8 boundary, using independently
 * recovered dispatcher behavior. Drain bookkeeping is not asserted
 * equal here; the payload/voice/SCSP boundary is, and the following
 * 55-instruction original-ROM suffix uses one identical checkpoint. */
static void test_d92_to_ed6_bridge(const uint8_t *rom, size_t rom_size,
                                   const uint8_t *samples, size_t sample_size)
{
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report report;
    uint8_t *ram = board.sound_ram;
    uint8_t *desc;
    uint8_t *voice;
    vf2_status cstatus;
    vf2_status status;
    uint32_t steps = 0u, source, destination;
    unsigned before = failed;
    uint32_t stream = UINT32_C(0x2020);

    memset(&board, 0, sizeof(board));
    board.audio_rom = rom;
    board.audio_rom_size = rom_size;
    board.sample_rom = samples;
    board.sample_rom_size = sample_size;
    vf2_scsp_initialize(&board.scsp);
    CHECK(vf2_scsp_attach_sample_rom(&board.scsp, samples, sample_size) ==
          VF2_OK);
    memset(ram, 0, VF2_SOUND_RAM_BYTES);
    ram[VF2_SOUND_COMMAND_RING_BASE] = 0x93u;
    ram[VF2_SOUND_COMMAND_RING_BASE + 1u] = 0u;
    ram[VF2_SOUND_COMMAND_RING_BASE + 2u] = 1u;
    ram[VF2_SOUND_COMMAND_COUNT] = 1u;
    desc = ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    desc[0] = 0x80u;
    desc[1] = 0x03u;
    desc[2] = 0xf5u;
    desc[8] = 0x02u;
    desc[9] = 0x01u;
    desc[10] = 0x80u;
    desc[12] = 0x01u;
    ram[VF2_SOUND_SHARED_COUNTER_152E] = 5u;
    ram[0x4223u] = 0x07u;
    voice = ram + VF2_SOUND_VOICE_TABLE_BASE;
    voice[0] = 0u;
    voice[1] = 0x20u;
    voice[2] = 0x5au;
    voice[9] = 0xffu;
    /* Original 0xD9C MOVEA.L 0x0C(A1),A2: copied destination
     * comes from the VOICE's +0x0C longword. The earlier test
     * left this zero, correctly blocking unmapped RAM access. */
    voice[0x0cu] = 0x00u;
    voice[0x0du] = 0x01u;
    voice[0x0eu] = 0x00u;
    voice[0x0fu] = 0x00u;
    /* Setup-authored voice select 0x0140 => d3=0x1900.
     * Original 0xC68 loads D5 from table+0; 0xC74 MOVEA.L
     * D5,A0 gives the sample-ROM copy source. The other
     * table+4 field is independently staged for stream sample
     * metadata. These are deliberately constructed inputs,
     * not gameplay-derived event evidence. */
    ram[0x6900u] = 0x00u;
    ram[0x6901u] = 0xe6u;
    ram[0x6902u] = 0x31u;
    ram[0x6903u] = 0xb2u;
    ram[0x6904u] = 0x00u;
    ram[0x6905u] = 0xe6u;
    ram[0x6906u] = 0x31u;
    ram[0x6907u] = 0xb2u;
    ram[0x690cu] = 0x00u;
    ram[0x690du] = 0x11u;
    ram[0x690eu] = 0x22u;
    ram[0x690fu] = 0x33u;
    memcpy(reference_ram, ram, VF2_SOUND_RAM_BYTES);
    memcpy(&reference_scsp, &board.scsp, sizeof(reference_scsp));
    memset(&bus, 0, sizeof(bus));
    bus.ram = reference_ram;
    bus.ram_size = sizeof(reference_ram);
    bus.rom = rom;
    bus.rom_size = rom_size;
    bus.sample_rom = samples;
    bus.sample_rom_size = sample_size;
    bus.scsp = &reference_scsp;
    vf2_m68k_regs_reset(&regs);
    regs.a[4] = UINT32_C(0x2506);
    regs.a[5] = VF2_SCSP_CPU_BASE;
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x7a6);

    memset(&report, 0, sizeof(report));
    cstatus = vf2_sound_board_dispatch_next(&board, &report);
    CHECK(cstatus == VF2_ERROR_UNSUPPORTED);
    CHECK(report.handler_address == UINT32_C(0x0df8));
    CHECK(ram[VF2_SOUND_COMMAND_COUNT] == 1u);
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x0df8),
                          0, 20000u, &steps);
    if (status != VF2_OK || regs.pc != UINT32_C(0x0df8)) {
        fprintf(stderr, "d92 bridge: failed original entry "
                "status=%d pc=%08x steps=%u\n", (int)status,
                (unsigned)regs.pc, (unsigned)steps);
        ++failed;
        return;
    }
    CHECK(steps == 334u);
    CHECK(voice[2] == reference_ram[VF2_SOUND_VOICE_TABLE_BASE + 2u]);
    CHECK(memcmp(&board.scsp, &reference_scsp,
                 sizeof(reference_scsp)) == 0);
    CHECK(memcmp(ram + stream, reference_ram + stream, 32u) == 0);

    /* Independently reached REAL 68000 A0/A2 are prerequisites.
     * Never invent default mapped addresses for this chain. */
    source = regs.a[0];
    destination = regs.a[2];
    if (source < VF2_SOUND_SAMPLE_ROM_BASE ||
        destination > VF2_SOUND_RAM_BYTES - 512u ||
        (source & 1u) != 0u || (destination & 1u) != 0u) {
        fprintf(stderr, "d92 bridge: unmapped original copy inputs "
                "a0=%08x a2=%08x\n",
                (unsigned)source, (unsigned)destination);
        ++failed;
        return;
    }
    /* Sound CPU command-drain bookkeeping may differ between the
     * native C and stepper at this first unretired boundary. Only
     * normalize that bookkeeping by starting the suffix from the
     * SAME original RAM image after separately pinning relevant
     * voice/stream/SCSP equality above. */
    memcpy(ram, reference_ram, VF2_SOUND_RAM_BYTES);
    memcpy(&board.scsp, &reference_scsp, sizeof(reference_scsp));
    CHECK(vf2_sound_board_copy_engine_page_512(
        &board, &source, &destination) == VF2_OK);
    CHECK(vf2_sound_board_finalize_engine_copy_eb8(
        &board, 0u, source, destination) == VF2_OK);
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x0ed6),
                          0, 56u, &steps);
    CHECK(status == VF2_OK);
    CHECK(regs.pc == UINT32_C(0x0ed6));
    CHECK(steps == 55u);
    CHECK(regs.a[0] == source);
    CHECK(regs.a[2] == destination);
    CHECK(memcmp(ram, reference_ram, VF2_SOUND_RAM_BYTES) == 0);
    CHECK(memcmp(&board.scsp, &reference_scsp,
                 sizeof(reference_scsp)) == 0);
    CHECK(reference_ram[stream] == 0x81u);
    CHECK(be32(reference_ram + stream + 2u) == source);
    CHECK(be32(reference_ram + stream + 6u) == destination);
    if (before == failed) {
        printf("MATCH original dispatch D92->DF8->ED6 "
               "entry_steps=334 copy_tail_steps=55 "
               "sample=%08x dest=%08x flag=81\n",
               (unsigned)source, (unsigned)destination);
    }
}

static int test_owner_rom(const char *directory)
{
    uint8_t *rom = NULL;
    uint8_t *samples = NULL;
    size_t rom_size = 0u, sample_size = 0u;
    static const uint8_t group[] = {
        0x4cu, 0xd8u, 0x00u, 0xffu,
        0x48u, 0xd2u, 0x00u, 0xffu,
        0xd4u, 0xfcu, 0x00u, 0x20u
    };
    static const uint8_t tail[] = {
        0x38u, 0x29u, 0x00u, 0x00u,
        0x27u, 0x88u, 0x40u, 0x02u,
        0x27u, 0x8au, 0x40u, 0x06u,
        0x1au, 0x35u, 0x40u, 0x00u,
        0x00u, 0x05u, 0x00u, 0x18u,
        0x1bu, 0x85u, 0x40u, 0x00u,
        0x17u, 0xbcu, 0x00u, 0x80u, 0x40u, 0x00u
    };
    size_t i;
    if (vf2_romset_build_region(directory, VF2_REGION_AUDIOCPU,
                                &rom, &rom_size) != VF2_OK ||
        vf2_romset_build_region(directory, VF2_REGION_SAMPLES,
                                &samples, &sample_size) != VF2_OK ||
        rom == NULL || samples == NULL) {
        fprintf(stderr, "copy: owner ROM load failed\n");
        free(rom);
        free(samples);
        return 0;
    }
    for (i = 0u; i < 16u; ++i) {
        if (rom_size <= UINT32_C(0x0cb2) + i * sizeof(group) ||
            rom_size - (UINT32_C(0x0cb2) + i * sizeof(group)) <
                sizeof(group) ||
            memcmp(rom + UINT32_C(0x0cb2) + i * sizeof(group),
                   group, sizeof(group)) != 0) {
            fprintf(stderr, "copy: original ROM opcode triplet changed "
                    "at group %u\n", (unsigned)i);
            ++failed;
            free(rom);
            free(samples);
            return 0;
        }
    }
    if (rom_size < UINT32_C(0x0d72) + sizeof(tail) ||
        memcmp(rom + UINT32_C(0x0d72), tail, sizeof(tail)) != 0) {
        fprintf(stderr, "copy: 0xD72..0xD90 original ROM tail changed\n");
        ++failed;
        free(rom);
        free(samples);
        return 0;
    }
    /* Independent original-ROM identity guard for the alternate
     * 0xDF8..0xEB8 page and 0xEB8..0xED6 tail. The final 0x81
     * channel flag is the only differing byte from 0xD72's tail. */
    for (i = 0u; i < 16u; ++i) {
        size_t address = UINT32_C(0x0df8) + i * sizeof(group);
        if (rom_size < address + sizeof(group) ||
            memcmp(rom + address, group, sizeof(group)) != 0) {
            fprintf(stderr, "copy: alternate MOVEM mismatch group %u\n",
                    (unsigned)i);
            ++failed;
            free(rom);
            free(samples);
            return 0;
        }
    }
    if (rom_size < UINT32_C(0x0eb8) + sizeof(tail) ||
        memcmp(rom + UINT32_C(0x0eb8), tail,
               sizeof(tail) - 3u) != 0 ||
        rom[UINT32_C(0x0eb8) + sizeof(tail) - 3u] != 0x81u ||
        memcmp(rom + UINT32_C(0x0eb8) + sizeof(tail) - 2u,
               tail + sizeof(tail) - 2u, 2u) != 0) {
        fprintf(stderr, "copy: 0xEB8..0xED6 original tail mismatch\n");
        ++failed;
        free(rom);
        free(samples);
        return 0;
    }
    test_rom_case(rom, rom_size, samples, sample_size,
                  UINT16_C(0x0242), UINT32_C(0x00e631b2),
                  UINT32_C(0x0cb2), UINT32_C(0x0d72),
                  UINT32_C(0x0d90), UINT8_C(0x80));
    test_rom_case(rom, rom_size, samples, sample_size,
                  UINT16_C(0x0261), UINT32_C(0x00f04a72),
                  UINT32_C(0x0cb2), UINT32_C(0x0d72),
                  UINT32_C(0x0d90), UINT8_C(0x80));
    test_rom_case(rom, rom_size, samples, sample_size,
                  UINT16_C(0x0242), UINT32_C(0x00e631b2),
                  UINT32_C(0x0df8), UINT32_C(0x0eb8),
                  UINT32_C(0x0ed6), UINT8_C(0x81));
    test_rom_case(rom, rom_size, samples, sample_size,
                  UINT16_C(0x0261), UINT32_C(0x00f04a72),
                  UINT32_C(0x0df8), UINT32_C(0x0eb8),
                  UINT32_C(0x0ed6), UINT8_C(0x81));
    test_d92_to_ed6_bridge(rom, rom_size, samples, sample_size);
    free(rom);
    free(samples);
    return 1;
}

int main(int argc, char **argv)
{
    if (argc != 1 && argc != 2) {
        fprintf(stderr, "usage: %s [owner-vf2-rom-directory]\n", argv[0]);
        return 2;
    }
    test_rom_free();
    test_rom_free_tail();
    if (argc == 2 && !test_owner_rom(argv[1])) {
        return 2;
    }
    if (failed == 0u) {
        puts("MOVEM+KEYON: two original 55-instruction copy/tail paths PASS");
    }
    return failed == 0u ? 0 : 1;
}
