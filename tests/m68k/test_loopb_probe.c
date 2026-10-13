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

/* Slice-1 Loop-B probe, re-pinned under the +2 PC-relative base
 * (v0823; the v0804-v0812 pins were +4-era).
 *
 * What survived: the walk/drain/epilogue legs still execute, with
 * stops moved to true instruction starts (drain entry 0x7a6, not
 * 0x7a8; walk jmp targets 0x7a6).
 * What flipped: Loop-B (0x902) is ordinary coherent code, not a
 * sled -- the scan runs all 32 iterations and exits via 0xf12.
 * The "trap at 0x934 / 0x00FF word / ori.b sleds" were
 * mid-instruction landings of the +4 base. F1/F3 pin the live
 * scan (stop 0xf12); F4 pins the walk landing.
 * The drain interior (0x7b6+: lea/move.w/andi.l/adda/descriptor
 * scan) is legal code too -- the 0x7bc/0x7b2 "bodges" were the
 * same artefact class. Legs reaching past the drain entry are
 * re-derived leg by leg below (F5-F10).
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
    /* Scan legs need room for 32 iterations (~140 steps); crash
       legs stop far earlier. One generous budget for all. */
    status = vf2_m68k_run(&bus, &regs, stop_pc, 0, 20000u,
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
                          uint32_t reg_a0, uint8_t stream0, uint8_t desc1,
                          uint32_t desc0a, uint32_t max_steps, vf2_status *status_out, uint32_t *pc_out,
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
    ram[reg_a0 + 1u] = desc1;
    ram[reg_a0 + 0x0au] = (uint8_t)(desc0a >> 24u);
    ram[reg_a0 + 0x0bu] = (uint8_t)(desc0a >> 16u);
    ram[reg_a0 + 0x0cu] = (uint8_t)(desc0a >> 8u);
    ram[reg_a0 + 0x0du] = (uint8_t)desc0a;
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
    *status_out = vf2_m68k_run(&bus, &regs, stop_pc, 0, max_steps, &done);
    *pc_out = regs.pc;
    *steps_out = done;
}

/* Stop-pc sweep: maps the executed prefix empirically. A hit is
   status OK + pc == stop; a miss records the trap point. */
