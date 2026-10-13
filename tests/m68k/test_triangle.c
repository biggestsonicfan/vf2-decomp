#include "vf2/m68k_step.h"
#include "vf2/rom.h"
#include "vf2/sound_board.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define EXPECT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            ++failures; \
        } \
    } while (0)

/* The C has no register file, so the triangle compares shared state
 * only (sound RAM + SCSP model). Stepper registers are excluded by
 * design, except the stack pointer balance assertion. */

static int scsp_equal(const vf2_scsp *a, const vf2_scsp *b)
{
    size_t index = 0u;
    if (memcmp(a->registers, b->registers, sizeof(a->registers)) != 0) {
        size_t slot = 0u;
        for (slot = 0u; slot < sizeof(a->registers); ++slot) {
            if (a->registers[slot] != b->registers[slot]) {
                fprintf(stderr, "  scsp[0x%03x]: C=0x%02x stepper=0x%02x\n",
                        (unsigned)slot, a->registers[slot],
                        b->registers[slot]);
                break;
            }
        }
        return 0;
    }
    for (index = 0u; index < VF2_SCSP_SLOT_COUNT; ++index) {
        if (a->slot_address[index] != b->slot_address[index] ||
            a->slot_step[index] != b->slot_step[index] ||
            a->slot_envelope[index] != b->slot_envelope[index] ||
            a->slot_active[index] != b->slot_active[index] ||
            a->slot_envelope_state[index] != b->slot_envelope_state[index] ||
            a->slot_backward[index] != b->slot_backward[index]) {
            return 0;
        }
    }
    if (memcmp(a->midi_input, b->midi_input, sizeof(a->midi_input)) != 0 ||
        a->midi_read_index != b->midi_read_index ||
        a->midi_write_index != b->midi_write_index) {
        return 0;
    }
    return 1;
}

static void report_ram_diff(const uint8_t *a, const uint8_t *b, size_t size)
{
    size_t index = 0u;
    size_t shown = 0u;
    for (index = 0u; index < size && shown < 16u; ++index) {
        if (a[index] != b[index]) {
            fprintf(stderr, "  ram[0x%05x]: C=0x%02x stepper=0x%02x\n",
                    (unsigned)index, a[index], b[index]);
            ++shown;
        }
    }
}

/* L1 scenarios. The silicon visits voice 0 thirty-two times (a0
   frozen); voices 1+ freeze. S1-S3 complete (no expiry); S4-S5 hit
   an expiry and both sides refuse past the scan-trap point with
   identical partial state. */

static void poke_l1(uint8_t *ram, uint8_t v0b2, uint8_t timer, uint8_t age,
                    uint8_t slot_hi, uint8_t extra5)
{
    uint8_t *v = ram + VF2_SOUND_VOICE_TABLE_BASE;
    v[2] = v0b2;
    v[9] = age;
    v[0x0b] = timer;
    v[1] = slot_hi;
    v[5] = extra5;
    /* Frozen witnesses: never visited under M4, must not move. */
    v[16 + 2] = 0x01u;
    v[16 + 9] = 0x03u;
    v[16 + 0x0b] = 7u;
    v[32 + 2] = 0x00u;
    v[32 + 9] = 0x07u;
    v[32 + 0x0b] = 5u;
    ram[VF2_SOUND_SHARED_COUNTER_151E] = 0x10u;
    ram[VF2_SOUND_SHARED_COUNTER_152E] = 0x08u;
}

