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

/* Slice-1 Loop-B deadness probe (see decomp/i960/notes/p1b veredict).
 *
 * Loop-B (0x902) voice scan re-enters mid-instruction on every
 * iteration:
 * - dbf d7 targets 0x934, the imm word of cmpi.b #$ff,$0009(a1).
 *   0x00FF decodes as ORI.L with an invalid extended ea: illegal
 *   on a stock 68000 (the repo stepper refuses UNSUPPORTED, the
 *   ROM's own vector-4 stub runs the 0x4fe crash handler).
 * - bne targets 0x948, an ori.b #$10,d0 sled that skips the
 *   adda.l (a1 frozen, Z forced 0).
 * - beq targets 0x95c, mid-move sled territory.
 * The scan therefore cannot execute past its first dbf, and the
 * 90-HIGH arm that feeds Loop-B needs desc[2] >= 0xf0, which the
 * drain (word match desc[1..2] == channel <= 0x0f) can never
 * produce. These pins prove the C's refuse-before-Loop-B is
 * COMPLETE rather than a frontier: there is no live caller to
 * recover.
 */

static void run_loopb_case(const uint8_t *audio_rom, size_t audio_rom_size,
                           const char *name, uint32_t entry_pc,
                           uint32_t stop_pc,
                           uint8_t voice0_9, uint8_t voice0_5,
                           uint8_t reg150e, uint8_t reg152e,
                           uint32_t reg_a0, uint8_t stream0,
                           vf2_status expect_status, uint32_t expect_pc,
                           uint32_t expect_steps)
{
    static uint8_t ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp scsp;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;

    memset(ram, 0, sizeof(ram));
    vf2_scsp_initialize(&scsp);
    /* Voice 0 slot word + scan bytes; voices 1-31 stay zeroed. */
    ram[0x2800u] = 0u;
    ram[0x2801u] = 0u;
    ram[0x2800u + 9u] = voice0_9;
    ram[0x2800u + 5u] = voice0_5;
    ram[0x250eu] = reg150e;
    ram[0x252eu] = reg152e;
    /* Stream slot 0 byte 0 (walk-head fixtures). */
    ram[0x2000u] = stream0;

    bus.ram = ram;
    bus.ram_size = sizeof(ram);
    bus.rom = audio_rom;
    bus.rom_size = audio_rom_size;
    bus.sample_rom = NULL;
    bus.sample_rom_size = 0u;
    bus.scsp = &scsp;
    bus.event_count = 0u;
    vf2_m68k_regs_reset(&regs);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.a[1] = UINT32_C(0x2800);
    regs.a[0] = reg_a0;
    regs.pc = entry_pc;
    status = vf2_m68k_run(&bus, &regs, stop_pc, 0, 200u,
                          &done);
    fprintf(stderr, "loopb %s: status=%d pc=0x%05x steps=%u events=%u\n",
            name, (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)bus.event_count);
    EXPECT_TRUE(status == expect_status);
    EXPECT_TRUE(regs.pc == expect_pc);
    EXPECT_TRUE(done == expect_steps);
    EXPECT_TRUE(bus.event_count == 0u);
}

static void run_stop_case(const uint8_t *audio_rom, size_t audio_rom_size,
                          uint32_t entry_pc, uint32_t stop_pc,
                          uint32_t reg_a0, uint8_t stream0,
                          vf2_status *status_out, uint32_t *pc_out,
                          uint32_t *steps_out)
{
    static uint8_t ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp scsp;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    uint32_t done = 0u;

    memset(ram, 0, sizeof(ram));
    vf2_scsp_initialize(&scsp);
    ram[0x2000u] = stream0;
    bus.ram = ram;
    bus.ram_size = sizeof(ram);
    bus.rom = audio_rom;
    bus.rom_size = audio_rom_size;
    bus.sample_rom = NULL;
    bus.sample_rom_size = 0u;
    bus.scsp = &scsp;
    bus.event_count = 0u;
    vf2_m68k_regs_reset(&regs);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.a[0] = reg_a0;
    regs.pc = entry_pc;
    *status_out = vf2_m68k_run(&bus, &regs, stop_pc, 0, 200u, &done);
    *pc_out = regs.pc;
    *steps_out = done;
}

/* Stop-pc sweep: maps the executed prefix empirically. A hit is
   status OK + pc == stop; a miss records the trap point. */
