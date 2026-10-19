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
                          uint16_t selection, uint32_t expected_source)
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
    regs.pc = UINT32_C(0x0cb2);
    regs.a[0] = source;
    regs.a[2] = destination;
    regs.a[7] = UINT32_C(0x7f00);

    status = vf2_sound_board_copy_engine_page_512(
        &board, &source, &destination);
    CHECK(status == VF2_OK);
    if (status != VF2_OK) {
        return;
    }

    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x0d72),
                          0, 49u, &steps);
    CHECK(status == VF2_OK);
    CHECK(regs.pc == UINT32_C(0x0d72));
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
    /* Continue from the same original state: the ROM tail at
     * 0xD72..0xD90 performs seven more instructions before RTS.
     * Compare whole RAM + SCSP with the separately recovered C tail. */
    regs.a[1] = VF2_SOUND_VOICE_TABLE_BASE;
    regs.a[3] = UINT32_C(0x2000);
    regs.a[5] = VF2_SCSP_CPU_BASE;
    status = vf2_sound_board_finalize_engine_copy_d72(
        &board, 0u, source, destination);
    CHECK(status == VF2_OK);
    if (status != VF2_OK) {
        return;
    }
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x0d90),
                          0, 8u, &steps);
    CHECK(status == VF2_OK);
    CHECK(regs.pc == UINT32_C(0x0d90));
    CHECK(steps == 7u);
    CHECK(memcmp(board.sound_ram, reference_ram,
                 VF2_SOUND_RAM_BYTES) == 0);
    CHECK(memcmp(&board.scsp, &reference_scsp,
                 sizeof(reference_scsp)) == 0);
    CHECK(reference_ram[UINT32_C(0x2000)] == 0x80u);
    CHECK(be32(reference_ram + UINT32_C(0x2002)) == source);
    CHECK(be32(reference_ram + UINT32_C(0x2006)) == destination);
    if (status == VF2_OK && failed == failures_before) {
        printf("MATCH 68000/C 0xcb2..0xd90 select=%04x "
               "source=%08x bytes=512 instructions=55\n",
               (unsigned)selection, (unsigned)expected_source);
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
    test_rom_case(rom, rom_size, samples, sample_size,
                  UINT16_C(0x0242), UINT32_C(0x00e631b2));
    test_rom_case(rom, rom_size, samples, sample_size,
                  UINT16_C(0x0261), UINT32_C(0x00f04a72));
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
        puts("MOVEM+KEYON 0xcb2..0xd90: bounded copy/tail tests PASS");
    }
    return failed == 0u ? 0 : 1;
}