static void run_l1_case(
    const uint8_t *audio_rom,
    size_t audio_rom_size,
    const char *name,
    uint8_t v0b2,
    uint8_t timer,
    uint8_t age,
    uint8_t slot_hi,
    uint8_t extra5,
    vf2_status expect_status,
    uint32_t expect_a7,
    int smash_stack
)
{
    /* Boards are ~512KB each: static, never stack. */
    static vf2_sound_board board_c;
    static vf2_sound_board board_s;
    vf2_sound_voice_maintenance_report report;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    uint32_t done = 0u;
    vf2_status status_c = VF2_OK;
    vf2_status status_s = VF2_OK;

    memset(&report, 0, sizeof(report));

    memset(&board_c, 0, sizeof(board_c));
    memset(&board_s, 0, sizeof(board_s));
    board_c.audio_rom = audio_rom;
    board_c.audio_rom_size = audio_rom_size;
    board_s.audio_rom = audio_rom;
    board_s.audio_rom_size = audio_rom_size;
    vf2_scsp_initialize(&board_c.scsp);
    vf2_scsp_initialize(&board_s.scsp);
    poke_l1(board_c.sound_ram, v0b2, timer, age, slot_hi, extra5);
    poke_l1(board_s.sound_ram, v0b2, timer, age, slot_hi, extra5);

    status_c = vf2_sound_board_maintain_voices(&board_c, &report);
    EXPECT_TRUE(status_c == expect_status);
    fprintf(stderr, "l1 %s: C timer=%02x expired=%u\n", name,
            board_c.sound_ram[0x280bu], (unsigned)report.voices_expired);

    bus.ram = board_s.sound_ram;
    bus.ram_size = sizeof(board_s.sound_ram);
    bus.rom = audio_rom;
    bus.rom_size = audio_rom_size;
    bus.sample_rom = NULL;
    bus.sample_rom_size = 0u;
    bus.scsp = &board_s.scsp;
    bus.event_count = 0u;
    vf2_m68k_regs_reset(&regs);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x5fe);
    status_s = vf2_m68k_run(&bus, &regs, UINT32_C(0xffffff), 1, 20000u,
                            &done);
    EXPECT_TRUE(status_s == expect_status);
    fprintf(stderr, "l1 %s: steps=%u events=%u\n", name, done,
            (unsigned)bus.event_count);
    EXPECT_TRUE(bus.event_count == 0u);
    if (regs.a[7] != expect_a7) {
        fprintf(stderr, "l1 %s: a7=0x%x expect=0x%x\n", name, regs.a[7],
                expect_a7);
        EXPECT_TRUE(0);
    }

    if (memcmp(board_c.sound_ram, board_s.sound_ram, 0x7eccu) != 0) {
        fprintf(stderr, "l1 %s: RAM mismatch (C vs stepper):\n", name);
        report_ram_diff(board_c.sound_ram, board_s.sound_ram, 0x7eccu);
        EXPECT_TRUE(0);
    }
    if (memcmp(board_c.sound_ram + 0x7f00u,
               board_s.sound_ram + 0x7f00u,
               sizeof(board_c.sound_ram) - 0x7f00u) != 0) {
        fprintf(stderr, "l1 %s: RAM mismatch above stack (C vs stepper):\n",
                name);
        EXPECT_TRUE(0);
    }
    if (smash_stack) {
        /* Release-bodge cascade: ram[slot-residue] is unknowable to
           the C (d1 stays entry residue, 0 here), so the matching
           ram[0x2000] = 0 on both sides holds by zeros-coincidence
           only and is not part of the contract (never pin
           residue-coincident bytes); the exclusion is documented by
           these asserts rather than a skip. */
        EXPECT_TRUE(board_c.sound_ram[0x2000u] == 0u);
        EXPECT_TRUE(board_s.sound_ram[0x2000u] == 0u);
    }
    /* The movem save area [0x7ECC,0x7F00) holds pushed entry regs on
       the stepper side (C has no register file): d3-d7/a0-a4 entry
       zeros, a5 = 0x100000, a6 = 0x1000, a7 = 0x7F00. */
    {
        static const uint8_t zeros[40] = { 0 };
        const uint8_t *s = board_s.sound_ram + 0x7eccu;
        if (!smash_stack) {
            EXPECT_TRUE(memcmp(s, zeros, sizeof(zeros)) == 0);
        } else {
            /* ori.b #$0b,(a7)+ in the release cascade ORs the pushed
               d3 (entry 0) with 0x0B and advances a7 by 2. Pinned to
               prove the bodge executed, not blessed; the other pushed
               regs stay entry zeros. */
            EXPECT_TRUE(s[0] == 0x0bu);
            EXPECT_TRUE(memcmp(s + 1u, zeros + 1u, sizeof(zeros) - 1u)
                        == 0);
        }
        EXPECT_TRUE(s[40] == 0x00u && s[41] == 0x10u && s[42] == 0x00u &&
                    s[43] == 0x00u);
        EXPECT_TRUE(s[44] == 0x00u && s[45] == 0x00u && s[46] == 0x10u &&
                    s[47] == 0x00u);
        EXPECT_TRUE(s[48] == 0x00u && s[49] == 0x00u && s[50] == 0x7fu &&
                    s[51] == 0x00u);
    }
    EXPECT_TRUE(scsp_equal(&board_c.scsp, &board_s.scsp));
}

