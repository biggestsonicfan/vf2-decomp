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

/* L3 MIDI ring-fill path proof (see audio_midi_0409_v0813.md).
 *
 * L3 (vector 27 -> 0x526) files SCSP-MIDI bytes into the $1500 ring
 * ($1500 data, $1501-$1503 state, $1504 cursor, $1506 count — shared
 * with the L2 gate). There is no C model (maintain_streams is a
 * no-op), so these are stepper-only path proofs in the F4/F5 style:
 * real ROM, pinned steps, balanced stack, exact RAM effects.
 * MIDI bytes are staged through the modeled SCSP FIFO.
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
    /* a3/a2 are preempted-uncleaned on silicon (no L3 path sets
       them before pushing); the fixture parks them on clean RAM
       scratch so pushes are observable. Silicon behavior depends
       on the preempted values: open. */
    regs->a[3] = UINT32_C(0x7000);
    regs->a[2] = UINT32_C(0x7100);
    regs->pc = UINT32_C(0x526);
}

/* Leg 1: 0xFF byte takes the reset path (beq.w $5ee): push d0,
   clear SCSP $401, reset SP to $5000, bra.w into the vector image
   as code ($102), fault at $10c. The ISR frame is abandoned (no
   restore, no rte): a7 ends at $5000, not entry. */
static void test_ff_ignore(
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
    EXPECT_TRUE(vf2_scsp_midi_receive(&scsp, 0xffu) == VF2_OK);
    reset_regs(&regs);
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0xffffff), 1, 20000u,
                          &done);
    fprintf(stderr, "midi ff-ignore: status=%d steps=%u a7=0x%04x\n",
            (int)status, (unsigned)done, (unsigned)regs.a[7]);
    EXPECT_TRUE(status == VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(regs.pc == UINT32_C(0x10c));
    EXPECT_TRUE(done == 10u);
    /* Stack reset abandoned the ISR frame. */
    EXPECT_TRUE(regs.a[7] == UINT32_C(0x5000));
    /* Two pushes landed (0x5ee and the vector-data coincidence at
       0x102, same bytes): d0 stayed 0xFF throughout (the linear
       0x5f8 addq print is phantom, mid-lea). */
    EXPECT_TRUE(regs.a[3] == UINT32_C(0x6ff8));
    EXPECT_TRUE(ram[0x6ffcu] == 0u);
    EXPECT_TRUE(ram[0x6ffdu] == 0u);
    EXPECT_TRUE(ram[0x6ffeu] == 0u);
    EXPECT_TRUE(ram[0x6fffu] == 0xffu);
    EXPECT_TRUE(ram[0x6ff8u] == 0u);
    EXPECT_TRUE(ram[0x6ff9u] == 0u);
    EXPECT_TRUE(ram[0x6ffau] == 0u);
    EXPECT_TRUE(ram[0x6ffbu] == 0xffu);
    /* MIDI ring ($2500 area) untouched on this path. */
    EXPECT_TRUE(ram[0x2500u] == 0u);
    EXPECT_TRUE(ram[0x2501u] == 0u);
    EXPECT_TRUE(ram[0x2504u] == 0u && ram[0x2505u] == 0u);
    EXPECT_TRUE(ram[0x2506u] == 0u);
    /* The $400001 system-port signal (0x1f) is the one bus event. */
    EXPECT_TRUE(bus.event_count == 1u);
    EXPECT_TRUE(bus.events[0].address == UINT32_C(0x400001));
    EXPECT_TRUE(bus.events[0].value == UINT32_C(0x1f));
    EXPECT_TRUE(bus.events[0].is_write != 0u);
    EXPECT_TRUE(vf2_scsp_midi_empty(&scsp));
}

/* Leg 2: data byte with running-status state ($1503 != 0) files to
   $1501(a6) = absolute $2501, then exits via restore + rte. The
   0x5e4 push uses preempted a2 (fixture scratch $7100). */
static void test_data_file(
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
    ram[0x2503u] = 1u;
    EXPECT_TRUE(vf2_scsp_midi_receive(&scsp, 0x05u) == VF2_OK);
    reset_regs(&regs);
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0xffffff), 1, 20000u,
                          &done);
    fprintf(stderr, "midi data-file: status=%d steps=%u a7=0x%04x\n",
            (int)status, (unsigned)done, (unsigned)regs.a[7]);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x5ea));
    EXPECT_TRUE(done == 16u);
    EXPECT_TRUE(regs.a[7] == UINT32_C(0x7f00));
    EXPECT_TRUE(bus.event_count == 0u);
    EXPECT_TRUE(ram[0x2501u] == 0x05u);
    EXPECT_TRUE(ram[0x2504u] == 0u && ram[0x2505u] == 0u);
    EXPECT_TRUE(ram[0x2506u] == 0u);
    /* The exit push stored entry d3 (0) at the scratch top. */
    EXPECT_TRUE(ram[0x70ffu] == 0u);
    /* Save mask $fff8 vs restore mask $1fff: the restore shifts
       every slot by 3 (D0<-D3 .. A2<-A5, A3<-A6, A4<-A7). Entry
       values were d*=0, a2=$7100, a3=$7000, a5=$100000,
       a6=$1000, a7=$7f00; A5-A7 are not in the restore mask. */
    EXPECT_TRUE(regs.d[0] == 0u && regs.d[1] == 0u);
    EXPECT_TRUE(regs.d[2] == 0u && regs.d[3] == 0u);
    EXPECT_TRUE(regs.d[4] == 0u && regs.d[5] == 0u);
    EXPECT_TRUE(regs.d[6] == 0u);
    EXPECT_TRUE(regs.d[7] == UINT32_C(0x7100));
    EXPECT_TRUE(regs.a[0] == UINT32_C(0x7000));
    EXPECT_TRUE(regs.a[1] == 0u);
    EXPECT_TRUE(regs.a[2] == UINT32_C(0x100000));
    EXPECT_TRUE(regs.a[3] == UINT32_C(0x1000));
    EXPECT_TRUE(regs.a[4] == UINT32_C(0x7f00));
    EXPECT_TRUE(regs.a[5] == UINT32_C(0x100000));
    EXPECT_TRUE(regs.a[6] == UINT32_C(0x1000));
    EXPECT_TRUE(vf2_scsp_midi_empty(&scsp));
}

/* Wrap-fix control: with a3 = 0 the 0x5ee push underflows to
   $fffffffc. Pre-fix this misread as an SCSP access (wrong code
   INVALID_ARGUMENT); with uint64 range checks it refuses
   UNSUPPORTED (silicon bus-error class) at the same pc/steps. */
static void test_push_unmapped(
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
    EXPECT_TRUE(vf2_scsp_midi_receive(&scsp, 0xffu) == VF2_OK);
    reset_regs(&regs);
    regs.a[3] = 0u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0xffffff), 1, 20000u,
                          &done);
    fprintf(stderr, "midi push-unmapped: status=%d pc=0x%05x steps=%u\n",
            (int)status, (unsigned)regs.pc, (unsigned)done);
    EXPECT_TRUE(status == VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(regs.pc == UINT32_C(0x5ee));
    EXPECT_TRUE(done == 4u);
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
    test_ff_ignore(audio_rom, audio_rom_size);
    test_data_file(audio_rom, audio_rom_size);
    test_push_unmapped(audio_rom, audio_rom_size);
    free(audio_rom);
    if (failures != 0) {
        fprintf(stderr, "midi: %d failures\n", failures);
    }
    return failures == 0 ? 0 : 1;
}
