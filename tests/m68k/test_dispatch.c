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

/* True-dispatch hunt for the record walk (see audio_dispatch_v0814.md).
 *
 * Byte-verified map (ROM bytes checked, not just prints):
 *  - walk-HEAD 0x2242 (move.b (a0),d0) has exactly two callers:
 *    prescaler 0x21be (beq.w) and 0x21c6 (bne.w). TRUE entry.
 *  - walk-MID 0x2244 (bmi) has three: boot 0x21e (jmp), epilogue
 *    0x266a/0x2672 (jmp). The scanner's "0x2233 br8" is a phantom
 *    (mid-andi.b byte 0x6E).
 *  - TRUE prescaler entry 0x21ba (tst.b $c(a2)) has NO referrers
 *    (no jsr, no vector, no fall-through: 0x21b8 rts). Only the
 *    mid-instruction bodge 0x21bc is live (L2 jsr from 0x1f36).
 *  - epilogue init-lea 0x266e is dead: 0x265e ori.b #$8,(a6) can
 *    never set Z, so beq $2670 never takes; the loop only advances
 *    (a0 += $20) and re-enters at MID with preempted a0.
 *  - boot sets a0 = $1000(a6) (record base) at 0x1d4 and jumps to
 *    MID at 0x21e: the TRUE first dispatch. Slots $2000..$20e0 and
 *    records $1000(a6) stride $20 are the same window (a6=$1000).
 *
 * These are stepper-only path proofs: real ROM, pinned steps, exact
 * register/RAM effects. a2 parks on the slot ($2000), a3/a4 on clean
 * scratch; silicon preemption dependence is stated open.
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
    regs->a[2] = UINT32_C(0x2000);
    regs->a[3] = UINT32_C(0x7000);
    regs->a[4] = UINT32_C(0x7200);
}

/* Leg 1: inactive record at HEAD -> drain-mid -> epilogue.
 * 0x2242 move.b / 0x2244 bmi(nt) / 0x2248 jmp / 0x7a8 bne(nt, flags
 * from walk's zero load) / 0x7ac jmp. 5 steps, a0 untouched. */
static void test_inactive_to_epilogue(
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
    ram[0x2000u] = 0u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x265e), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch inactive-epi: status=%d pc=0x%05x steps=%u\n",
            (int)status, (unsigned)regs.pc, (unsigned)done);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x265e));
    EXPECT_TRUE(done == 5u);
    EXPECT_TRUE(regs.a[0] == UINT32_C(0x2000));
    EXPECT_TRUE(regs.d[0] == 0u);
}

/* Leg 2: epilogue advances a0 by one $20 stride and re-enters MID.
 * 0x265e ori.b / 0x2662 beq.w(nt: OR-with-$8 never zero) /
 * 0x2666 adda / 0x266a jmp. 4 steps. */
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
    regs.pc = UINT32_C(0x265e);
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x2244), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch epi-advance: status=%d pc=0x%05x steps=%u a0=0x%05x\n",
            (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)regs.a[0]);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x2244));
    EXPECT_TRUE(done == 4u);
    EXPECT_TRUE(regs.a[0] == UINT32_C(0x2020));
    /* Epilogue sets bit3 of the (a6) header byte. */
    EXPECT_TRUE((ram[0x1000u] & 0x08u) != 0u);
}

/* Leg 3: full inactive loop back at MID with a0 advanced. Proves the
 * 9-step circuit without stop-at-start (exhaust run). */
static void test_inactive_full_loop(
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
    ram[0x2000u] = 0u;
    ram[0x2020u] = 0u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0xffffff), 0, 9u,
                          &done);
    fprintf(stderr, "dispatch full-loop: status=%d pc=0x%05x steps=%u a0=0x%05x\n",
            (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)regs.a[0]);
    EXPECT_TRUE(status == VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(done == 9u);
    EXPECT_TRUE(regs.pc == UINT32_C(0x2244));
    EXPECT_TRUE(regs.a[0] == UINT32_C(0x2020));
}