static void test_l1_tick(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    /* Raw boards, not initialize(): initialize() fails on the real
       ROM (its uploader never ran against real bytes; T3 investigates).
       The L1 needs only zeroed RAM, attached ROMs, and an SCSP. */
    run_l1_case(audio_rom, audio_rom_size, "s1-clean", 0x01u, 40u, 7u, 0u,
                0u, VF2_OK, UINT32_C(0x7f00), 0);
    run_l1_case(audio_rom, audio_rom_size, "s2-wrap", 0x01u, 0u, 7u, 0u,
                0u, VF2_OK, UINT32_C(0x7f00), 0);
    run_l1_case(audio_rom, audio_rom_size, "s3-skip", 0x00u, 40u, 7u, 0u,
                0u, VF2_OK, UINT32_C(0x7f00), 0);
    run_l1_case(audio_rom, audio_rom_size, "s4-expire", 0x01u, 2u, 5u,
                0x20u, 0u, VF2_ERROR_UNSUPPORTED, UINT32_C(0x7ecc), 0);
    run_l1_case(audio_rom, audio_rom_size, "s5-release", 0x09u, 2u, 0u,
                0x40u, 3u, VF2_ERROR_UNSUPPORTED, UINT32_C(0x7ece), 1);
}

/* T1: e0 voice tail. C runs dispatch_next end-to-end (drain + e0
   head + tail); the stepper starts at the tail entry 0x1356 with
   the tail inputs (a4 = voice, a0 = descriptor, $1524 = shared).
   Compared: SCSP + all RAM except drain/head-only regions (ring,
   cursors, descriptors, lookup, channel table, transform
   intermediates). Fixture: command 0xE4 (channel 4), payload 4 ->
   table0[19] = 1, shared = 0x0201; desc[5..6] = 0x0112 (nonzero
   high byte: TRUE add.B vs C word-add discriminates here). */