static void sweep_path(const uint8_t *audio_rom, size_t audio_rom_size,
                       const char *name, uint32_t entry_pc, uint32_t reg_a0,
                       uint8_t stream0, const uint32_t *stops,
                       const int *expect_hit, const uint32_t *expect_steps,
                       size_t nstops)
{
    size_t i = 0u;
    fprintf(stderr, "loopb sweep %s: entry=0x%05x stream0=0x%02x\n", name,
            (unsigned)entry_pc, (unsigned)stream0);
    for (i = 0u; i < nstops; ++i) {
        vf2_status status = VF2_OK;
        uint32_t pc = 0u;
        uint32_t steps = 0u;
        int hit = 0;
        run_stop_case(audio_rom, audio_rom_size, entry_pc, stops[i], reg_a0,
                      stream0, &status, &pc, &steps);
        hit = (status == VF2_OK && pc == stops[i]) ? 1 : 0;
        fprintf(stderr, "  stop=0x%05x: %s status=%d pc=0x%05x steps=%u\n",
                (unsigned)stops[i], hit ? "HIT " : "miss",
                (int)status, (unsigned)pc, (unsigned)steps);
        EXPECT_TRUE(hit == expect_hit[i]);
        if (expect_hit[i]) {
            EXPECT_TRUE(steps == expect_steps[i]);
        }
    }
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
    /* F1: direct scan entry, voice[9]=0 (bne arm): ori sled, dbf,
       trap on the 0x00FF word. 6 insns execute. */
    run_loopb_case(audio_rom, audio_rom_size, "f1-scan-bne", 0x92au,
                   0xffffffu, 0u, 0u, 0u, 0u, 0u, 0u,
                   VF2_ERROR_UNSUPPORTED, 0x934u, 6u);
    /* F3: full Loop-B head entry ($150e&0xf=3, $152e=5): head runs
       clean (9 insns), scan traps identically. 15 insns execute. */
    run_loopb_case(audio_rom, audio_rom_size, "f3-head-to-scan", 0x902u,
                   0xffffffu, 0u, 0u, 3u, 5u, 0u, 0u,
                   VF2_ERROR_UNSUPPORTED, 0x934u, 15u);
    /* F4: walk head, zeroed flags (bmi not taken), a0 = stream slot 0:
       falls to the drain jmp $7a8. 2 insns, stops clean at the
       drain entry. (Entry is mid-head at 0x2244, as the L2 worker
       jumps there; d0/flags are fixture-zero here, so this pins
       the N=0 arm only. $7a8 is the raw-verified silicon target;
       the pre-fix print/stepper said $7a6, a -2 in the shared
       jsr/jmp-(d16,PC) base.) */
    run_loopb_case(audio_rom, audio_rom_size, "f4-walk-inactive", 0x2244u,
                   0x7a8u, 0u, 0u, 0u, 0u, 0x2000u, 0u,
                   VF2_OK, 0x7a8u, 2u);
    {
        /* F5 path-map sweep, ACTIVE stream via the head (entry 0x2242
           executes the move.b that sets d0/N from the fixture; entering
           at 0x2244 would leave fixture-zero flags and re-test the N=0
           arm). Zeroed timer+delta: the ori.b sled bypasses the timer
           check (0x2254/0x2258/0x225c never execute), runs the
           0x2260 move, the ror.l chain, the $0409 btst/beq (SCSP
           zeroed, exits), and reaches the 0x265e epilogue in 21 steps.
           The twin tails need a desc[1]!=0 fixture (open, P2 slice).
           Post-0x265e second-iteration flow is unpinned (frontier). */
        static const uint32_t f5_stops[] = {
            0x224eu, 0x2254u, 0x2258u, 0x225cu, 0x2260u,
            0x2262u, 0x2266u, 0x227cu, 0x2282u, 0x265eu
        };
        static const int f5_hit[] = {
            1, 0, 0, 0, 1, 0, 0, 1, 1, 1
        };
        static const uint32_t f5_steps[] = {
            2u, 0u, 0u, 0u, 7u, 0u, 0u, 19u, 20u, 21u
        };
        sweep_path(audio_rom, audio_rom_size, "f5-walk-active", 0x2242u,
                   0x2000u, 0x80u, f5_stops, f5_hit, f5_steps,
                   sizeof(f5_stops) / sizeof(f5_stops[0]));
    }
    free(audio_rom);
    if (failures != 0) {
        fprintf(stderr, "loopb: %d failures\n", failures);
    }
    return failures == 0 ? 0 : 1;
}