/* Leg 4: TRUE prescaler entry, counter clear -> HEAD via beq.
 * 0x21ba tst.b ($200c zero) / 0x21be beq.w(t). 2 steps. */
static void test_prescaler_true_entry(
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
    ram[0x200cu] = 0u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x2242), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch prescaler: status=%d pc=0x%05x steps=%u\n",
            (int)status, (unsigned)regs.pc, (unsigned)done);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x2242));
    EXPECT_TRUE(done == 2u);
}

/* Leg 5: TRUE prescaler entry, countdown expiry -> HEAD via bne.
 * $200c != 0 (beq nt), $200d = 2 -> 1 (Z=0, bne t). 4 steps. */
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
    ram[0x200cu] = 0x05u;
    ram[0x200du] = 0x02u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x2242), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch countdown: status=%d pc=0x%05x steps=%u d=$200d=%02x\n",
            (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)ram[0x200du]);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x2242));
    EXPECT_TRUE(done == 4u);
    EXPECT_TRUE(ram[0x200du] == 0x01u);
}

/* Leg 6: second record takes the stale-Z overlap arm and faults.
 * Record 1 (zero) walks HEAD-fresh to the epilogue; record 2 enters
 * at MID with the epilogue ori.b's Z=0, so drain's bne takes the
 * 0x7b2 arm: mid-movem bytes run as ori.l #$531447ee,d0 then
 * ori.b #$2e,d0, and 0x7bc (move.b-An, silicon-illegal) refuses.
 * The movem push is skipped: a7 stays balanced. 14 steps. */
static void test_record2_overlap_fault(
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
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0xffffff), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch record2-fault: status=%d pc=0x%05x steps=%u d0=0x%08x a7=0x%04x\n",
            (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)regs.d[0], (unsigned)regs.a[7]);
    EXPECT_TRUE(status == VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(regs.pc == UINT32_C(0x7bc));
    EXPECT_TRUE(done == 14u);
    EXPECT_TRUE(regs.d[0] == UINT32_C(0x531447ee));
    EXPECT_TRUE(regs.a[7] == UINT32_C(0x7f00));
    EXPECT_TRUE(regs.a[0] == UINT32_C(0x2020));
}

/* Leg 7: active record 1 files through to the epilogue.
 * bmi taken -> 0x224e arm (linear chain HIT-verified to 0x2264) ->
 * byte1 zero -> ror pad -> $0409-beq (model zero) -> epilogue.
 * Pins pc/steps/a0; d0 residue printed for the record. */
static void test_active_record_to_epilogue(
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
    regs.a[2] = UINT32_C(0x7300);
    ram[0x2000u] = 0x80u;
    ram[0x2001u] = 0u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x265e), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch active-epi: status=%d pc=0x%05x steps=%u d0=0x%08x d2=0x%08x\n",
            (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)regs.d[0], (unsigned)regs.d[2]);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x265e));
    EXPECT_TRUE(done == 21u);
    EXPECT_TRUE(regs.a[0] == UINT32_C(0x2000));
    /* d0 = $80 << 2 (0x225e lsl.w); d2 = record byte1 (zero). */
    EXPECT_TRUE(regs.d[0] == UINT32_C(0x200));
    EXPECT_TRUE(regs.d[2] == 0u);
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
    test_inactive_to_epilogue(audio_rom, audio_rom_size);
    test_epilogue_advance(audio_rom, audio_rom_size);
    test_inactive_full_loop(audio_rom, audio_rom_size);
    test_prescaler_true_entry(audio_rom, audio_rom_size);
    test_prescaler_countdown(audio_rom, audio_rom_size);
    test_record2_overlap_fault(audio_rom, audio_rom_size);
    test_active_record_to_epilogue(audio_rom, audio_rom_size);
    free(audio_rom);
    if (failures != 0) {
        fprintf(stderr, "dispatch: %d failures\n", failures);
    }
    return failures == 0 ? 0 : 1;
}