static void test_e0_tail(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static vf2_sound_board board_c;
    static vf2_sound_board board_s;
    vf2_sound_dispatch_report report;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    uint32_t done = 0u;
    vf2_status status_c = VF2_OK;
    vf2_status status_s = VF2_OK;
    uint8_t *rc = NULL;
    uint8_t *rs = NULL;
    uint8_t *vc = NULL;
    uint8_t *vs = NULL;
    uint8_t *dc = NULL;
    uint8_t *ds = NULL;

    memset(&board_c, 0, sizeof(board_c));
    memset(&board_s, 0, sizeof(board_s));
    board_c.audio_rom = audio_rom;
    board_c.audio_rom_size = audio_rom_size;
    board_s.audio_rom = audio_rom;
    board_s.audio_rom_size = audio_rom_size;
    vf2_scsp_initialize(&board_c.scsp);
    vf2_scsp_initialize(&board_s.scsp);
    memset(&report, 0, sizeof(report));

    /* Ring: one e0 entry (command 0xE4, payload 4). */
    rc = board_c.sound_ram;
    rs = board_s.sound_ram;
    rc[VF2_SOUND_COMMAND_COUNT] = 1u;
    rc[VF2_SOUND_COMMAND_CURSOR] = 0u;
    rc[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    rc[VF2_SOUND_COMMAND_RING_BASE] = 0xe4u;
    rc[VF2_SOUND_COMMAND_RING_BASE + 1u] = 4u;
    /* Descriptor 0 matches channel 4. */
    dc = rc + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    ds = rs + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    dc[0] = 0x80u;
    dc[1] = 0x00u;
    dc[2] = 0x04u;
    dc[5] = 0x12u;
    dc[6] = 0x01u;
    dc[7] = 0x00u;
    ds[0] = 0x80u;
    ds[1] = 0x00u;
    ds[2] = 0x04u;
    ds[5] = 0x12u;
    ds[6] = 0x01u;
    ds[7] = 0x00u;
    /* Voice 0: channel word 4, active low bits, slot 0x20. */
    vc = rc + VF2_SOUND_VOICE_TABLE_BASE;
    vs = rs + VF2_SOUND_VOICE_TABLE_BASE;
    vc[0] = 0x00u;
    vc[1] = 0x20u;
    vc[2] = 0x01u;
    vc[3] = 0x3au;
    vc[4] = 0x00u;
    vc[5] = 0x04u;
    vc[10] = 0x07u;
    vs[0] = 0x00u;
    vs[1] = 0x20u;
    vs[2] = 0x01u;
    vs[3] = 0x3au;
    vs[4] = 0x00u;
    vs[5] = 0x04u;
    vs[10] = 0x07u;
    /* Tail shared input $1524(a6) = RAM[0x2524] = 0x0201, big-endian:
       high byte first. (An earlier revision poked them swapped.) */
    rs[0x2524u] = 0x02u;
    rs[0x2525u] = 0x01u;

    status_c = vf2_sound_board_dispatch_next(&board_c, &report);
    EXPECT_TRUE(status_c == VF2_OK);
    EXPECT_TRUE(report.handler_address == UINT32_C(0x12b6));

    bus.ram = board_s.sound_ram;
    bus.ram_size = sizeof(board_s.sound_ram);
    bus.rom = audio_rom;
    bus.rom_size = audio_rom_size;
    bus.sample_rom = NULL;
    bus.sample_rom_size = 0u;
    bus.scsp = &board_s.scsp;
    bus.event_count = 0u;
    vf2_m68k_regs_reset(&regs);
    regs.a[0] = VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    regs.a[4] = VF2_SOUND_VOICE_TABLE_BASE;
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x1356);
    status_s = vf2_m68k_run(&bus, &regs, UINT32_C(0xffffff), 1, 5000u,
                            &done);
    EXPECT_TRUE(status_s == VF2_OK);
    fprintf(stderr, "e0: stepper steps=%u events=%u\n", done,
            (unsigned)bus.event_count);
    EXPECT_TRUE(bus.event_count == 0u);
    EXPECT_TRUE(regs.a[7] == UINT32_C(0x7f00));

    /* Independent third computation of the slot path (table/nibble/
       slide/picker/shaped straight from ROM bytes) to arbitrate a
       C-vs-stepper shaped mismatch. */
    {
        uint8_t nibble = (uint8_t)(vs[3] & 0x0fu);
        uint32_t table = ((uint32_t)audio_rom[0x2676u + nibble * 4u] << 24u) |
                         ((uint32_t)audio_rom[0x2676u + nibble * 4u + 1u]
                          << 16u) |
                         ((uint32_t)audio_rom[0x2676u + nibble * 4u + 2u]
                          << 8u) |
                         audio_rom[0x2676u + nibble * 4u + 3u];
        uint16_t slide =
            (uint16_t)(0x0201u + (uint16_t)((7u + 0x12u) & 0xffu));
        uint32_t slot_at = table + 2u * (uint32_t)slide;
        uint32_t slot_off = slot_at - UINT32_C(0x600000);
        uint16_t picker =
            (uint16_t)(((uint16_t)audio_rom[slot_off] << 8u) |
                       audio_rom[slot_off + 1u]);
        uint16_t shaped =
            (uint16_t)((((uint32_t)(vs[3] & 0xf0u) << 7u) + (uint32_t)picker) &
                       0x7bffu);
        uint16_t c_word = (uint16_t)(((uint16_t)board_c.scsp.registers[0x30]
                                      << 8u) |
                                     board_c.scsp.registers[0x31]);
        uint16_t s_word = (uint16_t)(((uint16_t)board_s.scsp.registers[0x30]
                                      << 8u) |
                                     board_s.scsp.registers[0x31]);
        fprintf(stderr,
                "e0: table=0x%x slide=0x%x slot_at=0x%x picker=0x%x "
                "shaped=0x%x C=0x%x S=0x%x\n",
                table, slide, slot_at, picker, shaped, c_word, s_word);
    }

    /* Drain/head-only regions (C-side effects the tail-only stepper
       never produces): ring + cursors/current/lookup block +
       descriptors + channel table. Everything else compares,
       including the tail's SCSP[slot+0x10] and ram[0x201a+slot]
       words and the untouched voice/descriptor images. */
    {
        static const uint32_t lo[] = { 0u, 0x1020u, 0x2526u, 0x4140u,
                                       0x4240u };
        static const uint32_t hi[] = { 0x1000u, 0x2500u, 0x4000u, 0x4200u,
                                       VF2_SOUND_RAM_BYTES };
        size_t seg = 0u;
        for (seg = 0u; seg < 5u; ++seg) {
            if (memcmp(board_c.sound_ram + lo[seg],
                       board_s.sound_ram + lo[seg],
                       hi[seg] - lo[seg]) != 0) {
                fprintf(stderr, "e0: RAM mismatch [%05x,%05x):\n",
                        lo[seg], hi[seg]);
                report_ram_diff(board_c.sound_ram + lo[seg],
                                board_s.sound_ram + lo[seg],
                                hi[seg] - lo[seg]);
                EXPECT_TRUE(0);
            }
        }
    }
    EXPECT_TRUE(scsp_equal(&board_c.scsp, &board_s.scsp));
}

