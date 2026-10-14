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

/* S7a: drain class-switch routing + e0-head path (see
 * audio_switch_audit_v0819.md). The ROM switch reads the filed
 * $150e byte and jumps: $90->$858, $80->$10d0, $e0->$12b4,
 * $b0->$13ca, $c0->$1e5a, $a0->$1e74, anything else->rts at $856
 * (byte-verified 4e 75). S2b retired the C's default->90 mapping:
 * unlisted classes ignore (handler 0x856) with drain consume.
 */

static void make_bus(
    vf2_m68k_bus *bus,
    vf2_scsp *scsp,
    uint8_t *ram,
    const uint8_t *rom,
    size_t rom_size
)
{
    bus->ram = ram;
    bus->ram_size = VF2_SOUND_RAM_BYTES;
    bus->rom = rom;
    bus->rom_size = rom_size;
    bus->sample_rom = NULL;
    bus->sample_rom_size = 0u;
    bus->scsp = scsp;
    bus->event_count = 0u;
}

static void reset_regs(vf2_m68k_regs *regs)
{
    vf2_m68k_regs_reset(regs);
    regs->a[5] = UINT32_C(0x100000);
    regs->a[6] = UINT32_C(0x1000);
    regs->a[7] = UINT32_C(0x7f00);
    regs->pc = UINT32_C(0x81a);
}

static int scsp_equal(const vf2_scsp *a, const vf2_scsp *b)
{
    size_t i = 0u;
    if (memcmp(a->registers, b->registers, sizeof(a->registers)) != 0) {
        return 0;
    }
    for (i = 0u; i < VF2_SCSP_SLOT_COUNT; ++i) {
        if (a->slot_address[i] != b->slot_address[i] ||
            a->slot_step[i] != b->slot_step[i] ||
            a->slot_envelope[i] != b->slot_envelope[i] ||
            a->slot_active[i] != b->slot_active[i] ||
            a->slot_envelope_state[i] != b->slot_envelope_state[i] ||
            a->slot_backward[i] != b->slot_backward[i]) {
            return 0;
        }
    }
    return (memcmp(a->midi_input, b->midi_input, sizeof(a->midi_input)) ==
            0 &&
            a->midi_read_index == b->midi_read_index &&
            a->midi_write_index == b->midi_write_index);
}

/* Switch routing: filed class -> arm entry. Step counts predicted
   from the linear cmpi/beq chain (2 steps per rejected class). */
static void test_switch_routing(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static const struct {
        uint8_t filed;
        uint32_t target;
        uint32_t steps;
    } legs[] = {
        { 0x90u, 0x858u, 4u },
        { 0x80u, 0x10d0u, 6u },
        { 0xe0u, 0x12b4u, 8u },
        { 0xb0u, 0x13cau, 10u },
        { 0xc0u, 0x1e5au, 12u },
        { 0xa0u, 0x1e74u, 15u },
        { 0x00u, 0x856u, 14u },
    };
    size_t i = 0u;
    for (i = 0u; i < sizeof(legs) / sizeof(legs[0]); ++i) {
        static uint8_t ram[VF2_SOUND_RAM_BYTES];
        static vf2_scsp scsp;
        vf2_m68k_bus bus;
        vf2_m68k_regs regs;
        uint32_t done = 0u;
        vf2_status status = VF2_OK;
        memset(ram, 0, sizeof(ram));
        vf2_scsp_initialize(&scsp);
        make_bus(&bus, &scsp, ram, audio_rom, audio_rom_size);
        reset_regs(&regs);
        ram[0x250eu] = legs[i].filed;
        status = vf2_m68k_run(&bus, &regs, legs[i].target, 0, 20000u,
                              &done);
        fprintf(stderr, "dispatch switch-%02x: status=%d pc=0x%05x steps=%u\n",
                (unsigned)legs[i].filed, (int)status,
                (unsigned)regs.pc, (unsigned)done);
        EXPECT_TRUE(status == VF2_OK);
        EXPECT_TRUE(regs.pc == legs[i].target);
        EXPECT_TRUE(done == legs[i].steps);
    }
}

/* e0 full head, channel 3, voices miss-all: stream d2/d0 prep,
   neg.w (transformed-bit8 clear), $1524 + $4206 stores, 32-voice
   scan with no match, rts at 0x1354. 164 steps. d1 ends 3 (filed
   channel), d2 ends $e9 (neg.b ran). */
static void test_e0_head_nomatch(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static uint8_t ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp scsp;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;

    memset(ram, 0, sizeof(ram));
    vf2_scsp_initialize(&scsp);
    make_bus(&bus, &scsp, ram, audio_rom, audio_rom_size);
    reset_regs(&regs);
    regs.pc = UINT32_C(0x12b4);
    regs.a[3] = UINT32_C(0x7300);
    regs.a[0] = UINT32_C(0x7400);
    ram[0x7300u] = 0xe3u;
    ram[0x7301u] = 0x05u;
    ram[0x7407u] = 0x00u;
    ram[0x250eu] = 0xe3u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x1354), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch e0-nomatch: status=%d pc=0x%05x steps=%u\n",
            (int)status, (unsigned)regs.pc, (unsigned)done);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x1354));
    EXPECT_TRUE(done == 164u);
    EXPECT_TRUE(regs.d[1] == 3u);
    EXPECT_TRUE(regs.d[2] == UINT32_C(0xe9));
    EXPECT_TRUE(ram[0x2524u] == 0xffu && ram[0x2525u] == 0xf2u);
    EXPECT_TRUE(ram[0x4206u] == 0xffu && ram[0x4207u] == 0xf2u);
    EXPECT_TRUE(bus.event_count == 0u);
}

