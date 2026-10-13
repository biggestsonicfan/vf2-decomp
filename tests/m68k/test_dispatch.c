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

/* True-dispatch map re-pinned under the +2 PC-relative base (see
 * audio_pcrel_base_reaudit.md + audio_integration_pcrel_v0816.md).
 * The +4-era flag-coupling/overlap story is retracted: every live
 * entry now lands on HEAD 0x2242 (fresh flags) or drain-HEAD 0x7a6
 * (fresh (a4) load), and drain exits flow THROUGH the $0408 write
 * at 0x265c (live). The 0x265e ori.b is mid-instruction dead bytes;
 * the prescaler (TRUE and bodge) ends in rts at 0x2240, so the
 * L2->walk link is gone and boot (0x21e -> HEAD) is the only known
 * walk driver. Records live at absolute $2000, L2 slots at $3000
 * (v0792); same $20 stride, different bases.
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
    regs->a[0] = UINT32_C(0x2000);
    regs->a[2] = UINT32_C(0x3000);
    regs->a[3] = UINT32_C(0x7000);
    regs->a[4] = UINT32_C(0x7200);
}

/* Leg 1: inactive record (all zero) walks HEAD -> drain-HEAD ->
 * epilogue-write and loops on itself: byte $16 == 0 takes beq to
 * the init-lea, so a0 is re-parked, never advanced. 10 steps.
 * The $0408 write logs one bus event (value 0). */
static void test_inactive_self_loop(
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
    regs.pc = UINT32_C(0x2242);
    /* Poison the SCSP $408 cell: the epilogue write must overwrite
       it, proving the write executed (a zero-write over zero RAM
       would be vacuous). $0408(a5) is SCSP+$408, not a system port:
       no bus event is logged. */
    EXPECT_TRUE(vf2_scsp_write_u8(&scsp, 0x408u, 0xaau) == VF2_OK);
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0xffffff), 0, 10u,
                          &done);
    fprintf(stderr, "dispatch self-loop: status=%d pc=0x%05x steps=%u a0=0x%05x ev=%u\n",
            (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)regs.a[0], (unsigned)bus.event_count);
    EXPECT_TRUE(status == VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(done == 10u);
    EXPECT_TRUE(regs.pc == UINT32_C(0x2242));
    EXPECT_TRUE(regs.a[0] == UINT32_C(0x2000));
    EXPECT_TRUE(bus.event_count == 0u);
    {
        uint8_t cell = 0xffu;
        EXPECT_TRUE(vf2_scsp_read_u8(&scsp, 0x408u, &cell) == VF2_OK);
        EXPECT_TRUE(cell == 0u);
    }
}

/* Leg 2: epilogue advance. Enter 0x265c with record byte $16 != 0:
 * write logs the byte, beq not taken, a0 += $20, jmp to HEAD. */
static void test_epilogue_advance(
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
    regs.pc = UINT32_C(0x265c);
    ram[0x2016u] = 0x07u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x2242), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch epi-advance: status=%d pc=0x%05x steps=%u a0=0x%05x ev=%u\n",
            (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)regs.a[0], (unsigned)bus.event_count);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x2242));
    EXPECT_TRUE(done == 4u);
    EXPECT_TRUE(regs.a[0] == UINT32_C(0x2020));
    EXPECT_TRUE(bus.event_count == 0u);
    {
        uint8_t cell = 0xffu;
        EXPECT_TRUE(vf2_scsp_read_u8(&scsp, 0x408u, &cell) == VF2_OK);
        EXPECT_TRUE(cell == 0x07u);
    }
}

/* Leg 3: two-record circuit. Record 1 ($16=$07) advances to record
 * 2 ($2020, zero) which re-inits to record 1. 20 steps, two $0408
 * events ($07 then $00), a0 back at $2000. */