static void sweep_path(const uint8_t *audio_rom, size_t audio_rom_size,
                       const char *name, uint32_t entry_pc, uint32_t reg_a0,
                       uint8_t stream0, uint8_t desc1, uint32_t desc0a,
                       uint32_t max_steps,
                       const uint32_t *stops,
                       const int *expect_hit, const uint32_t *expect_steps,
                       size_t nstops)
{
    size_t i = 0u;
    fprintf(stderr, "loopb sweep %s: entry=0x%05x stream0=0x%02x desc1=0x%02x\n", name,
            (unsigned)entry_pc, (unsigned)stream0, (unsigned)desc1);
    for (i = 0u; i < nstops; ++i) {
        vf2_status status = VF2_OK;
        uint32_t pc = 0u;
        uint32_t steps = 0u;
        int hit = 0;
        run_stop_case(audio_rom, audio_rom_size, entry_pc, stops[i], reg_a0,
                      stream0, desc1, desc0a, max_steps, &status, &pc, &steps);
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
    /* F1: direct scan entry, voice[9]=0 (bne arm always): the full
       32-iteration scan runs clean (128 insns), clears $152e, and
       exits via bra to 0xf12. 2 head + 128 scan + move.b + bra =
       132 steps to the 0xf12 landing. */
    run_loopb_case(audio_rom, audio_rom_size, "f1-scan-bne", 0x92au,
                   0xf12u, 0u, 0u, 0u, 0u, 0u, 0u,
                   VF2_OK, 0xf12u, 132u);
    /* F3: full Loop-B head entry ($150e&0xf=3, $152e=5): 9 head
       insns (d4-residue indexed ROM peek, 0xf gate, $152e>=5 gate),
       then the identical 130-step scan+exit. 141 to 0xf12. */
    run_loopb_case(audio_rom, audio_rom_size, "f3-head-to-scan", 0x902u,
                   0xf12u, 0u, 0u, 3u, 5u, 0u, 0u,
                   VF2_OK, 0xf12u, 141u);
    /* F4: walk head, zeroed flags (bmi not taken), a0 = stream slot 0:
       falls to the drain jmp $7a6. 2 insns, stops clean at the
       TRUE drain entry 0x7a6 (move.b (a4),d0); 0x7a8 is now
       drain-interior (bne). Entered mid-head at 0x2244 as the L2
       worker jumps there; d0/flags are fixture-zero here, so this
       pins the N=0 arm only. */
    run_loopb_case(audio_rom, audio_rom_size, "f4-walk-inactive", 0x2244u,
                   0x7a6u, 0u, 0u, 0u, 0u, 0x2000u, 0u,
                   VF2_OK, 0x7a6u, 2u);
    {
        /* F5 path-map sweep, ACTIVE stream via the head (entry 0x2242
           executes the move.b that sets d0/N from the fixture; entering
           at 0x2244 would leave fixture-zero flags and re-test the N=0
           arm). Path (stream0=0x80, desc1=0): bmi taken (N=1),
           bpl taken (0+0), bne not taken (d2=0), 10x ror.l, btst
           $0409 (SCSP zeroed, Z=1), beq to the 0x265c epilogue in 20
           steps. Misses are untaken arms (0x2248/0x225c/0x228e).
           Post-0x265e second-iteration flow is unpinned (frontier). */
        static const uint32_t f5_stops[] = {
            0x224cu, 0x2248u, 0x2254u, 0x225cu, 0x2260u,
            0x227cu, 0x2282u, 0x228eu, 0x265cu
        };
        static const int f5_hit[] = {
            1, 0, 1, 0, 1, 1, 1, 0, 1
        };
        static const uint32_t f5_steps[] = {
            2u, 0u, 4u, 0u, 5u, 17u, 18u, 0u, 19u
        };
        sweep_path(audio_rom, audio_rom_size, "f5-walk-active", 0x2242u,
                   0x2000u, 0x80u, 0u, 0u, 200u, f5_stops, f5_hit, f5_steps,
                   sizeof(f5_stops) / sizeof(f5_stops[0]));
    }
    {
        /* F6 twin-tail approach (P2 slice, re-pinned v0823): desc[1]!=0
           walk arm. bls at 0x22c6 is TAKEN (d5 = 0 <= 0x200) to the
           0x23b0 btst; beq (bit0 of record byte0 = 0x80 clear) to
           0x24be; beq (bit3 clear) to 0x2522; the 0x252e movem
           executes and the illegal-ea store at 0x2532 refuses.
           Chain: 23 true steps. (The old 45-step 0x23fa pin was the
           +4 cascade's longer fall-through.) */
        static const uint32_t f6_stops[] = {
            0x2264u, 0x228eu, 0x22b4u, 0x22c6u, 0x23b0u,
            0x24beu, 0x2522u, 0x252eu
        };
        static const int f6_hit[] = {
            1, 1, 1, 1, 1, 1, 1, 1
        };
        static const uint32_t f6_steps[] = {
            6u, 7u, 9u, 13u, 14u, 16u, 18u, 22u
        };
        sweep_path(audio_rom, audio_rom_size, "f6-prefix", 0x2242u,
                   0x2000u, 0x80u, 0x01u, 0u, 20000u, f6_stops, f6_hit,
                   f6_steps, sizeof(f6_stops) / sizeof(f6_stops[0]));
    }
    {
        /* F6 phantom alignment pins: mid-instruction addresses on the
           true stream (0x22b6 in move.l 0x22b4; 0x22e6 in movem
           0x22e4; 0x23b2 in btst 0x23b0). The run passes through
           them without pc ever equaling them. */
        static const uint32_t f6_phantom[] = { 0x22b6u, 0x22e6u, 0x23b2u };
        static const int f6_phit[] = { 0, 0, 0 };
        static const uint32_t f6_psteps[] = { 0u, 0u, 0u };
        sweep_path(audio_rom, audio_rom_size, "f6-phantoms", 0x2242u,
                   0x2000u, 0x80u, 0x01u, 0u, 20000u, f6_phantom, f6_phit,
                   f6_psteps,
                   sizeof(f6_phantom) / sizeof(f6_phantom[0]));
    }
    {
        /* F6 crash verdict, both sides of the 0x228e gate: desc1=0x01
           takes bne straight to 0x22b4 (23 steps); desc1=0x80 falls
           through the 10x ror chain (35 steps). Both die UNSUPPORTED
           at the illegal-ea movem 0x2532. */
        static const uint8_t f6_desc1[] = { 0x01u, 0x80u };
        static const uint32_t f6_expect[] = { 23u, 35u };
        size_t v = 0u;
        for (v = 0u; v < sizeof(f6_desc1) / sizeof(f6_desc1[0]); ++v) {
            vf2_status status = VF2_OK;
            uint32_t pc = 0u;
            uint32_t steps = 0u;
            run_stop_case(audio_rom, audio_rom_size, 0x2242u, 0xffffffu,
                          0x2000u, 0x80u, f6_desc1[v], 0u, 20000u, &status,
                          &pc, &steps);
            fprintf(stderr, "f6-crash desc1=0x%02x: status=%d pc=0x%05x steps=%u\n",
                    (unsigned)f6_desc1[v], (int)status, (unsigned)pc,
                    (unsigned)steps);
            EXPECT_TRUE(status == VF2_ERROR_UNSUPPORTED);
            EXPECT_TRUE(pc == 0x2532u);
            EXPECT_TRUE(steps == f6_expect[v]);
        }
    }
    {
        /* F8a safe arm (P2 slice, re-derived v0823): third-arm entry
           at 0x25ee (flag-clean: first instr overwrites d2).
           THREE passes, all live -- nothing crashes here:
           pass 1 (0x25fa arm: bset #3 + desc[1] += 0x10) -> epilogue
           (0x266e reset arm, $16 == 0) -> pass 2 via 0x2242 (byte0 =
           0x88 now: bmi taken, ... 0x24d2 arm clears byte0) ->
           epilogue -> pass 3 (byte0 = 0: 0x2248 jmp) into the 0x7a6
           drain. The old "dies on the illegal move.b-An at 0x7bc"
           was +4 fiction (0x7bc is move.w $1508(a6),d0). Stops are
           first arrivals; counts below are predicted, silicon
           decides (adjusted to measured). */
        static const uint32_t f8a_stops[] = {
            0x25fau, 0x2604u, 0x265cu, 0x2242u, 0x24eeu, 0x2248u,
            0x7a8u
        };
        static const int f8a_hit[] = { 1, 1, 1, 1, 1, 1, 1 };
        static const uint32_t f8a_steps[] = {
            3u, 5u, 7u, 11u, 38u, 52u, 54u
        };
        sweep_path(audio_rom, audio_rom_size, "f8a-safe", 0x25eeu,
                   0x2000u, 0x80u, 0x00u, 0u, 20000u, f8a_stops, f8a_hit,
                   f8a_steps, sizeof(f8a_stops) / sizeof(f8a_stops[0]));
    }
    {
        /* F8a mutation effects: pass 1 leaves desc[1] += 0x10
           (0x2604 addi.b) and pass 2's 0x24d2 arm clears byte0
           (0x24ee). Measured: desc01=0x10, desc00=0x00 after the
           0x7a8 arrival. This is the measured desc[1]-author for
           the arming argument: each safe pass arms the next pass's
           0x228e arm (desc[1] != 0). */
        static uint8_t ram[VF2_SOUND_RAM_BYTES];
        static vf2_scsp scsp;
        vf2_m68k_bus bus;
        vf2_m68k_regs regs;
        uint32_t done = 0u;
        vf2_status status = VF2_OK;
        memset(ram, 0, sizeof(ram));
        vf2_scsp_initialize(&scsp);
        ram[0x2000u] = 0x80u;
        ram[0x2001u] = 0x00u;
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
        regs.a[0] = UINT32_C(0x2000);
        regs.pc = UINT32_C(0x25ee);
        status = vf2_m68k_run(&bus, &regs, UINT32_C(0x7a8), 0, 20000u,
                              &done);
        EXPECT_TRUE(status == VF2_OK);
        EXPECT_TRUE(regs.pc == UINT32_C(0x7a8));
        EXPECT_TRUE(done == 54u);
        EXPECT_TRUE(ram[0x2001u] == 0x10u);
        EXPECT_TRUE(ram[0x2000u] == 0x00u);
        EXPECT_TRUE(bus.event_count == 0u);
        fprintf(stderr, "f8a-mutate: steps=%u desc01=0x%02x desc00=0x%02x\n",
                (unsigned)done, (unsigned)ram[0x2001u],
                (unsigned)ram[0x2000u]);
    }
    {
        /* F8a end verdict: the 0x7a8 arrival above is the pass-3
           drain entry (OK, 54 steps). Past it the drain/epilogue
           livelocks on the zeroed record (frontier, not a crash). */
        vf2_status status = VF2_OK;
        uint32_t pc = 0u;
        uint32_t steps = 0u;
        run_stop_case(audio_rom, audio_rom_size, 0x25eeu, 0x7a8u,
                      0x2000u, 0x80u, 0x00u, 0u, 20000u, &status, &pc,
                      &steps);
        fprintf(stderr, "f8a-drain: status=%d pc=0x%05x steps=%u\n",
                (int)status, (unsigned)pc, (unsigned)steps);
        EXPECT_TRUE(status == VF2_OK);
        EXPECT_TRUE(pc == 0x7a8u);
        EXPECT_TRUE(steps == 54u);
    }
    {
        /* F8b twin arm (desc[1]=0x81, bit7 set): bne at 0x25f6 takes
           the 0x260c voice-setup arm (SCSP writes + byte0 clear),
           then the epilogue and one walk pass into the drain.
           Predicted 25 steps to 0x7a8; silicon decides. */
        static const uint32_t f8b_stops[] = {
            0x260cu, 0x2628u, 0x265cu, 0x2248u, 0x7a8u
        };
        static const int f8b_hit[] = { 1, 1, 1, 1, 1 };
        static const uint32_t f8b_steps[] = { 3u, 10u, 18u, 24u, 26u };
        sweep_path(audio_rom, audio_rom_size, "f8b-bodge", 0x25eeu,
                   0x2000u, 0x80u, 0x81u, 0u, 20000u, f8b_stops, f8b_hit,
                   f8b_steps, sizeof(f8b_stops) / sizeof(f8b_stops[0]));
    }
    {
        /* F8b end verdict (bounded OK at the drain entry). */
        vf2_status status = VF2_OK;
        uint32_t pc = 0u;
        uint32_t steps = 0u;
        run_stop_case(audio_rom, audio_rom_size, 0x25eeu, 0x7a8u,
                      0x2000u, 0x80u, 0x81u, 0u, 20000u, &status, &pc,
                      &steps);
        fprintf(stderr, "f8b-drain: status=%d pc=0x%05x steps=%u\n",
                (int)status, (unsigned)pc, (unsigned)steps);
        EXPECT_TRUE(status == VF2_OK);
        EXPECT_TRUE(pc == 0x7a8u);
        EXPECT_TRUE(steps == 26u);
    }
    {
        /* F8c drain interior on the F8a fixture (pass-3 drain passage:
           d0 = RAM[0] = 0, bne not taken, jmp to the epilogue).
           Stops are first arrivals (predicted; silicon decides).
           Past 0x7ac the drain/epilogue livelocks on the zeroed
           record -- frontier, not a crash. */
        static const uint32_t f8c_stops[] = {
            0x7a6u, 0x7a8u, 0x7acu
        };
        static const int f8c_hit[] = { 1, 1, 1 };
        static const uint32_t f8c_steps[] = { 53u, 54u, 55u };
        sweep_path(audio_rom, audio_rom_size, "f8c-drain", 0x25eeu,
                   0x2000u, 0x80u, 0x00u, 0u, 20000u, f8c_stops, f8c_hit,
                   f8c_steps, sizeof(f8c_stops) / sizeof(f8c_stops[0]));
    }
    {
        /* F8c2 walk idle livelock: the epilogue's 0x266e reset arm
           ($16 == 0) leaves a0 = $1000 -- the RING bytes as a
           record, not a stream slot. Every lap is the drain-idle
           passage (byte0 = 0: 0x2248 jmp; drain reads RAM[0] = 0
           via a4 = 0, bne not taken, jmp to the epilogue):
           0x2242,44,48,7a6,7a8,7ac = 6 steps/lap, nothing mutated,
           forever. Pinned as a two-lap fixed point (same arrival
           twice from the reset state). */
        static uint8_t ram[VF2_SOUND_RAM_BYTES];
        static vf2_scsp scsp;
        vf2_m68k_bus bus;
        vf2_m68k_regs regs;
        uint32_t done1 = 0u;
        uint32_t done2 = 0u;
        vf2_status status = VF2_OK;
        memset(ram, 0, sizeof(ram));
        vf2_scsp_initialize(&scsp);
        ram[0x2000u] = 0x80u;
        ram[0x2001u] = 0x00u;
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
        regs.a[0] = UINT32_C(0x1000);
        regs.a[4] = UINT32_C(0x0000);
        regs.pc = UINT32_C(0x2242);
        status = vf2_m68k_run(&bus, &regs, UINT32_C(0x265c), 0, 20000u,
                              &done1);
        EXPECT_TRUE(status == VF2_OK);
        EXPECT_TRUE(regs.pc == UINT32_C(0x265c));
        EXPECT_TRUE(done1 == 6u);
        vf2_m68k_regs_reset(&regs);
        regs.a[5] = UINT32_C(0x100000);
        regs.a[6] = UINT32_C(0x1000);
        regs.a[7] = UINT32_C(0x7f00);
        regs.a[0] = UINT32_C(0x1000);
        regs.a[4] = UINT32_C(0x0000);
        regs.pc = UINT32_C(0x2242);
        status = vf2_m68k_run(&bus, &regs, UINT32_C(0x265c), 0, 20000u,
                              &done2);
        EXPECT_TRUE(status == VF2_OK);
        EXPECT_TRUE(regs.pc == UINT32_C(0x265c));
        EXPECT_TRUE(done2 == 6u);
        EXPECT_TRUE(bus.event_count == 0u);
        fprintf(stderr, "f8c2-lap: lap1=%u lap2=%u (idle fixed point)\n",
                (unsigned)done1, (unsigned)done2);
    }
    {
        /* F9 drain-head flag contract (P2 slice, re-derived v0823):
           TRUE entry 0x7a6 with a production-shaped fixture
           (a4 = $1506 count). count=1: bne taken, push, count--,
           the 20-iteration descriptor scan (all zeroed: btst/beq
           skip each), cursor advance ($1508 += 4), restore, exit
           via 0x816 in 98 steps (OK). count=0: bne not taken, jmp
           to the 0x265c epilogue in 3 steps (OK). No bodge, no
           crash -- the old 0x7bc verdicts were +4 fiction. */
        static uint8_t ram[VF2_SOUND_RAM_BYTES];
        static vf2_scsp scsp;
        vf2_m68k_bus bus;
        vf2_m68k_regs regs;
        uint32_t done = 0u;
        vf2_status status = VF2_OK;
        memset(ram, 0, sizeof(ram));
        vf2_scsp_initialize(&scsp);
        ram[VF2_SOUND_COMMAND_COUNT] = 1u;
        ram[VF2_SOUND_COMMAND_RING_BASE] = 0xe4u;
        ram[VF2_SOUND_COMMAND_RING_BASE + 1u] = 4u;
        bus.ram = ram;
        bus.ram_size = sizeof(ram);
        bus.rom = audio_rom;
        bus.rom_size = audio_rom_size;
        bus.sample_rom = NULL;
        bus.sample_rom_size = 0u;
        bus.scsp = &scsp;
        bus.event_count = 0u;
        vf2_m68k_regs_reset(&regs);
        regs.a[4] = UINT32_C(0x2506);
        regs.a[5] = UINT32_C(0x100000);
        regs.a[6] = UINT32_C(0x1000);
        regs.a[7] = UINT32_C(0x7f00);
        regs.pc = UINT32_C(0x7a6);
        status = vf2_m68k_run(&bus, &regs, UINT32_C(0x816), 0, 20000u,
                              &done);
        fprintf(stderr, "f9-drain count=1: status=%d pc=0x%05x steps=%u\n",
                (int)status, (unsigned)regs.pc, (unsigned)done);
        EXPECT_TRUE(status == VF2_OK);
        EXPECT_TRUE(regs.pc == 0x816u);
        EXPECT_TRUE(done == 98u);
        EXPECT_TRUE(regs.a[7] == UINT32_C(0x7f00));
        /* Idle arm: count=0 exits to the epilogue without scanning. */
        memset(ram, 0, sizeof(ram));
        vf2_scsp_initialize(&scsp);
        bus.event_count = 0u;
        vf2_m68k_regs_reset(&regs);
        regs.a[4] = UINT32_C(0x2506);
        regs.a[5] = UINT32_C(0x100000);
        regs.a[6] = UINT32_C(0x1000);
        regs.a[7] = UINT32_C(0x7f00);
        regs.pc = UINT32_C(0x7a6);
        status = vf2_m68k_run(&bus, &regs, UINT32_C(0x265c), 0, 20000u,
                              &done);
        fprintf(stderr, "f9-drain count=0: status=%d pc=0x%05x steps=%u\n",
                (int)status, (unsigned)regs.pc, (unsigned)done);
        EXPECT_TRUE(status == VF2_OK);
        EXPECT_TRUE(regs.pc == 0x265cu);
        EXPECT_TRUE(done == 3u);
        EXPECT_TRUE(regs.a[7] == UINT32_C(0x7f00));
    }
    {
        /* F10 map (P2 slice, re-derived v0823): full L2 tick from the
           vectored entry 0x1f16, preempted a0 = 0, zeroed records,
           count 0. movem + SCSP prologue, count gate (0 != 0xff:
           fall through), 0x1f36 prescaler jsr (zeroed +0xc: beq to
           the 0x2240 rts -- the TRUE prescaler is a countdown gate
           ending in return, v0816). Stops are first arrivals
           (predicted; silicon decides). */
        static const uint32_t f10_stops[] = {
            0x1f36u, 0x21bau, 0x2240u
        };
        static const int f10_hit[] = { 1, 1, 1 };
        static const uint32_t f10_steps[] = {
            7u, 8u, 10u
        };
        sweep_path(audio_rom, audio_rom_size, "f10-map", 0x1f16u,
                   0u, 0u, 0u, 0u, 20000u, f10_stops, f10_hit,
                   f10_steps, sizeof(f10_stops) / sizeof(f10_stops[0]));
    }
    {
        /* F10 end verdict: the full boot-state L2 tick runs clean to
           its rte. 7 head + prescaler jsr/ret (4) + 8 workers (one
           bare 4-step + seven 5-step lea/jsr/btst/bne/rts) + movem
           restore = 51 steps to the 0x1f7a arrival, a7 balanced
           (every jsr/rts and the entry movem pair up). No events
           (SCSP writes are model-silent). */
        static uint8_t ram[VF2_SOUND_RAM_BYTES];
        static vf2_scsp scsp;
        vf2_m68k_bus bus;
        vf2_m68k_regs regs;
        uint32_t done = 0u;
        vf2_status status = VF2_OK;
        memset(ram, 0, sizeof(ram));
        vf2_scsp_initialize(&scsp);
        ram[0x2506u] = 0u;
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
        regs.pc = UINT32_C(0x1f16);
        status = vf2_m68k_run(&bus, &regs, UINT32_C(0x1f7a), 0, 200000u,
                              &done);
        fprintf(stderr, "f10-l2tick: status=%d pc=0x%05x steps=%u a7=0x%04x events=%u\n",
                (int)status, (unsigned)regs.pc, (unsigned)done,
                (unsigned)regs.a[7], (unsigned)bus.event_count);
        EXPECT_TRUE(status == VF2_OK);
        EXPECT_TRUE(regs.pc == UINT32_C(0x1f7a));
        EXPECT_TRUE(done == 51u);
        EXPECT_TRUE(regs.a[7] == UINT32_C(0x7f00));
        EXPECT_TRUE(bus.event_count == 0u);
    }
    {
        /* F7 bulk arm (P2 slice, re-pinned v0823): desc[0x0a] clears
           the 0x22c6 bls (d5 > 0x200 falls through to 0x22ca), runs
           subi/addi/andi/move/move/move to the 0x22e4 movem (step
           21); 0x22e8 holds the same illegal-ea bytes as 0x2532
           (48 d2 00 ff) and refuses the same way. Both walk arms die
           on the same illegal store form, at different sites. */
        static const uint32_t f7_stops[] = {
            0x22cau, 0x22d0u, 0x22d4u, 0x22d8u, 0x22dcu, 0x22e0u,
            0x22e4u
        };
        static const int f7_hit[] = { 1, 1, 1, 1, 1, 1, 1 };
        static const uint32_t f7_steps[] = {
            14u, 15u, 16u, 17u, 18u, 19u, 20u
        };
        sweep_path(audio_rom, audio_rom_size, "f7-prefix", 0x2242u,
                   0x2000u, 0x80u, 0x01u, 0x0400u, 20000u, f7_stops,
                   f7_hit, f7_steps,
                   sizeof(f7_stops) / sizeof(f7_stops[0]));
    }
    {
        /* F7 crash verdict, both sides of the bls gate's taken arm:
           UNSUPPORTED at 0x22e8 after 21 true steps. */
        static const uint32_t f7_desc0a[] = { 0x0400u, 0x0201u };
        size_t v = 0u;
        for (v = 0u; v < sizeof(f7_desc0a) / sizeof(f7_desc0a[0]); ++v) {
            vf2_status status = VF2_OK;
            uint32_t pc = 0u;
            uint32_t steps = 0u;
            run_stop_case(audio_rom, audio_rom_size, 0x2242u, 0xffffffu,
                          0x2000u, 0x80u, 0x01u, f7_desc0a[v], 20000u,
                          &status, &pc, &steps);
            fprintf(stderr, "f7-crash desc0a=0x%04x: status=%d pc=0x%05x steps=%u\n",
                    (unsigned)f7_desc0a[v], (int)status, (unsigned)pc,
                    (unsigned)steps);
            EXPECT_TRUE(status == VF2_ERROR_UNSUPPORTED);
            EXPECT_TRUE(pc == 0x22e8u);
            EXPECT_TRUE(steps == 21u);
        }
    }
    free(audio_rom);
    if (failures != 0) {
        fprintf(stderr, "loopb: %d failures\n", failures);
    }
    return failures == 0 ? 0 : 1;
}