/* e0 gate, channel >= 9: bcc exits via rts at 0x13c8 (NOT the
   computed jump — the C's "falls into the B0 primer" gloss is
   +4-shaped; the early-return behavior it models still stands). */
static void test_e0_gate_rts(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static uint8_t ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp scsp;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;

    memset(ram, 0, sizeof(ram));
    vf2_scsp_initialize(&scsp);
    make_bus(&bus, &scsp, ram, audio_rom, audio_rom_size);
    reset_regs(&regs);
    regs.pc = UINT32_C(0x12b4);
    regs.a[3] = UINT32_C(0x7300);
    regs.a[0] = UINT32_C(0x7400);
    ram[0x7300u] = 0xe9u;
    ram[0x7301u] = 0x05u;
    ram[0x7407u] = 0x00u;
    ram[0x250eu] = 0xe9u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x13c8), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch e0-gate: status=%d pc=0x%05x steps=%u\n",
            (int)status, (unsigned)regs.pc, (unsigned)done);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x13c8));
    EXPECT_TRUE(done == 34u);
}

/* A0 primer, payload above row max: bgt exits via rts at 0x1eac.
   Row 0 max ($608e12[0] -> $6000128f) is $0003 in this ROM
   (measured, not assumed); payload $04 trips it. The C refuses
   the whole A0 path (stale-a2 join); this clean exit arm is a
   future micro-admission candidate (not taken here). */
static void test_a0_overmax(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static uint8_t ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp scsp;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    uint32_t rowptr = 0u;
    uint32_t rowoff = 0u;
    uint16_t maxv = 0u;
    uint8_t cmd = 0xa0u;
    uint8_t idx = (uint8_t)(cmd & 0x7fu);

    /* The row is indexed by command&$7f (0x1e7c), not row 0. */
    EXPECT_TRUE((uint32_t)0x8e12u + (uint32_t)idx * 4u + 4u <=
                audio_rom_size);
    rowptr = ((uint32_t)audio_rom[0x8e12u + (uint32_t)idx * 4u] << 24u) |
             ((uint32_t)audio_rom[0x8e13u + (uint32_t)idx * 4u] << 16u) |
             ((uint32_t)audio_rom[0x8e14u + (uint32_t)idx * 4u] << 8u) |
             (uint32_t)audio_rom[0x8e15u + (uint32_t)idx * 4u];
    rowoff = rowptr - UINT32_C(0x600000);

    EXPECT_TRUE(rowoff + 2u <= audio_rom_size);
    maxv = (uint16_t)(((uint16_t)audio_rom[rowoff] << 8u) |
                      audio_rom[rowoff + 1u]);
    fprintf(stderr, "dispatch a0-row: idx=0x%02x max=0x%04x\n",
            (unsigned)idx, (unsigned)maxv);
    EXPECT_TRUE(maxv != UINT32_C(0xffff));
    memset(ram, 0, sizeof(ram));
    vf2_scsp_initialize(&scsp);
    make_bus(&bus, &scsp, ram, audio_rom, audio_rom_size);
    reset_regs(&regs);
    regs.pc = UINT32_C(0x1e74);
    regs.a[3] = UINT32_C(0x7300);
    ram[0x7300u] = cmd;
    ram[0x7301u] = (uint8_t)(((uint32_t)maxv + 1u) & 0xffu);
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x1eac), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch a0-overmax: status=%d pc=0x%05x steps=%u\n",
            (int)status, (unsigned)regs.pc, (unsigned)done);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x1eac));
    EXPECT_TRUE(done == 10u);
    EXPECT_TRUE(regs.d[2] == (uint32_t)maxv);
}

/* S7b differential: C dispatch_next vs ROM e0 head+tail.
 *
 * Shared contract (asserted): $1524 word, $4200+2*ch word, voice
 * timer RAM[$201a+d4], SCSP voice word [$10+d4], report channel /
 * voices_matched / handler.
 * Excluded by construction (documented, NOT asserted): ring
 * cursor/count consume (C advances, ROM head doesn't traffic them),
 * $150e current-command write (C writes, ROM head only reads),
 * stepper regs / C scan counters (side-local).
 * Fixture: ring cmd 0xe3 payload 0x05, filed $150e=$e3, descriptor
 * $4000 (ch 3, table 0), voice0 match (slot 0), rest miss-all.
 * Stepper a3 stream mirrors ring[0..1] (controlled input). */