static void test_two_record_circuit(
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
    regs.pc = UINT32_C(0x2242);
    ram[0x2016u] = 0x07u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0xffffff), 0, 20u,
                          &done);
    fprintf(stderr, "dispatch two-record: status=%d pc=0x%05x steps=%u a0=0x%05x ev=%u\n",
            (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)regs.a[0], (unsigned)bus.event_count);
    EXPECT_TRUE(status == VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(done == 20u);
    EXPECT_TRUE(regs.pc == UINT32_C(0x2242));
    EXPECT_TRUE(regs.a[0] == UINT32_C(0x2000));
    EXPECT_TRUE(bus.event_count == 0u);
    {
        /* Record 1 wrote $07, record 2 overwrote $00: both writes
           in the counted 20-step chain executed. */
        uint8_t cell = 0xffu;
        EXPECT_TRUE(vf2_scsp_read_u8(&scsp, 0x408u, &cell) == VF2_OK);
        EXPECT_TRUE(cell == 0u);
    }
}

/* Leg 4: TRUE prescaler entry, counter clear -> rts (NOT the walk:
 * 0x21be beq.w lands 0x2240 under +2). 2 steps. */
static void test_prescaler_returns(
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
    regs.pc = UINT32_C(0x21ba);
    ram[0x300cu] = 0u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x2240), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch prescaler: status=%d pc=0x%05x steps=%u\n",
            (int)status, (unsigned)regs.pc, (unsigned)done);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x2240));
    EXPECT_TRUE(done == 2u);
}

/* Leg 5: TRUE prescaler entry, countdown expiry ($300d 2->1) ->
 * bne.w -> rts at 0x2240. 4 steps. (Slot window is abs $3000.) */
static void test_prescaler_countdown(
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
    regs.pc = UINT32_C(0x21ba);
    ram[0x300cu] = 0x05u;
    ram[0x300du] = 0x02u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x2240), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch countdown: status=%d pc=0x%05x steps=%u d=$300d=%02x\n",
            (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)ram[0x300du]);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x2240));
    EXPECT_TRUE(done == 4u);
    EXPECT_TRUE(ram[0x300du] == 0x01u);
}

/* Leg 6: active record ($80, byte1 0, $16=$07) runs the full arm
 * to the epilogue advance: bmi -> $1a/$1c add (zero) -> bpl ->
 * byte1 zero -> 10x ror pad -> $0409 screen (model zero, taken) ->
 * $0408 write ($07) -> beq not taken -> advance arm reached.
 * Pins the $224c-entry active path end to end (21 steps). */
static void test_active_to_advance(
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
    uint8_t cell = 0xffu;

    memset(ram, 0, sizeof(ram));
    vf2_scsp_initialize(&scsp);
    make_bus(&bus, &scsp, ram, audio_rom, audio_rom_size);
    reset_regs(&regs);
    regs.pc = UINT32_C(0x2242);
    ram[0x2000u] = 0x80u;
    ram[0x2001u] = 0u;
    ram[0x2016u] = 0x07u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x2666), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch active-advance: status=%d pc=0x%05x steps=%u a0=0x%05x\n",
            (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)regs.a[0]);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x2666));
    EXPECT_TRUE(done == 21u);
    EXPECT_TRUE(regs.a[0] == UINT32_C(0x2000));
    EXPECT_TRUE(regs.d[0] == 0u);
    EXPECT_TRUE(regs.d[2] == 0u);
    EXPECT_TRUE(vf2_scsp_read_u8(&scsp, 0x408u, &cell) == VF2_OK);
    EXPECT_TRUE(cell == 0x07u);
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
    test_inactive_self_loop(audio_rom, audio_rom_size);
    test_epilogue_advance(audio_rom, audio_rom_size);
    test_two_record_circuit(audio_rom, audio_rom_size);
    test_prescaler_returns(audio_rom, audio_rom_size);
    test_prescaler_countdown(audio_rom, audio_rom_size);
    test_active_to_advance(audio_rom, audio_rom_size);
    free(audio_rom);
    if (failures != 0) {
        fprintf(stderr, "dispatch: %d failures\n", failures);
    }
    return failures == 0 ? 0 : 1;
}
