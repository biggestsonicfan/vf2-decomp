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

/* R3a: twin-loop fault. byte1=$05 (d2 != $80 -> direct worker),
 * d5=$300 > $200 (bulk arm): subi/addi/store-backs, legal movem,
 * then the structural R->M movem to (a2) refuses. 21 steps. */
static void test_twin_loop_fault(
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
    ram[0x2000u] = 0x80u;
    ram[0x2001u] = 0x05u;
    ram[0x200au] = 0u;
    ram[0x200bu] = 0u;
    ram[0x200cu] = 0x03u;
    ram[0x200du] = 0u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0xffffff), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch twin-fault: status=%d pc=0x%05x steps=%u\n",
            (int)status, (unsigned)regs.pc, (unsigned)done);
    EXPECT_TRUE(status == VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(regs.pc == UINT32_C(0x22e8));
    EXPECT_TRUE(done == 21u);
    /* Writebacks landed before the fault: d2 += $10, d5 -= $200. */
    EXPECT_TRUE(ram[0x2001u] == 0x15u);
    EXPECT_TRUE(ram[0x200au] == 0u && ram[0x200bu] == 0u);
    EXPECT_TRUE(ram[0x200cu] == 0x01u && ram[0x200du] == 0u);
}

/* R3b: arith-body fault. byte0=$81 (bmi taken, btst#0 set),
 * byte1=$80 (pad path), d5=$100 (bls into arith): full arithmetic
 * body runs, writebacks land, then the same structural movem
 * refuses at 0x23fa. */
static void test_arith_fault(
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
    ram[0x2000u] = 0x81u;
    ram[0x2001u] = 0x80u;
    ram[0x200au] = 0u;
    ram[0x200bu] = 0u;
    ram[0x200cu] = 0x01u;
    ram[0x200du] = 0u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0xffffff), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch arith-fault: status=%d pc=0x%05x steps=%u d2=0x%08x\n",
            (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)regs.d[2]);
    EXPECT_TRUE(status == VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(regs.pc == UINT32_C(0x23fa));
    EXPECT_TRUE(done == 44u);
    /* d2 += $10 through the body. */
    EXPECT_TRUE(ram[0x2001u] == 0x90u);
}

/* R3c-i: walk-screen SET arm. $0409 bit7 set (SCSP poke): the pad
 * path takes the set arm into the worker (0x2286, bypassing the
 * a2 load) instead of exiting. With a zeroed record it reaches the
 * arith join; byte0=$80 bit0-clear sends it to the 0x24be twin
 * join. Pins the set-arm routing (previously unmeasured). */
static void test_screen1_set(
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
    ram[0x2000u] = 0x80u;
    ram[0x2001u] = 0u;
    EXPECT_TRUE(vf2_scsp_write_u8(&scsp, 0x409u, 0x80u) == VF2_OK);
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x24be), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch screen1-set: status=%d pc=0x%05x steps=%u\n",
            (int)status, (unsigned)regs.pc, (unsigned)done);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x24be));
    EXPECT_TRUE(done == 27u);
}

/* R3c-ii: desc-screen SET arm. byte1=$80 pad path with $0409 bit7
 * set: exits early to the epilogue-write (21 steps), skipping the
 * worker. The clear case instead falls into the worker (R3b). */
static void test_screen2_set(
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
    ram[0x2000u] = 0x80u;
    ram[0x2001u] = 0x80u;
    EXPECT_TRUE(vf2_scsp_write_u8(&scsp, 0x409u, 0x80u) == VF2_OK);
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x265c), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch screen2-set: status=%d pc=0x%05x steps=%u\n",
            (int)status, (unsigned)regs.pc, (unsigned)done);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x265c));
    EXPECT_TRUE(done == 21u);
}

/* R3d: drain-HEAD push path end to end. Count 1 at $2506 (a4
 * parked there as boot leaves it): HEAD loads nonzero -> push D7
 * frame -> decrement (cell 1->0) -> cursor/ring walk -> $4000-window
 * scan, 20 iterations, no match (zeroed) -> ADDQ.W cursor (the
 * former coverage gap, now implemented) -> re-park a4 -> restore
 * A0 from saved D7 (0) -> epilogue-write. 99 steps. (0x7fc is the
 * match-merge path, never executed without a match.) */
static void test_drain_push_path(
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
    regs.pc = UINT32_C(0x7a6);
    regs.a[4] = UINT32_C(0x2506);
    ram[0x2506u] = 1u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x265c), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch drain-push: status=%d pc=0x%05x steps=%u a0=0x%05x a7=0x%04x\n",
            (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)regs.a[0], (unsigned)regs.a[7]);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x265c));
    EXPECT_TRUE(done == 99u);
    /* Push/restore balanced (no-match path pushes once, pops once). */
    EXPECT_TRUE(regs.a[7] == UINT32_C(0x7f00));
    /* Countdown consumed, cursor advanced 0->4, a4 re-parked. */
    EXPECT_TRUE(ram[0x2506u] == 0u);
    EXPECT_TRUE(regs.a[4] == UINT32_C(0x2506));
    EXPECT_TRUE(ram[0x2508u] == 0u && ram[0x2509u] == 0x04u);
    /* Restore A0 from saved entry D7 (0): the epilogue reads $16
       of record 0 next, not the walk record. */
    EXPECT_TRUE(regs.a[0] == 0u);
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
    test_twin_loop_fault(audio_rom, audio_rom_size);
    test_arith_fault(audio_rom, audio_rom_size);
    test_screen1_set(audio_rom, audio_rom_size);
    test_screen2_set(audio_rom, audio_rom_size);
    test_drain_push_path(audio_rom, audio_rom_size);
    free(audio_rom);
    if (failures != 0) {
        fprintf(stderr, "dispatch: %d failures\n", failures);
    }
    return failures == 0 ? 0 : 1;
}