static void test_e0_differential(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_sound_board board;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report creport;
    uint8_t *cram = NULL;
    uint8_t *voice0 = NULL;
    uint8_t *desc = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    vf2_status cstatus = VF2_OK;
    uint16_t c_1524 = 0u;
    uint16_t s_1524 = 0u;
    uint16_t c_chw = 0u;
    uint16_t s_chw = 0u;
    uint16_t c_timer = 0u;
    uint16_t s_timer = 0u;

    /* NOTE (v0822): vf2_sound_board_initialize now attaches on the
       real 0x80000 audio region (T4 proves OK); the old failure was
       the uploader's misread row pointer. Bare-init SCSPs are still
       used here to keep both sides' start states identical without
       depending on boot. */
    memset(&board, 0, sizeof(board));
    board.audio_rom = audio_rom;
    board.audio_rom_size = audio_rom_size;
    vf2_scsp_initialize(&board.scsp);
    cram = board.sound_ram;
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    memset(step_ram, 0, sizeof(step_ram));
    vf2_scsp_initialize(&step_scsp);

    /* Ring: cursor 0, count 1, cmd e3 payload 05. */
    cram[VF2_SOUND_COMMAND_RING_BASE] = 0xe3u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 1u] = 0x05u;
    cram[VF2_SOUND_COMMAND_COUNT] = 1u;
    cram[VF2_SOUND_CURRENT_COMMAND] = 0xe3u;
    /* Descriptor $4000: active, byte-match channel 3 (S2a: the
       drain matches descriptor[1] == command&0xf), table 0. */
    desc = cram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    desc[0] = 0x80u;
    desc[1] = 0x03u;
    desc[2] = 0x03u;
    desc[3] = 0x00u;
    desc[5] = 0x02u;
    desc[7] = 0x00u;
    /* Voice0 $2800: slot 0, active, nibble $30, channel byte 3
       at [4] (ROM cmp.B matches the single byte; voice[5]
       ignored). */
    voice0 = cram + VF2_SOUND_VOICE_TABLE_BASE;
    voice0[0] = 0x00u;
    voice0[1] = 0x00u;
    voice0[2] = 0x01u;
    voice0[3] = 0x30u;
    voice0[4] = 0x03u;
    voice0[5] = 0x00u;
    voice0[10] = 0x00u;
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);

    memset(&creport, 0, sizeof(creport));
    cstatus = vf2_sound_board_dispatch_next(&board, &creport);
    fprintf(stderr, "dispatch e0-diff: c-status=%d ch=%u matched=%u handler=0x%05x\n",
            (int)cstatus, (unsigned)creport.channel,
            (unsigned)creport.voices_matched,
            (unsigned)creport.handler_address);
    EXPECT_TRUE(cstatus == VF2_OK);
    EXPECT_TRUE(creport.channel == 3u);
    EXPECT_TRUE(creport.voices_matched == 1u);
    EXPECT_TRUE(creport.handler_address == UINT32_C(0x12b6));

    make_bus(&bus, &step_scsp, step_ram, audio_rom, audio_rom_size);
    reset_regs(&regs);
    regs.pc = UINT32_C(0x12b4);
    regs.a[3] = UINT32_C(0x7300);
    regs.a[0] = UINT32_C(0x4000);
    /* Primer-correct stream (S2a): the drain leaves a3 at ring+1,
       so the primer reads payload first, then ring byte 2 --
       NOT {command, payload}. */
    step_ram[0x7300u] = 0x05u;
    step_ram[0x7301u] = 0x00u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x1354), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch e0-diff: s-status=%d pc=0x%05x steps=%u\n",
            (int)status, (unsigned)regs.pc, (unsigned)done);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x1354));

    c_1524 = (uint16_t)(((uint16_t)cram[0x2524u] << 8u) | cram[0x2525u]);
    s_1524 = (uint16_t)(((uint16_t)step_ram[0x2524u] << 8u) | step_ram[0x2525u]);
    c_chw = (uint16_t)(((uint16_t)cram[0x4206u] << 8u) | cram[0x4207u]);
    s_chw = (uint16_t)(((uint16_t)step_ram[0x4206u] << 8u) | step_ram[0x4207u]);
    c_timer = (uint16_t)(((uint16_t)cram[0x201au] << 8u) | cram[0x201bu]);
    s_timer = (uint16_t)(((uint16_t)step_ram[0x201au] << 8u) | step_ram[0x201bu]);
    fprintf(stderr, "dispatch e0-diff: 1524 c=%04x s=%04x chw c=%04x s=%04x timer c=%04x s=%04x\n",
            (unsigned)c_1524, (unsigned)s_1524,
            (unsigned)c_chw, (unsigned)s_chw,
            (unsigned)c_timer, (unsigned)s_timer);
    EXPECT_TRUE(c_1524 == s_1524);
    EXPECT_TRUE(c_chw == s_chw);
    EXPECT_TRUE(c_timer == s_timer);
    EXPECT_TRUE(board.scsp.registers[0x10u] == step_scsp.registers[0x10u]);
    EXPECT_TRUE(board.scsp.registers[0x11u] == step_scsp.registers[0x11u]);
}

/* S2a: drain-entry e0 differential -- the byte-match proof.
 *
 * The fixture discriminates byte vs word routing: desc[1] = 0x04
 * (byte-MATCHES channel 4), desc[2] = 0x00 (word be16 = 0x0400
 * MISMATCHES). Command 0xE4. Under the old word-match C the ring
 * is consumed with no handler; under byte-match (silicon: 0x7e6
 * `cmp.b $0001(a0),d0` after the 0x7d2 `andi.b #$f`) both sides
 * run the e0 head+tail through the real drain entry.
 * C: dispatch_next (ring E4/04, count 1, CURRENT pre-zeroed --
 * the drain files it). Stepper: drain entry 0x7a6 (a4 = $1506
 * count, standard a5/a6/a7; a3/a0/d regs set by the drain
 * itself), run to the tail exit 0x1354. Compared: the T1 segment
 * set + SCSP. Drain-only regions stay excluded (ring consume,
 * count/cursor, $150e file, stack window with the switch-jsr and
 * tail-jsr residue); a7 is unbalanced mid-call by construction
 * (drain movem + switch jsr + tail jsr outstanding) -- no assert.
 * Voice0 byte-matches ch 4 (V1 fixture shape). */
