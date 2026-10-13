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

/* L3 MIDI ring-fill path proof, re-pinned under the +2 PC-relative
 * base (see audio_pcrel_base_reaudit.md + audio_integration_pcrel
 * _v0816.md). The +4-era push story is retracted: beq.w now lands
 * 0x5ec (no push before the stack reset) and bra.w lands 0x5e2 (a
 * store, no push before the restore). The 0x5ee push
 * (move.l d0,-(a3)) has no referrers under +2: dead code, kept only
 * as a direct-entry wrap-fix control.
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
    /* a3/a2 are preempted-uncleaned on silicon (no live L3 path sets
       them); the fixture parks them on clean RAM scratch so any
       push would be observable. Silicon behavior depends on the
       preempted values: open. */
    regs->a[3] = UINT32_C(0x7000);
    regs->a[2] = UINT32_C(0x7100);
    regs->pc = UINT32_C(0x526);
}

/* Leg 1: 0xFF byte takes the reset path (beq.w $5ec): disable
   interrupts, clear SCSP $401, reset SP to $5000, bra.w into the
   vector image as code ($100), signal $400001, fault at $10c
   (clr.l d0: mapped, deferred). The ISR frame is abandoned (no
   restore, no rte): a7 ends at $5000, not entry. No pushes on this
   path: a3 and the scratch stay clean. */
static void test_ff_reset(
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
    fprintf(stderr, "midi ff-reset: status=%d steps=%u a7=0x%04x pc=0x%05x ev=%u\n",
            (int)status, (unsigned)done, (unsigned)regs.a[7],
            (unsigned)regs.pc, (unsigned)bus.event_count);
    EXPECT_TRUE(status == VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(regs.pc == UINT32_C(0x10c));
    EXPECT_TRUE(done == 10u);
    /* Stack reset abandoned the ISR frame. */
    EXPECT_TRUE(regs.a[7] == UINT32_C(0x5000));
    /* No pushes: scratch parked registers untouched, RAM clean. */
    EXPECT_TRUE(regs.a[3] == UINT32_C(0x7000));
    EXPECT_TRUE(regs.a[2] == UINT32_C(0x7100));
    EXPECT_TRUE(ram[0x6fffu] == 0u);
    EXPECT_TRUE(ram[0x6ffbu] == 0u);
    EXPECT_TRUE(regs.d[0] == UINT32_C(0xff));
    /* MIDI ring ($2500 area) untouched on this path. */
    EXPECT_TRUE(ram[0x2500u] == 0u);
    EXPECT_TRUE(ram[0x2501u] == 0u);
    EXPECT_TRUE(ram[0x2504u] == 0u && ram[0x2505u] == 0u);
    EXPECT_TRUE(ram[0x2506u] == 0u);
    /* Three bus events: two SR writes ($2700 at $5ec and $100) then
       the $400001 system-port signal (0x1f). SR-write logging
       predates the merge (2ea0e618); the +4 path never executed an
       SR write, so the old leg saw only the signal. */
    EXPECT_TRUE(bus.event_count == 3u);
    EXPECT_TRUE(bus.events[0].address == UINT32_C(0x5ec));
    EXPECT_TRUE(bus.events[0].value == UINT32_C(0x2700));
    EXPECT_TRUE(bus.events[0].is_write != 0u);
    EXPECT_TRUE(bus.events[1].address == UINT32_C(0x100));
    EXPECT_TRUE(bus.events[1].value == UINT32_C(0x2700));
    EXPECT_TRUE(bus.events[1].is_write != 0u);
    EXPECT_TRUE(bus.events[2].address == UINT32_C(0x400001));
    EXPECT_TRUE(bus.events[2].value == UINT32_C(0x1f));
    EXPECT_TRUE(bus.events[2].is_write != 0u);
    EXPECT_TRUE(vf2_scsp_midi_empty(&scsp));
}

/* Leg 2: data byte with running-status count ($1503 = 2) files to
   $1501(a6) = absolute $2501, then exits via store + restore + rte.
   Under +2, 0x54a bne.w lands 0x552 (not 0x554): the subq always
   runs, so the file arm needs a count of 2, not 1 (count 1 takes
   the 0x580 arm). No push precedes the restore: a2 stays parked
   and the scratch is untouched. */
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
    ram[0x2503u] = 2u;
    EXPECT_TRUE(vf2_scsp_midi_receive(&scsp, 0x05u) == VF2_OK);
    reset_regs(&regs);
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0xffffff), 1, 20000u,
                          &done);
    fprintf(stderr, "midi data-file: status=%d steps=%u a7=0x%04x\n",
            (int)status, (unsigned)done, (unsigned)regs.a[7]);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x5ea));
    EXPECT_TRUE(done == 17u);
    EXPECT_TRUE(regs.a[7] == UINT32_C(0x7f00));
    EXPECT_TRUE(bus.event_count == 0u);
    EXPECT_TRUE(ram[0x2501u] == 0x05u);
    EXPECT_TRUE(ram[0x2504u] == 0u && ram[0x2505u] == 0u);
    EXPECT_TRUE(ram[0x2506u] == 0u);
    /* No exit push under +2: the scratch top is clean (the restore
       shuffle still reloads a2 from the saved a5 — pinned below). */
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

/* Wrap-fix control, repurposed: the 0x5ee push is dead under +2 (no
   referrers), so enter it directly. With a3 = 0 the push underflows
   to $fffffffc. Pre-fix this misread as an SCSP access (wrong code
   INVALID_ARGUMENT); with uint64 range checks it refuses
   UNSUPPORTED (silicon bus-error class). */
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
    reset_regs(&regs);
    regs.a[3] = 0u;
    regs.pc = UINT32_C(0x5ee);
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0xffffff), 1, 20000u,
                          &done);
    fprintf(stderr, "midi push-unmapped: status=%d pc=0x%05x steps=%u\n",
            (int)status, (unsigned)regs.pc, (unsigned)done);
    EXPECT_TRUE(status == VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(regs.pc == UINT32_C(0x5ee));
    EXPECT_TRUE(done == 0u);
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
    test_ff_reset(audio_rom, audio_rom_size);
    test_data_file(audio_rom, audio_rom_size);
    test_push_unmapped(audio_rom, audio_rom_size);
    free(audio_rom);
    if (failures != 0) {
        fprintf(stderr, "midi: %d failures\n", failures);
    }
    return failures == 0 ? 0 : 1;
}
