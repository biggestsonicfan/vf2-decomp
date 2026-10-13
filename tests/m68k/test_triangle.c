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

/* L1 scenarios (v0821: true 32-voice scan, every voice visited
 * once). S1-S3 complete with no expiry; S4 takes the normal arm
 * (timer 1 -> 0, key-off slot 0x20, $151e--, +9 rebalance); S5 takes
 * the release arm (timer 1 -> 0, key-off slot 0x40, $152e--, +5
 * rebalance over 0xff-aged slots). The old frozen-pointer/bodge
 * pins were +4 artefacts and are retired. */

static void poke_l1(uint8_t *ram, uint8_t v0b2, uint8_t timer, uint8_t age,
                    uint8_t slot_hi, uint8_t extra5,
                    uint8_t v2_9, uint8_t v2_5, uint8_t v3_9, uint8_t v3_5)
{
    uint8_t *v = ram + VF2_SOUND_VOICE_TABLE_BASE;
    v[2] = v0b2;
    v[9] = age;
    v[0x0b] = timer;
    v[1] = slot_hi;
    v[5] = extra5;
    /* Voice 1: visited once (timer 7 -> 6 when active). */
    v[16 + 2] = 0x01u;
    v[16 + 9] = 0x03u;
    v[16 + 0x0b] = 7u;
    /* Voice 2: rebalance witness (normal arm decrements +9 when
       mem > d2; release arm decrements +5 iff +9 == 0xff). */
    v[32 + 2] = 0x00u;
    v[32 + 9] = v2_9;
    v[32 + 5] = v2_5;
    v[32 + 0x0b] = 5u;
    /* Voice 3: second release witness. */
    v[48 + 2] = 0x00u;
    v[48 + 9] = v3_9;
    v[48 + 5] = v3_5;
    v[48 + 0x0b] = 5u;
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
    uint8_t v2_9,
    uint8_t v2_5,
    uint8_t v3_9,
    uint8_t v3_5,
    size_t expect_expired,
    size_t expect_normal,
    size_t expect_release
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
    poke_l1(board_c.sound_ram, v0b2, timer, age, slot_hi, extra5,
            v2_9, v2_5, v3_9, v3_5);
    poke_l1(board_s.sound_ram, v0b2, timer, age, slot_hi, extra5,
            v2_9, v2_5, v3_9, v3_5);

    status_c = vf2_sound_board_maintain_voices(&board_c, &report);
    EXPECT_TRUE(status_c == VF2_OK);
    EXPECT_TRUE(report.voices_scanned == 32u);
    EXPECT_TRUE(report.voices_expired == expect_expired);
    EXPECT_TRUE(report.normal_expirations == expect_normal);
    EXPECT_TRUE(report.release_expirations == expect_release);
    fprintf(stderr, "l1 %s: C timer=%02x expired=%u aged=%u\n", name,
            board_c.sound_ram[0x280bu], (unsigned)report.voices_expired,
            (unsigned)report.voices_aged);

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
    EXPECT_TRUE(status_s == VF2_OK);
    fprintf(stderr, "l1 %s: steps=%u events=%u\n", name, done,
            (unsigned)bus.event_count);
    EXPECT_TRUE(bus.event_count == 0u);
    if (regs.a[7] != UINT32_C(0x7f00)) {
        fprintf(stderr, "l1 %s: a7=0x%x expect=0x7f00\n", name,
                regs.a[7]);
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
    /* The movem save area [0x7ECC,0x7F00) holds pushed entry regs on
       the stepper side (C has no register file): d3-d7/a0-a4 entry
       zeros, a5 = 0x100000, a6 = 0x1000, a7 = 0x7F00. The ISR always
       balances the stack (rte), so the area is identical on every
       leg; the old ori.b-bodge pin was a +4 artefact. */
    {
        static const uint8_t zeros[40] = { 0 };
        const uint8_t *s = board_s.sound_ram + 0x7eccu;
        EXPECT_TRUE(memcmp(s, zeros, sizeof(zeros)) == 0);
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
    /* s1: voice0 ticks 40 -> 39, voice1 ticks 7 -> 6, no expiry. */
    run_l1_case(audio_rom, audio_rom_size, "s1-clean", 0x01u, 40u, 7u, 0u,
                0u, 7u, 0u, 0u, 0u, 0u, 0u, 0u);
    /* s2: timer wraps 0 -> 0xff, bne skips expiry. */
    run_l1_case(audio_rom, audio_rom_size, "s2-wrap", 0x01u, 0u, 7u, 0u,
                0u, 7u, 0u, 0u, 0u, 0u, 0u, 0u);
    /* s3: bit0 clear skips voice0 entirely (timer stays 0x28). */
    run_l1_case(audio_rom, audio_rom_size, "s3-skip", 0x00u, 40u, 7u, 0u,
                0u, 7u, 0u, 0u, 0u, 0u, 0u, 0u);
    /* s4: normal expiry (timer 1 -> 0, slot 0x20, age 5, $151e--).
       Rebalance with d2 = 5: voice1 +9 = 3 skips (5 >= 3), voice2
       +9 = 7 decrements to 6. */
    run_l1_case(audio_rom, audio_rom_size, "s4-expire", 0x01u, 1u, 5u,
                0x20u, 0u, 7u, 0u, 0u, 0u, 1u, 1u, 0u);
    /* s5: release expiry (timer 1 -> 0, slot 0x40, +5 = 3, $152e--).
       Rebalance with d2 = 3 over 0xff-aged slots: voice2 +5 = 7
       decrements to 6, voice3 +5 = 2 skips (3 >= 2). */
    run_l1_case(audio_rom, audio_rom_size, "s5-release", 0x09u, 1u, 0u,
                0x40u, 3u, 0xffu, 7u, 0xffu, 2u, 1u, 0u, 1u);
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
    uint8_t *dc = NULL;

    memset(&board_c, 0, sizeof(board_c));
    memset(&board_s, 0, sizeof(board_s));
    board_c.audio_rom = audio_rom;
    board_c.audio_rom_size = audio_rom_size;
    board_s.audio_rom = audio_rom;
    board_s.audio_rom_size = audio_rom_size;
    vf2_scsp_initialize(&board_c.scsp);
    vf2_scsp_initialize(&board_s.scsp);
    memset(&report, 0, sizeof(report));

/* T1: e0 head+tail on both sides (v0821: head-entry, not
 * tail-only). C runs dispatch_next end-to-end (drain + e0 head +
 * tail); the stepper enters at the head 0x12b4 with the same inputs
 * (a3 stream mirrors the ring bytes, a0 = descriptor, $250e filed
 * as the drain would file it) and runs to the tail exit 0x1354.
 * Compared: SCSP + all RAM except drain/head-only regions (ring,
 * cursors, descriptors, lookup, channel table, transform
 * intermediates). Fixture: command 0xE4 (channel 4), payload 4,
 * desc[5..6] = 0x0112 (nonzero high byte: add.B `voice[10]+desc[5]`
 * vs a word-add discriminates here -- ROM 0x135e is add.b).
 * NOTE (v0821): the old tail-only form poked shared $1524 = 0x0201
 * on the stepper side, a value the TRUE head (no stale term, S7b)
 * can never produce -- it was reachable only under the retired
 * stale rule. Both sides now run the head, so shared is an
 * agreement not an input; the arbitrator reads it back from the C
 * RAM instead of hardcoding it. */

    /* Ring: one e0 entry (command 0xE4, payload 4). */
    rc = board_c.sound_ram;
    rc[VF2_SOUND_COMMAND_COUNT] = 1u;
    rc[VF2_SOUND_COMMAND_CURSOR] = 0u;
    rc[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    rc[VF2_SOUND_COMMAND_RING_BASE] = 0xe4u;
    rc[VF2_SOUND_COMMAND_RING_BASE + 1u] = 4u;
    /* Filed current-command, as the drain would file it; the stepper
       side inherits it via the memcpy below. */
    rc[VF2_SOUND_CURRENT_COMMAND] = 0xe4u;
    /* Descriptor 0 matches channel 4. */
    dc = rc + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    dc[0] = 0x80u;
    dc[1] = 0x00u;
    dc[2] = 0x04u;
    dc[5] = 0x12u;
    dc[6] = 0x01u;
    dc[7] = 0x00u;
    /* Voice 0: channel byte 4 (ROM 0x1332 is cmp.B on voice[4],
       v0821), active low bits, slot 0x20. */
    vc = rc + VF2_SOUND_VOICE_TABLE_BASE;
    vc[0] = 0x00u;
    vc[1] = 0x20u;
    vc[2] = 0x01u;
    vc[3] = 0x3au;
    vc[4] = 0x04u;
    vc[5] = 0x04u;
    vc[10] = 0x07u;
    /* Stepper side starts identical (head reads the same descriptor,
       voice, filed command; the stream below mirrors the ring). The
       stream staging lives in compared RAM, so it is staged on BOTH
       sides (the C never reads it -- identical inputs, not an
       agreement). */
    rs = board_s.sound_ram;
    memcpy(rs, rc, VF2_SOUND_RAM_BYTES);
    rc[0x7300u] = 0xe4u;
    rc[0x7301u] = 4u;

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
    regs.a[3] = UINT32_C(0x7300);
    regs.a[0] = VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x12b4);
    rs[0x7300u] = 0xe4u;
    rs[0x7301u] = 4u;
    status_s = vf2_m68k_run(&bus, &regs, UINT32_C(0x1354), 0, 20000u,
                            &done);
    EXPECT_TRUE(status_s == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x1354));
    fprintf(stderr, "e0: stepper steps=%u events=%u\n", done,
            (unsigned)bus.event_count);
    EXPECT_TRUE(bus.event_count == 0u);
    EXPECT_TRUE(regs.a[7] == UINT32_C(0x7f00));

    /* Independent third computation of the slot path (table/nibble/
       slide/picker/shaped straight from ROM bytes) to arbitrate a
       C-vs-stepper shaped mismatch. Shared is read back from the C
       RAM (head agreement is proven by the cdispatch differential;
       hardcoding it re-pinned the retired stale value once). */
    {
        uint8_t nibble = (uint8_t)(vc[3] & 0x0fu);
        uint16_t shared = (uint16_t)(((uint16_t)board_c.sound_ram[0x2524u]
                                      << 8u) |
                                     board_c.sound_ram[0x2525u]);
        uint32_t table = ((uint32_t)audio_rom[0x2676u + nibble * 4u] << 24u) |
                         ((uint32_t)audio_rom[0x2676u + nibble * 4u + 1u]
                          << 16u) |
                         ((uint32_t)audio_rom[0x2676u + nibble * 4u + 2u]
                          << 8u) |
                         audio_rom[0x2676u + nibble * 4u + 3u];
        uint16_t slide =
            (uint16_t)(shared + (uint16_t)((7u + 0x12u) & 0xffu));
        uint32_t slot_at = table + 2u * (uint32_t)slide;
        uint32_t slot_off = slot_at - UINT32_C(0x600000);
        uint16_t picker =
            (uint16_t)(((uint16_t)audio_rom[slot_off] << 8u) |
                       audio_rom[slot_off + 1u]);
        uint16_t shaped =
            (uint16_t)((((uint32_t)(vc[3] & 0xf0u) << 7u) + (uint32_t)picker) &
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

    /* Drain/head-only regions stay excluded: ring + count/cursor/
       current/lookup block + descriptors + $1524 + channel table.
       (The stepper head doesn't traffic the ring consume; $1524 and
       the channel word are head agreements proven by the cdispatch
       differential instead.) The jsr/rts stack window [0x7ECC,0x7F00)
       is excluded too: the stepper's tail jsr pushes a return address
       there (stale after rts; balance proven by the a7 assert above)
       and the C has no stack -- same rationale as the L1 movem-area
       asserts. Everything else compares, including the tail's
       SCSP[slot+0x10] and ram[0x201a+slot] words and the untouched
       voice images. */
    {
        static const uint32_t lo[] = { 0u, 0x1020u, 0x2526u, 0x4140u,
                                       0x4240u, 0x7f00u };
        static const uint32_t hi[] = { 0x1000u, 0x2500u, 0x4000u, 0x4200u,
                                       0x7eccu, VF2_SOUND_RAM_BYTES };
        size_t seg = 0u;
        for (seg = 0u; seg < 6u; ++seg) {
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

/* T3a: sample uploader, payload 0, real row 0. C runs dispatch_next
   end-to-end (drain + B0 0x3a + upload); the stepper starts at the
   uploader entry 0x1AC0 with d0 = payload. Compared: all RAM except
   drain-only regions (ring, cursor block, descriptors) and the
   jsr-free stack window. The uploader touches no SCSP. Expects 2 SR
   events (0x1AC0 prologue, 0x1B46 epilogue). Sample ROM is 4MB zeros
   on both sides (v0822: real row-0 sources reach ~0x23e226 past the
   old 2MB window; with zeros attached both sides run the full flow
   OK/OK and the RAM compare pins the control path -- slot words and
   stored longs from REAL entry structs -- while T3b pins the DATA
   path with markers). The old P0c trap contract (both refuse on the
   unmapped read) is retired: the row pointer was never unmapped,
   only misread. */
static void test_upload_first(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static vf2_sound_board board_c;
    static vf2_sound_board board_s;
    static uint8_t sample_rom[0x400000u];
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
    memcpy(board_s.sound_ram, rc, VF2_SOUND_RAM_BYTES);

    status_c = vf2_sound_board_dispatch_next(&board_c, &report);
    EXPECT_TRUE(status_c == VF2_OK);
    EXPECT_TRUE(report.handler_address == UINT32_C(0x13cc));

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
    EXPECT_TRUE(status_s == VF2_OK);
    fprintf(stderr, "up-first: steps=%u events=%u C=%d S=%d\n", done,
            (unsigned)bus.event_count, status_c, status_s);
    EXPECT_TRUE(bus.event_count == 2u);
    EXPECT_TRUE(regs.a[7] == UINT32_C(0x7f00));

    {
        static const uint32_t lo[] = { 0u, 0x1020u, 0x2526u, 0x4140u,
                                       0x4240u, 0x7f00u };
        static const uint32_t hi[] = { 0x1000u, 0x2500u, 0x4000u, 0x4200u,
                                       0x7eccu, VF2_SOUND_RAM_BYTES };
        size_t seg = 0u;
        for (seg = 0u; seg < 6u; ++seg) {
            if (memcmp(board_c.sound_ram + lo[seg],
                       board_s.sound_ram + lo[seg],
                       hi[seg] - lo[seg]) != 0) {
                fprintf(stderr, "up-first: RAM mismatch [%05x,%05x):\n",
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

/* T3b: sample uploader with MARKER data (v0822). Synthetic audio
   ROM (directory + rows + entries fully staged) and a 256-byte
   sample ROM with marker bytes; both sides run the full copy and
   every staged/copied/slot byte is compared. Three runs:
   - payload 1 (row 1, 2 records): proves dbf+1 staging (4th byte),
     dbf+1 records (2nd record), entry-indexed stored words
     (record 1 at $501C, not collapsed onto $6008), full-long
     stored differences, and marker copies at $50000+;
   - payload 5 (> max 2): clean OK, staged bytes only;
   - payload 0x7f: clean OK via the 0x1b46 epilogue (the old
     UNSUPPORTED "trap" was +4 damage), staged bytes only.
   C runs dispatch_next (ring 0xBA/payload); the stepper enters at
   0x1AC0 with d0 = payload. Same segment exclusions as T3a. */
static void poke_synth_rom(uint8_t *rom)
{
    memset(rom, 0, 0x9000u);
    /* first_count = 3 -> 4 staged bytes (dbf+1). */
    rom[0x5b26u] = 0x00u; rom[0x5b27u] = 0x03u;
    /* entry 0 @0x5b28 (its first 4 bytes double as stage markers):
       source 0x800010, length field for an exact 4-byte copy
       ((L+0x1ff)&0xffff == 3 -> L = 0xfe04), stored 0x11223344. */
    rom[0x5b28u] = 0x00u; rom[0x5b29u] = 0x80u;
    rom[0x5b2au] = 0x00u; rom[0x5b2bu] = 0x10u;
    rom[0x5b2cu] = 0x00u; rom[0x5b2du] = 0x00u;
    rom[0x5b2eu] = 0xfeu; rom[0x5b2fu] = 0x04u;
    rom[0x5b30u] = 0x11u; rom[0x5b31u] = 0x22u;
    rom[0x5b32u] = 0x33u; rom[0x5b33u] = 0x44u;
    /* entry 1 @0x5b3c: source 0x800020, exact 3-byte copy
       (L = 0xfe03), stored 0x55667788. */
    rom[0x5b3cu] = 0x00u; rom[0x5b3du] = 0x80u;
    rom[0x5b3eu] = 0x00u; rom[0x5b3fu] = 0x20u;
    rom[0x5b40u] = 0x00u; rom[0x5b41u] = 0x00u;
    rom[0x5b42u] = 0xfeu; rom[0x5b43u] = 0x03u;
    rom[0x5b44u] = 0x55u; rom[0x5b45u] = 0x66u;
    rom[0x5b46u] = 0x77u; rom[0x5b47u] = 0x88u;
    /* entry 2 @0x5b50: source 0x800030, exact 4-byte copy
       (L = 0xfe04), stored 0x99aabbcc. */
    rom[0x5b50u] = 0x00u; rom[0x5b51u] = 0x80u;
    rom[0x5b52u] = 0x00u; rom[0x5b53u] = 0x30u;
    rom[0x5b54u] = 0x00u; rom[0x5b55u] = 0x00u;
    rom[0x5b56u] = 0xfeu; rom[0x5b57u] = 0x04u;
    rom[0x5b58u] = 0x99u; rom[0x5b59u] = 0xaau;
    rom[0x5b5au] = 0xbbu; rom[0x5b5bu] = 0xccu;
    /* directory @0x8df0: max 2, rows at 0x608e00/10/20. */
    rom[0x8df0u] = 0x00u; rom[0x8df1u] = 0x02u;
    rom[0x8df2u] = 0x00u; rom[0x8df3u] = 0x60u;
    rom[0x8df4u] = 0x8eu; rom[0x8df5u] = 0x00u;
    rom[0x8df6u] = 0x00u; rom[0x8df7u] = 0x60u;
    rom[0x8df8u] = 0x8eu; rom[0x8df9u] = 0x10u;
    rom[0x8dfau] = 0x00u; rom[0x8dfbu] = 0x60u;
    rom[0x8dfcu] = 0x8eu; rom[0x8dfdu] = 0x20u;
    /* row 1 @0x8e10: count 1 -> 2 records, selectors 1, 2 (entry 0
       is never selected, so the $5000 stage area survives and the
       two stored words at $501c/$5030 prove entry indexing). */
    rom[0x8e10u] = 0x00u; rom[0x8e11u] = 0x01u;
    rom[0x8e12u] = 0x00u; rom[0x8e13u] = 0x01u;
    rom[0x8e14u] = 0x00u; rom[0x8e15u] = 0x02u;
}

static void run_upload_synth_case(
    const uint8_t *synth_rom,
    const uint8_t *sample_rom,
    size_t sample_size,
    uint8_t payload,
    const char *name,
    int pin_markers
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

    memset(&board_c, 0, sizeof(board_c));
    memset(&board_s, 0, sizeof(board_s));
    board_c.audio_rom = synth_rom;
    board_c.audio_rom_size = 0x9000u;
    board_s.audio_rom = synth_rom;
    board_s.audio_rom_size = 0x9000u;
    board_c.sample_rom = sample_rom;
    board_c.sample_rom_size = sample_size;
    board_s.sample_rom = sample_rom;
    board_s.sample_rom_size = sample_size;
    vf2_scsp_initialize(&board_c.scsp);
    vf2_scsp_initialize(&board_s.scsp);
    memset(&report, 0, sizeof(report));

    rc = board_c.sound_ram;
    rc[VF2_SOUND_COMMAND_COUNT] = 1u;
    rc[VF2_SOUND_COMMAND_CURSOR] = 0u;
    rc[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    rc[VF2_SOUND_COMMAND_RING_BASE] = 0xbau;
    rc[VF2_SOUND_COMMAND_RING_BASE + 1u] = payload;
    rc[VF2_SOUND_COMMAND_DESCRIPTOR_BASE] = 0x80u;
    rc[VF2_SOUND_COMMAND_DESCRIPTOR_BASE + 1u] = 0x00u;
    rc[VF2_SOUND_COMMAND_DESCRIPTOR_BASE + 2u] = 0x0au;
    memcpy(board_s.sound_ram, rc, VF2_SOUND_RAM_BYTES);

    status_c = vf2_sound_board_dispatch_next(&board_c, &report);
    EXPECT_TRUE(status_c == VF2_OK);
    EXPECT_TRUE(report.handler_address == UINT32_C(0x13cc));

    bus.ram = board_s.sound_ram;
    bus.ram_size = sizeof(board_s.sound_ram);
    bus.rom = synth_rom;
    bus.rom_size = 0x9000u;
    bus.sample_rom = sample_rom;
    bus.sample_rom_size = sample_size;
    bus.scsp = &board_s.scsp;
    bus.event_count = 0u;
    vf2_m68k_regs_reset(&regs);
    regs.d[0] = payload;
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x1ac0);
    status_s = vf2_m68k_run(&bus, &regs, UINT32_C(0xffffff), 1, 20000u,
                            &done);
    EXPECT_TRUE(status_s == VF2_OK);
    fprintf(stderr, "up-synth %s: steps=%u events=%u C=%d S=%d\n", name,
            done, (unsigned)bus.event_count, status_c, status_s);
    EXPECT_TRUE(bus.event_count == 2u);
    EXPECT_TRUE(regs.a[7] == UINT32_C(0x7f00));

    if (pin_markers) {
        /* Execution-verified marker contract (row1, entries 1,2):
           stage area survives (entry 0 never selected), slot
           targets/stored at the indexed $5014/$501c + $5028/$5030,
           marker copies at $50000/$50003. */
        const uint8_t *cram = board_c.sound_ram;
        EXPECT_TRUE(cram[0x5000u] == 0x00u && cram[0x5001u] == 0x80u &&
                    cram[0x5002u] == 0x00u && cram[0x5003u] == 0x10u);
        EXPECT_TRUE(cram[0x5014u] == 0x80u && cram[0x5015u] == 0x05u &&
                    cram[0x5016u] == 0x00u && cram[0x5017u] == 0x00u);
        EXPECT_TRUE(cram[0x5028u] == 0x80u && cram[0x5029u] == 0x05u &&
                    cram[0x502au] == 0x00u && cram[0x502bu] == 0x03u);
        EXPECT_TRUE(cram[0x501cu] == 0x54u && cram[0x501du] == 0xe6u &&
                    cram[0x501eu] == 0x77u && cram[0x501fu] == 0x68u);
        EXPECT_TRUE(cram[0x5030u] == 0x99u && cram[0x5031u] == 0x2au &&
                    cram[0x5032u] == 0xbbu && cram[0x5033u] == 0x9cu);
        EXPECT_TRUE(cram[0x50000u] == 0xf0u && cram[0x50001u] == 0xf1u &&
                    cram[0x50002u] == 0xf2u);
        EXPECT_TRUE(cram[0x50003u] == 0xe4u && cram[0x50004u] == 0xe5u &&
                    cram[0x50005u] == 0xe6u && cram[0x50006u] == 0xe7u);
    }

    {
        static const uint32_t lo[] = { 0u, 0x1020u, 0x2526u, 0x4140u,
                                       0x4240u, 0x7f00u };
        static const uint32_t hi[] = { 0x1000u, 0x2500u, 0x4000u, 0x4200u,
                                       0x7eccu, VF2_SOUND_RAM_BYTES };
        size_t seg = 0u;
        for (seg = 0u; seg < 6u; ++seg) {
            if (memcmp(board_c.sound_ram + lo[seg],
                       board_s.sound_ram + lo[seg],
                       hi[seg] - lo[seg]) != 0) {
                fprintf(stderr, "up-synth %s: RAM mismatch [%05x,%05x):\n",
                        name, lo[seg], hi[seg]);
                report_ram_diff(board_c.sound_ram + lo[seg],
                                board_s.sound_ram + lo[seg],
                                hi[seg] - lo[seg]);
                EXPECT_TRUE(0);
            }
        }
    }
    EXPECT_TRUE(scsp_equal(&board_c.scsp, &board_s.scsp));
}

static void test_upload_synth(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static uint8_t synth_rom[0x9000u];
    static uint8_t sample_rom[0x100u];

    /* The uploader BODY is real code: copy it from the real image
       (position-independent: all tables come via a0/a1). The
       directory/rows/entries stay synthetic-staged above. */
    poke_synth_rom(synth_rom);
    if (audio_rom_size < 0x1b50u) {
        fprintf(stderr, "up-synth: real ROM too small for code copy\n");
        EXPECT_TRUE(0);
        return;
    }
    memcpy(synth_rom + 0x1ac0u, audio_rom + 0x1ac0u,
           0x1b50u - 0x1ac0u);
    memset(sample_rom, 0, sizeof(sample_rom));
    sample_rom[0x10u] = 0xe0u; sample_rom[0x11u] = 0xe1u;
    sample_rom[0x12u] = 0xe2u; sample_rom[0x13u] = 0xe3u;
    sample_rom[0x20u] = 0xf0u; sample_rom[0x21u] = 0xf1u;
    sample_rom[0x22u] = 0xf2u;
    sample_rom[0x30u] = 0xe4u; sample_rom[0x31u] = 0xe5u;
    sample_rom[0x32u] = 0xe6u; sample_rom[0x33u] = 0xe7u;

    /* Payload 1: two records via entries 1,2 (entry 0 never selected:
       the $5000 stage area survives). C-side pins below (execution-
       verified values); the stepper memcmp is the proof. */
    run_upload_synth_case(synth_rom, sample_rom, sizeof(sample_rom),
                          1u, "row1", 1);
    /* Payload 5 > max 2: staged bytes only. */
    run_upload_synth_case(synth_rom, sample_rom, sizeof(sample_rom),
                          5u, "overmax", 0);
    /* Payload 0x7f: epilogue exit, staged bytes only. */
    run_upload_synth_case(synth_rom, sample_rom, sizeof(sample_rom),
                          0x7fu, "trap7f", 0);
}

/* T4: boot init attaches on the REAL ROM (v0822 payoff). Before
   the uploader re-recovery this failed: the row pointer 0x10060
   fell below the audio base (v0819 note). The true boot row is
   picked indexed (row 0 = 0x608dfa, real records), so with a 4MB
   sample window every read maps and init returns OK -- the session
   no longer goes silent-at-boot for this reason. C-only leg (no
   stepper): the upload inside is the T3a-proven flow with
   identical real bytes; this pins the attach outcome. */
static void test_init_real(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static vf2_sound_board board;
    static uint8_t sample_rom[0x400000u];
    vf2_status status = VF2_OK;

    memset(sample_rom, 0, sizeof(sample_rom));
    memset(&board, 0, sizeof(board));
    status = vf2_sound_board_initialize(&board, audio_rom, audio_rom_size,
                                        sample_rom, sizeof(sample_rom));
    fprintf(stderr, "init-real: status=%d $1526=%02x $5000=%02x%02x "
            "$50000=%02x v0slot=%02x%02x $151e=%02x $152e=%02x\n",
            (int)status, (unsigned)board.sound_ram[0x2526u],
            (unsigned)board.sound_ram[0x5000u],
            (unsigned)board.sound_ram[0x5001u],
            (unsigned)board.sound_ram[0x50000u],
            (unsigned)board.sound_ram[0x2800u],
            (unsigned)board.sound_ram[0x2801u],
            (unsigned)board.sound_ram[VF2_SOUND_SHARED_COUNTER_151E],
            (unsigned)board.sound_ram[VF2_SOUND_SHARED_COUNTER_152E]);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(board.sound_ram[0x2526u] == 0xefu);
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
    test_upload_synth(audio_rom, audio_rom_size);
    test_init_real(audio_rom, audio_rom_size);
    free(audio_rom);
    if (failures != 0) {
        fprintf(stderr, "triangle: %d failures\n", failures);
    }
    return failures == 0 ? 0 : 1;
}