static void test_drain_e0_entry(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_sound_board board;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report creport;
    uint8_t *cram = NULL;
    uint8_t *voice0 = NULL;
    uint8_t *desc = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    vf2_status cstatus = VF2_OK;

    memset(&board, 0, sizeof(board));
    board.audio_rom = audio_rom;
    board.audio_rom_size = audio_rom_size;
    vf2_scsp_initialize(&board.scsp);
    cram = board.sound_ram;
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    memset(step_ram, 0, sizeof(step_ram));
    vf2_scsp_initialize(&step_scsp);

    cram[VF2_SOUND_COMMAND_RING_BASE] = 0xe4u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 1u] = 4u;
    cram[VF2_SOUND_COMMAND_COUNT] = 1u;
    cram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    cram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    desc = cram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    desc[0] = 0x80u;
    desc[1] = 0x04u;
    desc[2] = 0x00u;
    desc[3] = 0x00u;
    desc[5] = 0x12u;
    desc[6] = 0x01u;
    desc[7] = 0x00u;
    voice0 = cram + VF2_SOUND_VOICE_TABLE_BASE;
    voice0[0] = 0x00u;
    voice0[1] = 0x20u;
    voice0[2] = 0x01u;
    voice0[3] = 0x3au;
    voice0[4] = 0x04u;
    voice0[5] = 0x04u;
    voice0[10] = 0x07u;
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);

    memset(&creport, 0, sizeof(creport));
    cstatus = vf2_sound_board_dispatch_next(&board, &creport);
    fprintf(stderr, "dispatch drain-e0: c-status=%d ch=%u matched=%u handler=0x%05x\n",
            (int)cstatus, (unsigned)creport.channel,
            (unsigned)creport.voices_matched,
            (unsigned)creport.handler_address);
    EXPECT_TRUE(cstatus == VF2_OK);
    EXPECT_TRUE(creport.channel == 4u);
    EXPECT_TRUE(creport.voices_matched == 1u);
    EXPECT_TRUE(creport.handler_address == UINT32_C(0x12b6));

    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = audio_rom;
    bus.rom_size = audio_rom_size;
    bus.sample_rom = NULL;
    bus.sample_rom_size = 0u;
    bus.scsp = &step_scsp;
    bus.event_count = 0u;
    vf2_m68k_regs_reset(&regs);
    regs.a[4] = UINT32_C(0x2506);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x7a6);
    step_ram[VF2_SOUND_COMMAND_COUNT] = 1u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x1354), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch drain-e0: s-status=%d pc=0x%05x steps=%u events=%u\n",
            (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)bus.event_count);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x1354));
    EXPECT_TRUE(bus.event_count == 0u);

    {
        static const uint32_t lo[] = { 0u, 0x1020u, 0x2526u, 0x4140u,
                                       0x4240u, 0x7f00u };
        static const uint32_t hi[] = { 0x1000u, 0x2500u, 0x4000u, 0x4200u,
                                       0x7eccu, VF2_SOUND_RAM_BYTES };
        size_t seg = 0u;
        for (seg = 0u; seg < 6u; ++seg) {
            size_t i = 0u;
            for (i = 0u; i < hi[seg] - lo[seg]; ++i) {
                if (board.sound_ram[lo[seg] + i] !=
                    step_ram[lo[seg] + i]) {
                    fprintf(stderr, "dispatch drain-e0: RAM mismatch [%05x,%05x) +0x%x: C=0x%02x S=0x%02x\n",
                            lo[seg], hi[seg], (unsigned)i,
                            (unsigned)board.sound_ram[lo[seg] + i],
                            (unsigned)step_ram[lo[seg] + i]);
                    EXPECT_TRUE(0);
                    break;
                }
            }
        }
    }
    EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
}

/* S2b: drain-entry 90-head differential (primer proof).
 *
 * Fixture: ring {0x93, 0x05, byte2}, desc[1] = 3 (byte-match),
 * desc[2] = 0xf5 (HIGH) or 0x03 (LOW), desc[4] = 0x10. Under the
 * old d2=command C, base would be 0xa3 and mode ROM[..+5]; under
 * the primer truth (d2 = payload, second = ring byte 2) base is
 * 0x15 and mode is ROM[0x60c6a6+2] (real ROM bytes -- both sides
 * share audio_rom, so no staging; the assert is C==S equality).
 * C: dispatch_next refuses at the 0x878 split (S2c owns both
 * arms), packet held. Stepper: drain 0x7a6 to the split target
 * (0x884 HIGH / 0xa3e LOW -- the step delta + target pin the split
 * direction on silicon; the C claims neither arm).
 * Compared: $150f/$1510 C==S, $1511 still 0 both sides, SCSP
 * equal (head touches none), zero bus events. Drain-only state
 * (count subq'd at entry, $150e file, stack residue) is excluded
 * by construction -- the head path is 8 linear instructions past
 * the switch, all accounted (base file, second peek+branch,
 * mode-table file). */
