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

/* Slice-B oracle save/resume (see audio_silicon_audit follow-up).
 *
 * Fixture is the triangle L1 s1-clean leg (voice 0 alive, timer 40):
 * ROM entry 0x5fe, a5 = 0x100000 (SCSP), a6 = 0x1000 (RAM base),
 * a7 = 0x7f00 (stack). The run must complete OK; the snapshot
 * layer must round-trip it bit-exactly and report real differences.
 */

static void poke_s1(uint8_t *ram)
{
    uint8_t *v = ram + VF2_SOUND_VOICE_TABLE_BASE;
    memset(ram, 0, VF2_SOUND_RAM_BYTES);
    v[2] = 0x01u;
    v[9] = 7u;
    v[0x0bu] = 40u;
    ram[VF2_SOUND_SHARED_COUNTER_151E] = 0x10u;
    ram[VF2_SOUND_SHARED_COUNTER_152E] = 0x08u;
}

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
    regs->pc = UINT32_C(0x5fe);
}

int main(int argc, char **argv)
{
    static uint8_t ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp scsp;
    uint8_t *audio_rom = NULL;
    size_t audio_rom_size = 0u;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_m68k_snapshot pre;
    vf2_m68k_snapshot post;
    vf2_m68k_snapshot replay;
    vf2_m68k_snapshot tampered;
    uint32_t done = 0u;
    uint32_t done2 = 0u;
    vf2_status status = VF2_OK;
    vf2_status status2 = VF2_OK;

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

    vf2_m68k_snapshot_init(&pre);
    vf2_m68k_snapshot_init(&post);
    vf2_m68k_snapshot_init(&replay);
    vf2_m68k_snapshot_init(&tampered);

    poke_s1(ram);
    vf2_scsp_initialize(&scsp);
    make_bus(&bus, &scsp, ram, audio_rom, audio_rom_size);
    reset_regs(&regs);

    /* Argument validation fails closed. */
    EXPECT_TRUE(vf2_m68k_snapshot_capture(NULL, &bus, &regs) ==
                VF2_ERROR_INVALID_ARGUMENT);
    EXPECT_TRUE(vf2_m68k_snapshot_capture(&pre, NULL, &regs) ==
                VF2_ERROR_INVALID_ARGUMENT);
    EXPECT_TRUE(vf2_m68k_snapshot_restore(NULL, &bus, &regs) ==
                VF2_ERROR_INVALID_ARGUMENT);
    EXPECT_TRUE(vf2_m68k_snapshot_equal(NULL, &pre) == 0);
    EXPECT_TRUE(vf2_m68k_snapshot_equal(&pre, NULL) == 0);

    EXPECT_TRUE(vf2_m68k_snapshot_capture(&pre, &bus, &regs) == VF2_OK);

    status = vf2_m68k_run(&bus, &regs, UINT32_C(0xffffff), 1, 20000u, &done);
    EXPECT_TRUE(status == VF2_OK);
    /* 138 steps under the +2 PC-relative base (was 200 with +4);
       the L1 voice ISR now walks all 32 voices instead of the
       frozen-voice loop (see audio_pcrel_base_reaudit.md). The
       oracle mechanism itself is unchanged: clean rte, balanced
       stack, no bus events. Voice-scan C re-recovery is separate. */
    EXPECT_TRUE(done == 138u);
    EXPECT_TRUE(regs.a[7] == UINT32_C(0x7f00));
    EXPECT_TRUE(bus.event_count == 0u);
    fprintf(stderr, "snapshot s1: steps=%u events=%u\n", done,
            (unsigned)bus.event_count);

    EXPECT_TRUE(vf2_m68k_snapshot_capture(&post, &bus, &regs) == VF2_OK);
    /* The run changed state: the comparator must see it. */
    EXPECT_TRUE(vf2_m68k_snapshot_equal(&pre, &post) == 0);

    /* Restore the pre-run state and replay: identical outcome. */
    EXPECT_TRUE(vf2_m68k_snapshot_restore(&pre, &bus, &regs) == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x5fe));
    status2 = vf2_m68k_run(&bus, &regs, UINT32_C(0xffffff), 1, 20000u,
                           &done2);
    EXPECT_TRUE(status2 == status);
    EXPECT_TRUE(done2 == done);
    EXPECT_TRUE(vf2_m68k_snapshot_capture(&replay, &bus, &regs) == VF2_OK);
    EXPECT_TRUE(vf2_m68k_snapshot_equal(&post, &replay) == 1);

    /* Negative control: one flipped RAM byte must read as different. */
    EXPECT_TRUE(vf2_m68k_snapshot_restore(&post, &bus, &regs) == VF2_OK);
    ram[VF2_SOUND_VOICE_TABLE_BASE + 0x0bu] ^= 0xffu;
    EXPECT_TRUE(vf2_m68k_snapshot_capture(&tampered, &bus, &regs) == VF2_OK);
    EXPECT_TRUE(vf2_m68k_snapshot_equal(&post, &tampered) == 0);

    /* Fixture mismatch fails closed: different RAM size on restore. */
    {
        static uint8_t small_ram[16];
        vf2_m68k_bus small_bus;
        vf2_m68k_regs small_regs;
        memset(small_ram, 0, sizeof(small_ram));
        make_bus(&small_bus, &scsp, small_ram, audio_rom, audio_rom_size);
        small_bus.ram_size = sizeof(small_ram);
        vf2_m68k_regs_reset(&small_regs);
        EXPECT_TRUE(vf2_m68k_snapshot_restore(&pre, &small_bus,
                                              &small_regs) ==
                    VF2_ERROR_INVALID_ARGUMENT);
    }

    vf2_m68k_snapshot_destroy(&pre);
    vf2_m68k_snapshot_destroy(&post);
    vf2_m68k_snapshot_destroy(&replay);
    vf2_m68k_snapshot_destroy(&tampered);
    free(audio_rom);
    if (failures != 0) {
        fprintf(stderr, "snapshot: %d failures\n", failures);
    }
    return failures == 0 ? 0 : 1;
}
