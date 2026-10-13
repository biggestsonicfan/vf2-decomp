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
 * (byte-verified 4e 75). That default rts contradicts the C
 * dispatch_next default arm (->dispatch_90): reported, C untouched
 * pending the consumer mapping (S7b).
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
    free(audio_rom);
    if (failures != 0) {
        fprintf(stderr, "cdispatch: %d failures\n", failures);
    }
    return failures == 0 ? 0 : 1;
}