static void test_drain_90_head(
    const uint8_t *audio_rom,
    size_t audio_rom_size,
    uint8_t desc2,
    uint32_t stop_pc,
    const char *tag
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_sound_board board;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report creport;
    uint8_t *cram = NULL;
    uint8_t *desc = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    vf2_status cstatus = VF2_OK;

    memset(&board, 0, sizeof(board));
    board.audio_rom = audio_rom;
    board.audio_rom_size = audio_rom_size;
    vf2_scsp_initialize(&board.scsp);
    cram = board.sound_ram;
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    memset(step_ram, 0, sizeof(step_ram));
    vf2_scsp_initialize(&step_scsp);

    cram[VF2_SOUND_COMMAND_RING_BASE] = 0x93u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 1u] = 5u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 2u] = 2u;
    cram[VF2_SOUND_COMMAND_COUNT] = 1u;
    cram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    cram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    desc = cram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    desc[0] = 0x80u;
    desc[1] = 0x03u;
    desc[2] = desc2;
    desc[3] = 0x00u;
    desc[4] = 0x10u;
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);

    memset(&creport, 0, sizeof(creport));
    cstatus = vf2_sound_board_dispatch_next(&board, &creport);
    fprintf(stderr, "dispatch drain-90-%s: c-status=%d ch=%u handler=0x%05x base=%02x mode=%02x count=%u\n",
            tag, (int)cstatus, (unsigned)creport.channel,
            (unsigned)creport.handler_address,
            (unsigned)cram[VF2_SOUND_LOOKUP_CURRENT],
            (unsigned)cram[VF2_SOUND_LOOKUP_MODE],
            (unsigned)cram[VF2_SOUND_COMMAND_COUNT]);
    EXPECT_TRUE(cstatus == VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(creport.channel == 3u);
    EXPECT_TRUE(creport.handler_address == UINT32_C(0x858));
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_CURRENT] == 0x15u);
    EXPECT_TRUE(cram[VF2_SOUND_COMMAND_COUNT] == 1u);
    EXPECT_TRUE(creport.cursor_after == creport.cursor_before);

    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = audio_rom;
    bus.rom_size = audio_rom_size;
    bus.sample_rom = NULL;
    bus.sample_rom_size = 0u;
    bus.scsp = &step_scsp;
    bus.event_count = 0u;
    vf2_m68k_regs_reset(&regs);
    regs.a[4] = UINT32_C(0x2506);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x7a6);
    step_ram[VF2_SOUND_COMMAND_COUNT] = 1u;
    status = vf2_m68k_run(&bus, &regs, stop_pc, 0, 20000u, &done);
    fprintf(stderr, "dispatch drain-90-%s: s-status=%d pc=0x%05x steps=%u events=%u base=%02x mode=%02x\n",
            tag, (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)bus.event_count,
            (unsigned)step_ram[VF2_SOUND_LOOKUP_CURRENT],
            (unsigned)step_ram[VF2_SOUND_LOOKUP_MODE]);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == stop_pc);
    EXPECT_TRUE(done == 36u);
    EXPECT_TRUE(bus.event_count == 0u);
    EXPECT_TRUE(step_ram[VF2_SOUND_LOOKUP_CURRENT] == 0x15u);
    EXPECT_TRUE(step_ram[0x2511u] == 0u);
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_CURRENT] ==
                step_ram[VF2_SOUND_LOOKUP_CURRENT]);
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_MODE] ==
                step_ram[VF2_SOUND_LOOKUP_MODE]);
    EXPECT_TRUE(cram[0x2511u] == step_ram[0x2511u]);
    EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
}

/* S2b: 90 zero-second trap. Ring {0x93, 0x05, 0x00}: the head
 * files base 0x15, then 0x866/0x868 beq-takes into the 80 body
 * mid-instruction (0x10e2) -- trap on HW. C refuses with mode
 * untouched, packet held. Stepper stop: 0x10e2. */
static void test_drain_90_trap(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_sound_board board;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report creport;
    uint8_t *cram = NULL;
    uint8_t *desc = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    vf2_status cstatus = VF2_OK;

    memset(&board, 0, sizeof(board));
    board.audio_rom = audio_rom;
    board.audio_rom_size = audio_rom_size;
    vf2_scsp_initialize(&board.scsp);
    cram = board.sound_ram;
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    memset(step_ram, 0, sizeof(step_ram));
    vf2_scsp_initialize(&step_scsp);

    cram[VF2_SOUND_COMMAND_RING_BASE] = 0x93u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 1u] = 5u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 2u] = 0u;
    cram[VF2_SOUND_COMMAND_COUNT] = 1u;
    cram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    cram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    desc = cram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    desc[0] = 0x80u;
    desc[1] = 0x03u;
    desc[2] = 0xf5u;
    desc[4] = 0x10u;
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);

    memset(&creport, 0, sizeof(creport));
    cstatus = vf2_sound_board_dispatch_next(&board, &creport);
    fprintf(stderr, "dispatch drain-90-trap: c-status=%d handler=0x%05x base=%02x mode=%02x count=%u\n",
            (int)cstatus, (unsigned)creport.handler_address,
            (unsigned)cram[VF2_SOUND_LOOKUP_CURRENT],
            (unsigned)cram[VF2_SOUND_LOOKUP_MODE],
            (unsigned)cram[VF2_SOUND_COMMAND_COUNT]);
    EXPECT_TRUE(cstatus == VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(creport.handler_address == UINT32_C(0x858));
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_CURRENT] == 0x15u);
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_MODE] == 0u);
    EXPECT_TRUE(cram[VF2_SOUND_COMMAND_COUNT] == 1u);

    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = audio_rom;
    bus.rom_size = audio_rom_size;
    bus.sample_rom = NULL;
    bus.sample_rom_size = 0u;
    bus.scsp = &step_scsp;
    bus.event_count = 0u;
    vf2_m68k_regs_reset(&regs);
    regs.a[4] = UINT32_C(0x2506);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x7a6);
    step_ram[VF2_SOUND_COMMAND_COUNT] = 1u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x10e2), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch drain-90-trap: s-status=%d pc=0x%05x steps=%u events=%u base=%02x mode=%02x\n",
            (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)bus.event_count,
            (unsigned)step_ram[VF2_SOUND_LOOKUP_CURRENT],
            (unsigned)step_ram[VF2_SOUND_LOOKUP_MODE]);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x10e2));
    EXPECT_TRUE(done == 31u);
    EXPECT_TRUE(bus.event_count == 0u);
    EXPECT_TRUE(step_ram[VF2_SOUND_LOOKUP_CURRENT] == 0x15u);
    EXPECT_TRUE(step_ram[VF2_SOUND_LOOKUP_MODE] == 0u);
    EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
}