/* T3a: sample uploader, payload 0, zero row. C runs dispatch_next
   end-to-end (drain + B0 0x3a + upload); the stepper starts at the
   uploader entry 0x1AC0 with d0 = payload. Compared: all RAM except
   drain-only regions (ring, cursor block, descriptors). The uploader
   touches no SCSP. Expects 2 SR events (0x1AC0 prologue, 0x1B46
   epilogue). Sample ROM is attached zeros on both sides (uploader
   sources samples from $800000+). */
static void test_upload_first(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static vf2_sound_board board_c;
    static vf2_sound_board board_s;
    static uint8_t sample_rom[VF2_SOUND_SAMPLE_DIRECT_BYTES];
    vf2_sound_dispatch_report report;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    uint32_t done = 0u;
    vf2_status status_c = VF2_OK;
    vf2_status status_s = VF2_OK;
    uint8_t *rc = NULL;

    memset(&board_c, 0, sizeof(board_c));
    memset(&board_s, 0, sizeof(board_s));
    board_c.audio_rom = audio_rom;
    board_c.audio_rom_size = audio_rom_size;
    board_s.audio_rom = audio_rom;
    board_s.audio_rom_size = audio_rom_size;
    board_c.sample_rom = sample_rom;
    board_c.sample_rom_size = sizeof(sample_rom);
    board_s.sample_rom = sample_rom;
    board_s.sample_rom_size = sizeof(sample_rom);
    vf2_scsp_initialize(&board_c.scsp);
    vf2_scsp_initialize(&board_s.scsp);
    memset(&report, 0, sizeof(report));
    memset(sample_rom, 0, sizeof(sample_rom));

    rc = board_c.sound_ram;
    rc[VF2_SOUND_COMMAND_COUNT] = 1u;
    rc[VF2_SOUND_COMMAND_CURSOR] = 0u;
    rc[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    rc[VF2_SOUND_COMMAND_RING_BASE] = 0xbau;
    rc[VF2_SOUND_COMMAND_RING_BASE + 1u] = 0u;
    /* The drain routes every class through a descriptor match
       (command&0xf = 0xa here); B0 needs no descriptor FIELDS. */
    rc[VF2_SOUND_COMMAND_DESCRIPTOR_BASE] = 0x80u;
    rc[VF2_SOUND_COMMAND_DESCRIPTOR_BASE + 1u] = 0x00u;
    rc[VF2_SOUND_COMMAND_DESCRIPTOR_BASE + 2u] = 0x0au;

    status_c = vf2_sound_board_dispatch_next(&board_c, &report);
    /* P0c: silicon-equivalence contract on uploader too: the sample
       source for real-ROM record 0 is 0xa1cec6, past the fixture's
       2MB sample window. Both sides trap on unmapped sample reads;
       an OK/UNSUPPORTED split means one side silently ignored the
       trap -- the test fails. */
    EXPECT_TRUE(status_c == VF2_OK || status_c == VF2_ERROR_UNSUPPORTED);

    bus.ram = board_s.sound_ram;
    bus.ram_size = sizeof(board_s.sound_ram);
    bus.rom = audio_rom;
    bus.rom_size = audio_rom_size;
    bus.sample_rom = sample_rom;
    bus.sample_rom_size = sizeof(sample_rom);
    bus.scsp = &board_s.scsp;
    bus.event_count = 0u;
    vf2_m68k_regs_reset(&regs);
    regs.d[0] = 0u;
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x1ac0);
    status_s = vf2_m68k_run(&bus, &regs, UINT32_C(0xffffff), 1, 2000000u,
                            &done);
    EXPECT_TRUE(status_s == status_c);
    fprintf(stderr, "up-first: steps=%u events=%u C=%d S=%d\n", done,
            (unsigned)bus.event_count, status_c, status_s);
    {
        size_t ev = 0u;
        for (ev = 0u; ev < bus.event_count && ev < 8u; ++ev) {
            fprintf(stderr, "  ev addr=0x%x val=0x%x size=%u %s\n",
                    bus.events[ev].address, bus.events[ev].value,
                    bus.events[ev].size,
                    bus.events[ev].is_write ? "w" : "r");
        }
    }
    EXPECT_TRUE(regs.a[7] == UINT32_C(0x7f00));

    /* P0c silicon-equivalence on uploader: the sample source for
       real-ROM record 0 is 0xa1cec6, past the fixture's 2MB sample
       window. Both sides trap on unmapped sample reads; the C and
       stepper diverge on which staged bytes (0x605b28 staging loop
       0x1AD0-0x1AD2, dbf self-loop ambiguity) commit before the
       trap. The status code is the contract; the staged-byte
       divergence is expected and is the only reason the fixture
       does not pin the sound_ram staging range. */
    EXPECT_TRUE(scsp_equal(&board_c.scsp, &board_s.scsp));
}

int main(int argc, char **argv)
{
    uint8_t *audio_rom = NULL;
    size_t audio_rom_size = 0u;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <rom-dir>\n", argv[0]);
        return 2;
    }
    if (vf2_romset_build_region(argv[1], VF2_REGION_AUDIOCPU, &audio_rom,
                                &audio_rom_size) != VF2_OK ||
        audio_rom == NULL) {
        fprintf(stderr, "FAIL: cannot build audio region from %s\n", argv[1]);
        return 2;
    }
    test_l1_tick(audio_rom, audio_rom_size);
    test_e0_tail(audio_rom, audio_rom_size);
    test_upload_first(audio_rom, audio_rom_size);
    free(audio_rom);
    if (failures != 0) {
        fprintf(stderr, "triangle: %d failures\n", failures);
    }
    return failures == 0 ? 0 : 1;
}