/* S2b: drain-entry 80-head differential. Ring {0x81, 0x06, x},
 * desc[1] = 1, desc[4] = 0x10: base = (0x06&0x7f)+0x10 = 0x16
 * (byte-add; desc[5] never participates). Old word-add C gave
 * (0x01)+be16(desc+4); old command-head gave 0x01+... -- both
 * dead. C refuses at the 0x10e2 split, packet held. Stepper stop:
 * 0x10e2 (the split load, pre-branch). */
static void test_drain_80_head(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_sound_board board;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report creport;
    uint8_t *cram = NULL;
    uint8_t *desc = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    vf2_status cstatus = VF2_OK;

    memset(&board, 0, sizeof(board));
    board.audio_rom = audio_rom;
    board.audio_rom_size = audio_rom_size;
    vf2_scsp_initialize(&board.scsp);
    cram = board.sound_ram;
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    memset(step_ram, 0, sizeof(step_ram));
    vf2_scsp_initialize(&step_scsp);

    cram[VF2_SOUND_COMMAND_RING_BASE] = 0x81u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 1u] = 6u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 2u] = 0u;
    cram[VF2_SOUND_COMMAND_COUNT] = 1u;
    cram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    cram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    desc = cram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    desc[0] = 0x80u;
    desc[1] = 0x01u;
    desc[2] = 0x03u;
    desc[4] = 0x10u;
    desc[5] = 0x20u;
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);

    memset(&creport, 0, sizeof(creport));
    cstatus = vf2_sound_board_dispatch_next(&board, &creport);
    fprintf(stderr, "dispatch drain-80: c-status=%d ch=%u handler=0x%05x base=%02x count=%u\n",
            (int)cstatus, (unsigned)creport.channel,
            (unsigned)creport.handler_address,
            (unsigned)cram[VF2_SOUND_LOOKUP_CURRENT],
            (unsigned)cram[VF2_SOUND_COMMAND_COUNT]);
    EXPECT_TRUE(cstatus == VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(creport.channel == 1u);
    EXPECT_TRUE(creport.handler_address == UINT32_C(0x10d0));
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_CURRENT] == 0x16u);
    EXPECT_TRUE(cram[VF2_SOUND_COMMAND_COUNT] == 1u);

    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = audio_rom;
    bus.rom_size = audio_rom_size;
    bus.sample_rom = NULL;
    bus.sample_rom_size = 0u;
    bus.scsp = &step_scsp;
    bus.event_count = 0u;
    vf2_m68k_regs_reset(&regs);
    regs.a[4] = UINT32_C(0x2506);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x7a6);
    step_ram[VF2_SOUND_COMMAND_COUNT] = 1u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x10e2), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch drain-80: s-status=%d pc=0x%05x steps=%u events=%u base=%02x\n",
            (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)bus.event_count,
            (unsigned)step_ram[VF2_SOUND_LOOKUP_CURRENT]);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x10e2));
    EXPECT_TRUE(done == 31u);
    EXPECT_TRUE(bus.event_count == 0u);
    EXPECT_TRUE(step_ram[VF2_SOUND_LOOKUP_CURRENT] == 0x16u);
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_CURRENT] ==
                step_ram[VF2_SOUND_LOOKUP_CURRENT]);
    EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
}

/* S2c: drain-entry 90-HIGH differential (Loop-B door).
 *
 * Fixture (F11 shape): ring {0x93, 0x00, 0x01}, desc[1] = 3,
 * desc[2] = 0xf5 (HIGH), desc[4] = 0, desc[6] = 0, desc[12] /
 * desc[13] per leg. Table path: a2 = be32(0x6032c6 + 5*4) +
 * (payload&0x7f)*6 on real ROM; gate word != 0x00ff (passes);
 * files $1511/$1512/$1516/$150d (+desc[13] clear); $1515 belongs
 * to Loop-B's own first block (F11: still 0 at the door).
 * C: HIGH body then refuse AT the 0x902 door (not at the split),
 * packet held. Stepper: drain 0x7a6 -> 0x902.
 * Compared: $150f/$1510/$1511/$1512/$1516/$150d + desc[12..13]
 * C==S, $1515 == 0 both sides, SCSP equal, zero events.
 * RED pre-fix: the C refuses at the 0x878 split, so $1511+
 * stay 0 while silicon files them. */
static void test_drain_90_high(
    const uint8_t *audio_rom,
    size_t audio_rom_size,
    uint8_t desc12,
    uint8_t desc13,
    const char *tag
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_sound_board board;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report creport;
    uint8_t *cram = NULL;
    uint8_t *desc = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    vf2_status cstatus = VF2_OK;

    memset(&board, 0, sizeof(board));
    board.audio_rom = audio_rom;
    board.audio_rom_size = audio_rom_size;
    vf2_scsp_initialize(&board.scsp);
    cram = board.sound_ram;
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    memset(step_ram, 0, sizeof(step_ram));
    vf2_scsp_initialize(&step_scsp);

    cram[VF2_SOUND_COMMAND_RING_BASE] = 0x93u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 1u] = 0u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 2u] = 1u;
    cram[VF2_SOUND_COMMAND_COUNT] = 1u;
    cram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    cram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    desc = cram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    desc[0] = 0x80u;
    desc[1] = 0x03u;
    desc[2] = 0xf5u;
    desc[3] = 0x00u;
    desc[4] = 0x00u;
    desc[6] = 0x00u;
    desc[12] = desc12;
    desc[13] = desc13;
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);

    memset(&creport, 0, sizeof(creport));
    cstatus = vf2_sound_board_dispatch_next(&board, &creport);
    fprintf(stderr, "dispatch drain-90h-%s: c-status=%d handler=0x%05x 11=%02x 12=%02x%02x 16=%02x%02x 0d=%02x d13=%02x count=%u\n",
            tag, (int)cstatus, (unsigned)creport.handler_address,
            (unsigned)cram[0x2511u],
            (unsigned)cram[0x2512u], (unsigned)cram[0x2513u],
            (unsigned)cram[0x2516u], (unsigned)cram[0x2517u],
            (unsigned)cram[0x250du],
            (unsigned)cram[VF2_SOUND_COMMAND_DESCRIPTOR_BASE + 13u],
            (unsigned)cram[VF2_SOUND_COMMAND_COUNT]);
    EXPECT_TRUE(cstatus == VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(creport.handler_address == UINT32_C(0x858));
    EXPECT_TRUE(creport.channel == 3u);
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_CURRENT] == 0x00u);
    EXPECT_TRUE(cram[0x2515u] == 0u);
    EXPECT_TRUE(cram[VF2_SOUND_COMMAND_COUNT] == 1u);
    EXPECT_TRUE(creport.cursor_after == creport.cursor_before);

    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = audio_rom;
    bus.rom_size = audio_rom_size;
    bus.sample_rom = NULL;
    bus.sample_rom_size = 0u;
    bus.scsp = &step_scsp;
    bus.event_count = 0u;
    vf2_m68k_regs_reset(&regs);
    regs.a[4] = UINT32_C(0x2506);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x7a6);
    step_ram[VF2_SOUND_COMMAND_COUNT] = 1u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x902), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch drain-90h-%s: s-status=%d pc=0x%05x steps=%u events=%u 11=%02x 12=%02x%02x 16=%02x%02x 0d=%02x d13=%02x\n",
            tag, (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)bus.event_count,
            (unsigned)step_ram[0x2511u],
            (unsigned)step_ram[0x2512u], (unsigned)step_ram[0x2513u],
            (unsigned)step_ram[0x2516u], (unsigned)step_ram[0x2517u],
            (unsigned)step_ram[0x250du],
            (unsigned)step_ram[VF2_SOUND_COMMAND_DESCRIPTOR_BASE + 13u]);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x902));
    /* Step-pinned $150d sub-branches (measured): desc[13]-clear
       skips the desc[12] logic (60), keep runs it (62), table+5
       adds the a2+5 fetch (63). */
    EXPECT_TRUE(done == (desc13 != 0u ? 60u :
                         (desc12 & 0x07u) != 0u ? 62u : 63u));
    EXPECT_TRUE(bus.event_count == 0u);
    EXPECT_TRUE(step_ram[0x2515u] == 0u);
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_CURRENT] ==
                step_ram[VF2_SOUND_LOOKUP_CURRENT]);
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_MODE] ==
                step_ram[VF2_SOUND_LOOKUP_MODE]);
    EXPECT_TRUE(cram[0x2511u] == step_ram[0x2511u]);
    EXPECT_TRUE(cram[0x2512u] == step_ram[0x2512u]);
    EXPECT_TRUE(cram[0x2513u] == step_ram[0x2513u]);
    EXPECT_TRUE(cram[0x2516u] == step_ram[0x2516u]);
    EXPECT_TRUE(cram[0x2517u] == step_ram[0x2517u]);
    EXPECT_TRUE(cram[0x250du] == step_ram[0x250du]);
    EXPECT_TRUE(cram[VF2_SOUND_COMMAND_DESCRIPTOR_BASE + 12u] ==
                step_ram[VF2_SOUND_COMMAND_DESCRIPTOR_BASE + 12u]);
    EXPECT_TRUE(cram[VF2_SOUND_COMMAND_DESCRIPTOR_BASE + 13u] ==
                step_ram[VF2_SOUND_COMMAND_DESCRIPTOR_BASE + 13u]);
    EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
}

/* S2c: drain-entry 90-HIGH gate-hit differential (the 0x8b0 rts).
 *
 * Ring {0x93, 0x6a, 0x01} (payload 0x6a hits a 0x00ff table word
 * in the desc-0xf5 table -- found by the committed sweep's
 * one-off run, hard-pinned here): the head files base/mode, the
 * gate takes the rts back to the drain scan, no other descriptor
 * matches, the drain consumes. C: OK, handler 0x858, head only,
 * consumed. Stepper: drain 0x7a6 -> main-loop 0x265c.
 * Compared: $150f/$1510 C==S, $1511/$1512/$1516/$150d still 0
 * both sides, SCSP equal, zero events. */
static void test_drain_90_gate(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_sound_board board;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report creport;
    uint8_t *cram = NULL;
    uint8_t *desc = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    vf2_status cstatus = VF2_OK;

    memset(&board, 0, sizeof(board));
    board.audio_rom = audio_rom;
    board.audio_rom_size = audio_rom_size;
    vf2_scsp_initialize(&board.scsp);
    cram = board.sound_ram;
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    memset(step_ram, 0, sizeof(step_ram));
    vf2_scsp_initialize(&step_scsp);

    cram[VF2_SOUND_COMMAND_RING_BASE] = 0x93u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 1u] = 0x6au;
    cram[VF2_SOUND_COMMAND_RING_BASE + 2u] = 1u;
    cram[VF2_SOUND_COMMAND_COUNT] = 1u;
    cram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    cram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    desc = cram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    desc[0] = 0x80u;
    desc[1] = 0x03u;
    desc[2] = 0xf5u;
    desc[4] = 0x00u;
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);

    memset(&creport, 0, sizeof(creport));
    cstatus = vf2_sound_board_dispatch_next(&board, &creport);
    fprintf(stderr, "dispatch drain-90g: c-status=%d handler=0x%05x base=%02x mode=%02x 11=%02x 0d=%02x count=%u cursor=%u handled=%d\n",
            (int)cstatus, (unsigned)creport.handler_address,
            (unsigned)cram[VF2_SOUND_LOOKUP_CURRENT],
            (unsigned)cram[VF2_SOUND_LOOKUP_MODE],
            (unsigned)cram[0x2511u], (unsigned)cram[0x250du],
            (unsigned)cram[VF2_SOUND_COMMAND_COUNT],
            (unsigned)creport.cursor_after, (int)creport.handled);
    EXPECT_TRUE(cstatus == VF2_OK);
    EXPECT_TRUE(creport.handler_address == UINT32_C(0x858));
    EXPECT_TRUE(creport.channel == 3u);
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_CURRENT] == 0x6au);
    EXPECT_TRUE(cram[0x2511u] == 0u);
    EXPECT_TRUE(cram[0x2512u] == 0u && cram[0x2516u] == 0u);
    EXPECT_TRUE(cram[0x250du] == 0u);
    EXPECT_TRUE(cram[VF2_SOUND_COMMAND_COUNT] == 0u);
    EXPECT_TRUE(creport.cursor_after == 4u);
    EXPECT_TRUE(creport.handled != 0);

    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = audio_rom;
    bus.rom_size = audio_rom_size;
    bus.sample_rom = NULL;
    bus.sample_rom_size = 0u;
    bus.scsp = &step_scsp;
    bus.event_count = 0u;
    vf2_m68k_regs_reset(&regs);
    regs.a[4] = UINT32_C(0x2506);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x7a6);
    step_ram[VF2_SOUND_COMMAND_COUNT] = 1u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x265c), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch drain-90g: s-status=%d pc=0x%05x steps=%u events=%u base=%02x mode=%02x 11=%02x 0d=%02x\n",
            (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)bus.event_count,
            (unsigned)step_ram[VF2_SOUND_LOOKUP_CURRENT],
            (unsigned)step_ram[VF2_SOUND_LOOKUP_MODE],
            (unsigned)step_ram[0x2511u], (unsigned)step_ram[0x250du]);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x265c));
    EXPECT_TRUE(done == 132u);
    EXPECT_TRUE(bus.event_count == 0u);
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_CURRENT] ==
                step_ram[VF2_SOUND_LOOKUP_CURRENT]);
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_MODE] ==
                step_ram[VF2_SOUND_LOOKUP_MODE]);
    EXPECT_TRUE(step_ram[0x2511u] == 0u);
    EXPECT_TRUE(step_ram[0x2512u] == 0u && step_ram[0x2516u] == 0u);
    EXPECT_TRUE(step_ram[0x250du] == 0u);
    EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
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
    fprintf(stderr, "audio region size: 0x%x\n", (unsigned)audio_rom_size);
    test_switch_routing(audio_rom, audio_rom_size);
    test_e0_head_nomatch(audio_rom, audio_rom_size);
    test_e0_gate_rts(audio_rom, audio_rom_size);
    test_a0_overmax(audio_rom, audio_rom_size);
    test_e0_differential(audio_rom, audio_rom_size);
    test_drain_e0_entry(audio_rom, audio_rom_size);
    test_drain_90_head(audio_rom, audio_rom_size, 0xf5u,
                       UINT32_C(0x884), "high");
    test_drain_90_head(audio_rom, audio_rom_size, 0x03u,
                       UINT32_C(0xa3e), "low");
    test_drain_90_trap(audio_rom, audio_rom_size);
    test_drain_80_head(audio_rom, audio_rom_size);
    test_drain_90_high(audio_rom, audio_rom_size, 0x01u, 0x00u,
                       "keep");
    test_drain_90_high(audio_rom, audio_rom_size, 0x08u, 0x00u,
                       "table5");
    test_drain_90_high(audio_rom, audio_rom_size, 0x01u, 0x07u,
                       "clear");
    test_drain_90_gate(audio_rom, audio_rom_size);
    free(audio_rom);
    if (failures != 0) {
        fprintf(stderr, "cdispatch: %d failures\n", failures);
    }
    return failures == 0 ? 0 : 1;
}
