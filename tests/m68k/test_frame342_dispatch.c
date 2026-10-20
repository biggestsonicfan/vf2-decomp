#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/m68k_step.h"
#include "vf2/rom.h"
#include "vf2/scsp.h"
#include "vf2/sound_board.h"
#include "vf2/status.h"

/* Real original i960 frame-342 vector-15 UART witness:
 *   9e0a7f cd74f8 ca7ef8 cb7ff8 ad2100
 *
 * The original 68000 L3 receives all 15 bytes and files 5 entries.
 * From that exact filed state, each individual command's original
 * 68000 drain is evaluated from the SAME post-L3 snapshot. The
 * recovered C dispatcher is independently evaluated from that
 * same snapshot (count=1, cursor=i*4 to isolate a command).
 * The initial sound-board state comes from the actual owner audio/
 * sample ROMs, NOT the original i960's full concurrent audio state.
 * Therefore this is an isolated 68000 dispatch experiment, not
 * evidence that an arcade fighting hit triggered SCSP PCM.
 */
static vf2_sound_board board;
static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
static vf2_scsp step_scsp;
static uint8_t filed_ram[VF2_SOUND_RAM_BYTES];
static vf2_scsp filed_scsp;
static int failures;
static int probe_ad21_l2;
static int verify_ad21_ba;
static int16_t frame342_c_left[4096], frame342_c_right[4096];
static int16_t frame342_s_left[4096], frame342_s_right[4096];

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "FAIL:%u: %s\n", (unsigned)__LINE__, #c); \
    ++failures; } } while (0)

static unsigned active_slots(const vf2_scsp *scsp)
{
    unsigned n = 0u;
    size_t i;
    for (i = 0u; i < VF2_SCSP_SLOT_COUNT; ++i) {
        n += scsp->slot_active[i] != 0u ? 1u : 0u;
    }
    return n;
}

static int file_stream(const uint8_t *rom, size_t rom_size,
                       const uint8_t *samples, size_t sample_size)
{
    static const uint8_t uart[15] = {
        0x9eu, 0x0au, 0x7fu, 0xcdu, 0x74u,
        0xf8u, 0xcau, 0x7eu, 0xf8u, 0xcbu,
        0x7fu, 0xf8u, 0xadu, 0x21u, 0x00u
    };
    size_t i;
    if (vf2_sound_board_initialize(&board, rom, rom_size,
                                    samples, sample_size) != VF2_OK) {
        return 0;
    }
    for (i = 0u; i < sizeof(uart); ++i) {
        if (vf2_sound_board_file_uart_byte(&board, uart[i]) != VF2_OK) {
            fprintf(stderr, "frame342: L3 failed at byte=%u\n", (unsigned)i);
            return 0;
        }
    }
    if (board.sound_ram[VF2_SOUND_COMMAND_COUNT] != 5u ||
        board.sound_ram[VF2_SOUND_COMMAND_WRITE_CURSOR] != 0u ||
        board.sound_ram[VF2_SOUND_COMMAND_WRITE_CURSOR + 1u] != 0x14u) {
        fprintf(stderr, "frame342: L3 did not file exactly 5 natural commands\n");
        return 0;
    }
    memcpy(filed_ram, board.sound_ram, sizeof(filed_ram));
    memcpy(&filed_scsp, &board.scsp, sizeof(filed_scsp));
    return 1;
}

/* Owner-ROM-only checkpoint trace.  Run exactly one 68000 instruction
 * per call to expose *which* B0 table door was reached; max_steps=1
 * returns UNSUPPORTED after a completed instruction by design.
 * This is intentionally separate from the strict C/68000 comparison. */
static void trace_ad21_ba_opcode_path(const uint8_t *initial_ram,
                                      const vf2_scsp *initial_scsp,
                                      uint16_t cursor,
                                      const uint8_t *rom, size_t rom_size,
                                      const uint8_t *samples, size_t sample_size)
{
    static uint8_t trace_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp trace_scsp;
    vf2_m68k_bus trace_bus;
    vf2_m68k_regs regs;
    uint32_t step;
    unsigned checkpoints = 0u;
    memcpy(trace_ram, initial_ram, sizeof(trace_ram));
    memcpy(&trace_scsp, initial_scsp, sizeof(trace_scsp));
    if (vf2_scsp_attach_sample_ram(
            &trace_scsp, trace_ram, sizeof(trace_ram)) != VF2_OK) {
        ++failures;
        return;
    }
    trace_ram[VF2_SOUND_COMMAND_COUNT] = 1u;
    trace_ram[VF2_SOUND_COMMAND_CURSOR] = (uint8_t)(cursor >> 8u);
    trace_ram[VF2_SOUND_COMMAND_CURSOR + 1u] = (uint8_t)cursor;
    memset(&trace_bus, 0, sizeof(trace_bus));
    trace_bus.ram = trace_ram;
    trace_bus.ram_size = sizeof(trace_ram);
    trace_bus.rom = rom;
    trace_bus.rom_size = rom_size;
    trace_bus.sample_rom = samples;
    trace_bus.sample_rom_size = sample_size;
    trace_bus.scsp = &trace_scsp;
    vf2_m68k_regs_reset(&regs);
    regs.pc = UINT32_C(0x7a6);
    regs.a[4] = UINT32_C(0x2506);
    regs.a[5] = VF2_SCSP_CPU_BASE;
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = ((uint32_t)rom[0u] << 24u) |
                ((uint32_t)rom[1u] << 16u) |
                ((uint32_t)rom[2u] << 8u) |
                (uint32_t)rom[3u];
    for (step = 0u; step < 50000u; ++step) {
        uint32_t pc = regs.pc;
        uint32_t executed = 0u;
        vf2_status status;
        if (pc == UINT32_C(0x265c)) break;
        if ((pc >= UINT32_C(0x13ca) && pc < UINT32_C(0x1500)) ||
            (pc >= UINT32_C(0x1ac0) && pc <= UINT32_C(0x1b4a))) {
            if (checkpoints < 32u) {
                printf("FRAME342_AD21_BA_PC_TRACE cursor=%04x step=%u "
                       "pc=%05x d0=%08x d2=%08x a3=%08x sp=%08x\n",
                       (unsigned)cursor, (unsigned)step, (unsigned)pc,
                       (unsigned)regs.d[0], (unsigned)regs.d[2],
                       (unsigned)regs.a[3], (unsigned)regs.a[7]);
                ++checkpoints;
            }
        }
        status = vf2_m68k_run(&trace_bus, &regs, UINT32_C(0x265c),
                              0, 1u, &executed);
        if (executed != 1u ||
            (status != VF2_ERROR_UNSUPPORTED && status != VF2_OK)) {
            printf("FRAME342_AD21_BA_PC_TRACE_STOP cursor=%04x step=%u "
                   "status=%d executed=%u pc=%05x\n",
                   (unsigned)cursor, (unsigned)step,
                   (int)status, (unsigned)executed, (unsigned)regs.pc);
            break;
        }
    }
    printf("FRAME342_AD21_BA_PC_TRACE_END cursor=%04x steps=%u pc=%05x "
           "checkpoints=%u\n", (unsigned)cursor, (unsigned)step,
           (unsigned)regs.pc, checkpoints);
}

/* Diagnostic only: after the owner-ROM AD21 L2 tick, isolate the
 * BA 2A 00 and BA 07 7F entries for independent C/68000 dispatch.
 * Do not assert equivalence or imply KEYON before measuring it. */
static void probe_ad21_ba_dispatch(const uint8_t *rom, size_t rom_size,
                                    const uint8_t *samples, size_t sample_size)
{
    static uint8_t c_snapshot[VF2_SOUND_RAM_BYTES];
    static uint8_t m_snapshot[VF2_SOUND_RAM_BYTES];
    static vf2_scsp c_scsp_snapshot, m_scsp_snapshot;
    size_t which;
    memcpy(c_snapshot, board.sound_ram, sizeof(c_snapshot));
    memcpy(m_snapshot, step_ram, sizeof(m_snapshot));
    memcpy(&c_scsp_snapshot, &board.scsp, sizeof(c_scsp_snapshot));
    memcpy(&m_scsp_snapshot, &step_scsp, sizeof(m_scsp_snapshot));
    /* The VF2.1 owner sound program is word-swapped on loading.
     * 0x608df0 = maximum B0 uploader selector (original value: 1).
     * 0x605b26 = initial DBF byte count (original: 0x32c7).
     * BA 2A and BA 07 exceed the maximum and must NOT enter
     * the sample-copy loop, though both stage the initial bytes. */
    if (verify_ad21_ba) {
        size_t staging_bytes;
        uint16_t selector_max;
        if (rom_size <= 0x8df1u || rom_size <= 0x5b27u) {
            fprintf(stderr, "FRAME342_AD21_BA_STRICT missing original ROM table\n");
            ++failures;
            return;
        }
        staging_bytes = (size_t)(((uint16_t)rom[0x5b26u] << 8u) |
                                 rom[0x5b27u]) + 1u;
        selector_max = (uint16_t)(((uint16_t)rom[0x8df0u] << 8u) |
                                  rom[0x8df1u]);
        printf("FRAME342_AD21_BA_ROM_TABLE max_selector=%u stage_bytes=%u\n",
               (unsigned)selector_max, (unsigned)staging_bytes);
        if (selector_max != 1u || staging_bytes != 13000u ||
            staging_bytes > VF2_SOUND_RAM_BYTES - 0x5000u ||
            staging_bytes > rom_size - 0x5b28u) {
            fprintf(stderr,
                    "FRAME342_AD21_BA_STRICT ROM selector/staging guard failed\n");
            ++failures;
            return;
        }
    }
    for (which = 0u; which < 2u; ++which) {
        const unsigned packet = which == 0u ? 1u : 7u;
        const uint16_t cursor = (uint16_t)(0x14u + packet * 4u);
        vf2_sound_dispatch_report report;
        vf2_m68k_bus bus;
        vf2_m68k_regs regs;
        vf2_status cs, ms;
        uint32_t steps = 0u;
        size_t differences = 0u, first = VF2_SOUND_RAM_BYTES, i;
        size_t c_sample_changed = 0u, m_sample_changed = 0u;
        memcpy(board.sound_ram, c_snapshot, sizeof(c_snapshot));
        memcpy(step_ram, m_snapshot, sizeof(m_snapshot));
        memcpy(&board.scsp, &c_scsp_snapshot, sizeof(c_scsp_snapshot));
        memcpy(&step_scsp, &m_scsp_snapshot, sizeof(m_scsp_snapshot));
        (void)vf2_scsp_attach_sample_ram(&board.scsp,
                  board.sound_ram, sizeof(board.sound_ram));
        (void)vf2_scsp_attach_sample_ram(&step_scsp,
                  step_ram, sizeof(step_ram));
        board.sound_ram[VF2_SOUND_COMMAND_COUNT] = 1u;
        step_ram[VF2_SOUND_COMMAND_COUNT] = 1u;
        board.sound_ram[VF2_SOUND_COMMAND_CURSOR] = (uint8_t)(cursor >> 8u);
        board.sound_ram[VF2_SOUND_COMMAND_CURSOR + 1u] = (uint8_t)cursor;
        step_ram[VF2_SOUND_COMMAND_CURSOR] = (uint8_t)(cursor >> 8u);
        step_ram[VF2_SOUND_COMMAND_CURSOR + 1u] = (uint8_t)cursor;
        /* Validate the exact original L2 packet before dispatching:
         * the old diagnostic checked only after mutating both sides. */
        if (board.sound_ram[VF2_SOUND_COMMAND_RING_BASE + cursor] != 0xbau ||
            step_ram[VF2_SOUND_COMMAND_RING_BASE + cursor] != 0xbau ||
            board.sound_ram[VF2_SOUND_COMMAND_RING_BASE + cursor + 1u] !=
                (uint8_t)(which == 0u ? 0x2au : 7u) ||
            step_ram[VF2_SOUND_COMMAND_RING_BASE + cursor + 1u] !=
                (uint8_t)(which == 0u ? 0x2au : 7u) ||
            board.sound_ram[VF2_SOUND_COMMAND_RING_BASE + cursor + 2u] !=
                (uint8_t)(which == 0u ? 0u : 0x7fu) ||
            step_ram[VF2_SOUND_COMMAND_RING_BASE + cursor + 2u] !=
                (uint8_t)(which == 0u ? 0u : 0x7fu)) {
            fprintf(stderr,
                    "FRAME342_AD21_BA incorrect original L2 packet at %04x\n",
                    (unsigned)cursor);
            ++failures;
            continue;
        }
        memset(&report, 0, sizeof(report));
        cs = vf2_sound_board_dispatch_next(&board, &report);
        memset(&bus, 0, sizeof(bus));
        bus.ram = step_ram;
        bus.ram_size = sizeof(step_ram);
        bus.rom = rom;
        bus.rom_size = rom_size;
        bus.sample_rom = samples;
        bus.sample_rom_size = sample_size;
        bus.scsp = &step_scsp;
        vf2_m68k_regs_reset(&regs);
        regs.pc = UINT32_C(0x7a6);
        regs.a[4] = UINT32_C(0x2506);
        regs.a[5] = VF2_SCSP_CPU_BASE;
        regs.a[6] = UINT32_C(0x1000);
        /* The CPU reset vector in the original word-swapped sound
         * EPR sets SSP/A7 = $5000 (ROM bytes 00 00 50 00).
         * DO NOT use the $7f00 L2 test stack here: the BA uploader
         * stages $32c8 bytes at RAM $5000..$82c7, which would
         * overwrite return addresses on a synthetic $7f00 stack.
         * With the original $5000 SP, 68k pushes grow below staging. */
        regs.a[7] = ((uint32_t)rom[0u] << 24u) |
                    ((uint32_t)rom[1u] << 16u) |
                    ((uint32_t)rom[2u] << 8u) |
                    (uint32_t)rom[3u];
        if (regs.a[7] != UINT32_C(0x5000)) {
            fprintf(stderr,
                    "FRAME342_AD21_BA unexpected original SSP %08x\n",
                    (unsigned)regs.a[7]);
            ++failures;
            continue;
        }
        ms = vf2_m68k_run(&bus, &regs, UINT32_C(0x265c),
                          0, 50000u, &steps);
        if (verify_ad21_ba) {
            trace_ad21_ba_opcode_path(m_snapshot, &m_scsp_snapshot,
                                      cursor, rom, rom_size,
                                      samples, sample_size);
        }
        {
            size_t sample_diff = 0u, sample_first = VF2_SOUND_RAM_BYTES;
            size_t scsp_first = VF2_SCSP_REGISTER_BYTES;
            size_t c_scsp_changed = 0u, m_scsp_changed = 0u;
            for (i = 0x50000u; i < VF2_SOUND_RAM_BYTES; ++i) {
                if (board.sound_ram[i] != c_snapshot[i]) ++c_sample_changed;
                if (step_ram[i] != m_snapshot[i]) ++m_sample_changed;
                if (board.sound_ram[i] != step_ram[i]) {
                    if (sample_first == VF2_SOUND_RAM_BYTES) sample_first = i;
                    ++sample_diff;
                }
            }
            for (i = 0u; i < VF2_SCSP_REGISTER_BYTES; ++i) {
                if (board.scsp.registers[i] !=
                    c_scsp_snapshot.registers[i]) ++c_scsp_changed;
                if (step_scsp.registers[i] !=
                    m_scsp_snapshot.registers[i]) ++m_scsp_changed;
                if (board.scsp.registers[i] != step_scsp.registers[i] &&
                    scsp_first == VF2_SCSP_REGISTER_BYTES) scsp_first = i;
            }
            printf("FRAME342_AD21_BA_DETAIL packet=%u sample_diff=%u "
                   "sample_first=%05x scsp_first=%04x "
                   "C_handler=%05x C_handled=%u C_matches=%u "
                   "C_sample_changed=%u M_sample_changed=%u "
                   "C_scsp_changed=%u M_scsp_changed=%u\n",
                   packet, (unsigned)sample_diff, (unsigned)sample_first,
                   (unsigned)scsp_first, (unsigned)report.handler_address,
                   (unsigned)report.handled,
                   (unsigned)report.descriptors_matched,
                   (unsigned)c_sample_changed, (unsigned)m_sample_changed,
                   (unsigned)c_scsp_changed, (unsigned)m_scsp_changed);
        }
        for (i = 0u; i < VF2_SOUND_RAM_BYTES; ++i) {
            /* The BA run uses the EPR's original SSP=$5000.  Only
             * 68000 stack frames *below* $5000 are allowed to differ.
             * Do NOT exclude $7e00..$8000: the real BA staging copy
             * writes there and C/68000 must agree on every byte. */
            if (i >= 0x4f00u && i < 0x5000u) continue;
            if (board.sound_ram[i] != step_ram[i]) {
                if (first == VF2_SOUND_RAM_BYTES) first = i;
                ++differences;
            }
        }
        if (verify_ad21_ba) {
            size_t staging_bytes = (size_t)(
                ((uint16_t)rom[0x5b26u] << 8u) | rom[0x5b27u]) + 1u;
            int staged_equal = 1;
            int scsp_equal = memcmp(board.scsp.registers,
                                    step_scsp.registers,
                                    VF2_SCSP_REGISTER_BYTES) == 0;
            for (i = 0u; i < staging_bytes; ++i) {
                if (board.sound_ram[0x5000u + i] != rom[0x5b28u + i] ||
                    step_ram[0x5000u + i] != rom[0x5b28u + i]) {
                    staged_equal = 0;
                    break;
                }
            }
            /* Original VF2.1 selector max is 1. Both 7 and 42 are
             * out of range: the copy of staging bytes still runs,
             * but no PCM sample row is selected. Comparing unchanged
             * samples is *correct* here; earlier >0 gates were wrong. */
            {
                int equal = cs == VF2_OK && ms == VF2_OK &&
                            regs.pc == UINT32_C(0x265c) &&
                            report.handled != 0 &&
                            report.command == 0xbau &&
                            report.payload == (uint8_t)(which == 0u ? 0x2au : 7u) &&
                            report.handler_address == UINT32_C(0x13cc) &&
                            report.descriptors_matched == 1u &&
                            report.cursor_before == cursor &&
                            report.cursor_after == (uint16_t)(cursor + 4u) &&
                            board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u &&
                            step_ram[VF2_SOUND_COMMAND_COUNT] == 0u &&
                            differences == 0u && scsp_equal &&
                            staged_equal &&
                            c_sample_changed == 0u &&
                            m_sample_changed == 0u;
                printf("FRAME342_AD21_BA_STRICT packet=%u parity=%u "
                       "staged_equal=%u C_sample_changed=%u "
                       "M_sample_changed=%u\n",
                       packet, (unsigned)equal, (unsigned)staged_equal,
                       (unsigned)c_sample_changed,
                       (unsigned)m_sample_changed);
                CHECK(equal);
            }
        }
        printf("FRAME342_AD21_BA_DISPATCH packet=%u payload=%02x "
               "C_status=%d M_status=%d M_pc=%05x M_steps=%u "
               "ram_diff=%u first=%05x scsp_equal=%u "
               "C_count=%u M_count=%u diagnostic_only=1\n",
               packet, which == 0u ? 0x2au : 7u,
               (int)cs, (int)ms, (unsigned)regs.pc, (unsigned)steps,
               (unsigned)differences, (unsigned)first,
               (unsigned)(memcmp(board.scsp.registers, step_scsp.registers,
                                 VF2_SCSP_REGISTER_BYTES) == 0),
               (unsigned)board.sound_ram[VF2_SOUND_COMMAND_COUNT],
               (unsigned)step_ram[VF2_SOUND_COMMAND_COUNT]);
    }
    memcpy(board.sound_ram, c_snapshot, sizeof(c_snapshot));
    memcpy(step_ram, m_snapshot, sizeof(m_snapshot));
    memcpy(&board.scsp, &c_scsp_snapshot, sizeof(c_scsp_snapshot));
    memcpy(&step_scsp, &m_scsp_snapshot, sizeof(m_scsp_snapshot));
    (void)vf2_scsp_attach_sample_ram(&board.scsp,
              board.sound_ram, sizeof(board.sound_ram));
    (void)vf2_scsp_attach_sample_ram(&step_scsp,
              step_ram, sizeof(step_ram));
}

static void isolate_command(size_t command,
                            const uint8_t *rom, size_t rom_size,
                            const uint8_t *samples, size_t sample_size)
{
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report report;
    uint32_t steps = 0u;
    uint32_t cursor = (uint32_t)command * 4u;
    uint8_t cls;
    vf2_status c, s;
    size_t first_diff = VF2_SOUND_RAM_BYTES;
    size_t i;
    unsigned scsp_before, scsp_c, scsp_s;
    memcpy(board.sound_ram, filed_ram, VF2_SOUND_RAM_BYTES);
    memcpy(&board.scsp, &filed_scsp, sizeof(filed_scsp));
    memcpy(step_ram, filed_ram, sizeof(step_ram));
    memcpy(&step_scsp, &filed_scsp, sizeof(step_scsp));
    /* The two independently stepped SCSPs MUST read their own RAM
     * images. A shallow SCSP struct copy otherwise leaves the
     * oracle's sample-RAM pointer attached to the C board. */
    CHECK(vf2_scsp_attach_sample_ram(
        &step_scsp, step_ram, sizeof(step_ram)) == VF2_OK);
    /* C dispatch_NEXT does not advance a refused entry itself.
     * Isolating each event with count=1 avoids introducing
     * unverified previous-command game effects. */
    board.sound_ram[VF2_SOUND_COMMAND_COUNT] = 1u;
    step_ram[VF2_SOUND_COMMAND_COUNT] = 1u;
    board.sound_ram[VF2_SOUND_COMMAND_CURSOR] = (uint8_t)(cursor >> 8u);
    board.sound_ram[VF2_SOUND_COMMAND_CURSOR + 1u] = (uint8_t)cursor;
    step_ram[VF2_SOUND_COMMAND_CURSOR] = (uint8_t)(cursor >> 8u);
    step_ram[VF2_SOUND_COMMAND_CURSOR + 1u] = (uint8_t)cursor;

    scsp_before = active_slots(&board.scsp);
    cls = board.sound_ram[VF2_SOUND_COMMAND_RING_BASE + cursor];
    board.allow_verified_90_allocator = 0u;
    /* Preserve the old FAC refusal as the mandatory default;
     * then retry this one exact owner-ROM original command with
     * the explicit 9E/slot0 opt-in. The reference 68000 is always
     * run from the same untouched prestate. */
    if (command == 0u) {
        vf2_sound_dispatch_report refused = {0};
        vf2_status refusal = vf2_sound_board_dispatch_next(
            &board, &refused);
        CHECK(refusal == VF2_ERROR_UNSUPPORTED);
        CHECK(refused.handler_address == UINT32_C(0x0fac));
        CHECK(active_slots(&board.scsp) == 0u);
        memcpy(board.sound_ram, filed_ram, sizeof(filed_ram));
        memcpy(&board.scsp, &filed_scsp, sizeof(filed_scsp));
        board.sound_ram[VF2_SOUND_COMMAND_COUNT] = 1u;
        board.sound_ram[VF2_SOUND_COMMAND_CURSOR] = 0u;
        board.sound_ram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
        board.allow_verified_90_allocator = 1u;
    }
    memset(&report, 0, sizeof(report));
    c = vf2_sound_board_dispatch_next(&board, &report);
    scsp_c = active_slots(&board.scsp);

    memset(&bus, 0, sizeof(bus));
    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = rom;
    bus.rom_size = rom_size;
    bus.sample_rom = samples;
    bus.sample_rom_size = sample_size;
    bus.scsp = &step_scsp;
    vf2_m68k_regs_reset(&regs);
    regs.pc = UINT32_C(0x7a6);
    regs.a[4] = UINT32_C(0x2506);
    regs.a[5] = VF2_SCSP_CPU_BASE;
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    if (cls == UINT8_C(0x9e)) {
        uint32_t presteps = 0u;
        vf2_status pre = vf2_m68k_run(
            &bus, &regs, UINT32_C(0x0fac), 0, 20000u, &presteps);
        printf("FRAME342_PRE_FAC status=%d pc=%05x steps=%u "
               "d4=%08x a1=%08x a2=%08x a3=%08x "
               "voice0_status=%02x voice0_slot=%02x%02x "
               "table_select=%02x%02x scsp_active=%u\n",
               (int)pre, (unsigned)regs.pc, (unsigned)presteps,
               (unsigned)regs.d[4], (unsigned)regs.a[1],
               (unsigned)regs.a[2], (unsigned)regs.a[3],
               (unsigned)step_ram[VF2_SOUND_VOICE_TABLE_BASE + 2u],
               (unsigned)step_ram[VF2_SOUND_VOICE_TABLE_BASE + 0u],
               (unsigned)step_ram[VF2_SOUND_VOICE_TABLE_BASE + 1u],
               (unsigned)step_ram[VF2_SOUND_LOOKUP_WORD],
               (unsigned)step_ram[VF2_SOUND_LOOKUP_WORD + 1u],
               active_slots(&step_scsp));
        if (pre != VF2_OK) {
            s = pre;
        } else {
            uint32_t to_copy = 0u, tail_steps = 0u;
            vf2_status reached = vf2_m68k_run(
                &bus, &regs, UINT32_C(0x0cb2),
                0, 20000u, &to_copy);
            printf("FRAME342_FAC_TO_CB2 status=%d pc=%05x steps=%u "
                   "a0=%08x a1=%08x a2=%08x a3=%08x "
                   "d5=%08x voice_sample=%02x%02x%02x%02x\n",
                   (int)reached, (unsigned)regs.pc, (unsigned)to_copy,
                   (unsigned)regs.a[0], (unsigned)regs.a[1],
                   (unsigned)regs.a[2], (unsigned)regs.a[3],
                   (unsigned)regs.d[5],
                   (unsigned)step_ram[VF2_SOUND_VOICE_TABLE_BASE + 12u],
                   (unsigned)step_ram[VF2_SOUND_VOICE_TABLE_BASE + 13u],
                   (unsigned)step_ram[VF2_SOUND_VOICE_TABLE_BASE + 14u],
                   (unsigned)step_ram[VF2_SOUND_VOICE_TABLE_BASE + 15u]);
            if (reached == VF2_OK) {
                s = vf2_m68k_run(&bus, &regs, UINT32_C(0x265c),
                                  0, 20000u, &tail_steps);
                steps = presteps + to_copy + tail_steps;
                printf("FRAME342_CB2_TO_EXIT status=%d steps=%u\n",
                       (int)s, (unsigned)tail_steps);
            } else {
                s = reached;
                steps = presteps + to_copy;
            }
        }
    } else {
        s = vf2_m68k_run(&bus, &regs, UINT32_C(0x265c),
                          0, 20000u, &steps);
    }
    scsp_s = active_slots(&step_scsp);

    for (i = 0; i < VF2_SOUND_RAM_BYTES; ++i) {
        if (i >= 0x7e00u && i < 0x8000u) {
            continue; /* Original 68000 stack spills vs high-level C */
        }
        if (board.sound_ram[i] != step_ram[i]) {
            first_diff = i;
            break;
        }
    }

    printf("FRAME342_DISPATCH index=%u entry=%02x%02x%02x "
           "class=%02x C_status=%d C_handler=%05x C_handled=%d "
           "S_status=%d S_pc=%05x S_steps=%u "
           "SCSP_active_before=%u SCSP_active_C=%u SCSP_active_S=%u "
           "first_ram_diff=%s",
           (unsigned)command,
           (unsigned)board.sound_ram[VF2_SOUND_COMMAND_RING_BASE + cursor],
           (unsigned)board.sound_ram[VF2_SOUND_COMMAND_RING_BASE + cursor + 1u],
           (unsigned)board.sound_ram[VF2_SOUND_COMMAND_RING_BASE + cursor + 2u],
           (unsigned)(cls & 0xf0u), (int)c,
           (unsigned)report.handler_address, report.handled, (int)s,
           (unsigned)regs.pc, (unsigned)steps,
           scsp_before, scsp_c, scsp_s,
           first_diff == VF2_SOUND_RAM_BYTES ? "none" : "present");
    if (first_diff < VF2_SOUND_RAM_BYTES) {
        printf(" ram_addr=%05x C=%02x S=%02x", (unsigned)first_diff,
               (unsigned)board.sound_ram[first_diff],
               (unsigned)step_ram[first_diff]);
    }
    putchar('\n');
    if (command == 0u) {
        size_t differences = 0u;
        size_t first_other = VF2_SOUND_RAM_BYTES;
        for (i = 0u; i < VF2_SOUND_RAM_BYTES; ++i) {
            if (i == VF2_SOUND_COMMAND_COUNT ||
                (i >= VF2_SOUND_COMMAND_CURSOR &&
                 i <= VF2_SOUND_COMMAND_CURSOR + 1u) ||
                (i >= 0x7e00u && i < 0x8000u)) {
                continue;
            }
            if (board.sound_ram[i] != step_ram[i]) {
                if (first_other == VF2_SOUND_RAM_BYTES) first_other = i;
                if (differences < 16u) {
                    printf("FRAME342_9E_RAM_DIFF addr=%05x C=%02x S=%02x\n",
                           (unsigned)i, (unsigned)board.sound_ram[i],
                           (unsigned)step_ram[i]);
                }
                ++differences;
            }
        }
        printf("FRAME342_9E_PARITY RAM_diff_except_epilogue=%u"
               " first=%05x SCSP_C=%u SCSP_S=%u\n",
               (unsigned)differences, (unsigned)first_other,
               scsp_c, scsp_s);
        CHECK(differences == 0u);
    }
    /* These assertions capture the invariant that *no modeled
     * SCSP slot can activate without its required KEYON. Any
     * difference with the original is reported, not falsely treated
     * as an exact recovered-dispatch match. */
    CHECK(scsp_c <= VF2_SCSP_SLOT_COUNT);
    CHECK(scsp_s <= VF2_SCSP_SLOT_COUNT);
    CHECK(report.command == cls);
    CHECK(report.cursor_before == cursor);
    if (command == 0u) {
        CHECK(s == VF2_OK);
        CHECK(c == VF2_OK);
        CHECK(report.handled == 1);
        CHECK(scsp_s == 1u);
        CHECK(scsp_c == scsp_s);
        CHECK(memcmp(board.scsp.registers, step_scsp.registers,
                     VF2_SCSP_REGISTER_BYTES) == 0);
        CHECK(memcmp(board.scsp.slot_active, step_scsp.slot_active,
                     VF2_SCSP_SLOT_COUNT) == 0);
        CHECK(board.sound_ram[UINT32_C(0x2000)] == UINT8_C(0x80));
        /* The C copy path must expose the uploaded bytes through
         * the same SCSP sound-RAM map as the original 68000, rather
         * than reading unrelated offsets in the source sample ROM. */
        CHECK(board.scsp.sample_ram == board.sound_ram);
        CHECK(board.scsp.sample_ram_size == VF2_SOUND_RAM_BYTES);
        CHECK(step_scsp.sample_ram == step_ram);
        /* Both states were independently programmed by C and the
         * ORIGINAL 68000. Render the same 4096-frame software SCSP
         * model from each and compare every stereo PCM sample.
         * This is not hardware DAC/DSP accuracy. */
        {
            size_t frame;
            size_t nonzero = 0u;
            unsigned peak = 0u;
            /* Pre-render SCSP state and backing sample pointers are
             * required for a meaningful stereo differential: identical
             * registers alone are insufficient if the original
             * interpreter's sample writes went to its own RAM image.
             * Do NOT normalize or copy model state just to pass. */
            {
                uint32_t start_c = ((uint32_t)(board.scsp.registers[1u] &
                     0x0fu) << 16u) |
                    ((uint32_t)board.scsp.registers[2u] << 8u) |
                    board.scsp.registers[3u];
                uint32_t start_s = ((uint32_t)(step_scsp.registers[1u] &
                     0x0fu) << 16u) |
                    ((uint32_t)step_scsp.registers[2u] << 8u) |
                    step_scsp.registers[3u];
                printf("FRAME342_SCSP_PRE C_start=%06x S_start=%06x "
                       "C_sample_ram=%d S_sample_ram=%d "
                       "C_addr=%08x S_addr=%08x C_step=%08x S_step=%08x "
                       "C_env=%d S_env=%d C_state=%u S_state=%u "
                       "C_backward=%u S_backward=%u\n",
                       (unsigned)start_c,(unsigned)start_s,
                       board.scsp.sample_ram == board.sound_ram,
                       step_scsp.sample_ram == step_ram,
                       (unsigned)board.scsp.slot_address[0],
                       (unsigned)step_scsp.slot_address[0],
                       (unsigned)board.scsp.slot_step[0],
                       (unsigned)step_scsp.slot_step[0],
                       (int)board.scsp.slot_envelope[0],
                       (int)step_scsp.slot_envelope[0],
                       (unsigned)board.scsp.slot_envelope_state[0],
                       (unsigned)step_scsp.slot_envelope_state[0],
                       (unsigned)board.scsp.slot_backward[0],
                       (unsigned)step_scsp.slot_backward[0]);
                if (start_c < VF2_SOUND_RAM_BYTES - 8u &&
                    start_s < VF2_SOUND_RAM_BYTES - 8u) {
                    size_t k;
                    for (k = 0u; k < 8u; ++k) {
                        printf("FRAME342_START_SAMPLE offset=%u C=%02x S=%02x\n",
                               (unsigned)k,
                               (unsigned)board.sound_ram[start_c + k],
                               (unsigned)step_ram[start_s + k]);
                    }
                }
            }
            vf2_status cs = vf2_scsp_render(
                &board.scsp, frame342_c_left, frame342_c_right, 4096u);
            vf2_status ss = vf2_scsp_render(
                &step_scsp, frame342_s_left, frame342_s_right, 4096u);
            CHECK(cs == VF2_OK);
            CHECK(ss == VF2_OK);
            printf("FRAME342_SCSP_POST PCM_SLOT0 "
                   "C_addr=%08x S_addr=%08x "
                   "C_step=%08x S_step=%08x "
                   "C_env=%d S_env=%d "
                   "C_state=%u S_state=%u "
                   "C_backward=%u S_backward=%u\n",
                   (unsigned)board.scsp.slot_address[0],
                   (unsigned)step_scsp.slot_address[0],
                   (unsigned)board.scsp.slot_step[0],
                   (unsigned)step_scsp.slot_step[0],
                   (int)board.scsp.slot_envelope[0],
                   (int)step_scsp.slot_envelope[0],
                   (unsigned)board.scsp.slot_envelope_state[0],
                   (unsigned)step_scsp.slot_envelope_state[0],
                   (unsigned)board.scsp.slot_backward[0],
                   (unsigned)step_scsp.slot_backward[0]);
            if (cs == VF2_OK && ss == VF2_OK) {
                CHECK(memcmp(frame342_c_left, frame342_s_left,
                             sizeof(frame342_c_left)) == 0);
                for (frame = 0u; frame < 4096u; ++frame) {
                    if (frame342_c_right[frame] != frame342_s_right[frame]) {
                        printf("FRAME342_PCM_RIGHT_FIRST_DIFF sample=%u C=%d S=%d "
                               "C_left=%d S_left=%d\n",
                               (unsigned)frame,
                               (int)frame342_c_right[frame],
                               (int)frame342_s_right[frame],
                               (int)frame342_c_left[frame],
                               (int)frame342_s_left[frame]);
                        break;
                    }
                }
                CHECK(memcmp(frame342_c_right, frame342_s_right,
                             sizeof(frame342_c_right)) == 0);
                for (frame = 0u; frame < 4096u; ++frame) {
                    int32_t a = frame342_c_left[frame];
                    int32_t b = frame342_c_right[frame];
                    unsigned pa = (unsigned)(a < 0 ? -a : a);
                    unsigned pb = (unsigned)(b < 0 ? -b : b);
                    if (a != 0 || b != 0) ++nonzero;
                    if (pa > peak) peak = pa;
                    if (pb > peak) peak = pb;
                }
            }
            /* A byte-exact all-silence match would not verify
             * playable sample output; the owner-ROM differential
             * also requires nonzero stereo PCM after KEYON. */
            CHECK(nonzero > 0u);
            printf("FRAME342_9E_PCM status_c=%d status_s=%d "
                   "matched_stereo_samples=%u nonzero_frames=%u/4096 "
                   "peak=%u model_only=1\n",
                   (int)cs, (int)ss,
                   (unsigned)(cs == VF2_OK && ss == VF2_OK),
                   (unsigned)nonzero, peak);
        }
    }
    if (command == 4u) {
        size_t differences = 0u, first = VF2_SOUND_RAM_BYTES;
        CHECK(s == VF2_OK);
        CHECK(c == VF2_OK);
        CHECK(report.handled == 1);
        CHECK(scsp_c == scsp_s);
        CHECK(memcmp(board.scsp.registers, step_scsp.registers,
                     VF2_SCSP_REGISTER_BYTES) == 0);
        for (i = 0u; i < VF2_SOUND_RAM_BYTES; ++i) {
            if (i >= 0x7e00u && i < 0x8000u) continue;
            if (board.sound_ram[i] != step_ram[i]) {
                if (first == VF2_SOUND_RAM_BYTES) first = i;
                if (differences < 16u) {
                    printf("FRAME342_AD21_RAM_DIFF addr=%05x C=%02x S=%02x\n",
                           (unsigned)i, (unsigned)board.sound_ram[i],
                           (unsigned)step_ram[i]);
                }
                ++differences;
            }
        }
        printf("FRAME342_AD21_C68000_PARITY differences=%u first=%05x "
               "C_handler=%05x S_steps=%u\n",
               (unsigned)differences, (unsigned)first,
               (unsigned)report.handler_address, (unsigned)steps);
        CHECK(differences == 0u);
        if (probe_ad21_l2) {
            vf2_m68k_regs tick;
            vf2_sound_stream_maintenance_report maintenance;
            vf2_status c_tick, m_tick;
            uint32_t tick_steps = 0u;
            uint8_t *descriptor = board.sound_ram +
                VF2_SOUND_DESCRIPTOR_BASE +
                VF2_SOUND_STREAM_DESCRIPTOR_BYTES;
            uint32_t next = ((uint32_t)descriptor[4] << 24u) |
                ((uint32_t)descriptor[5] << 16u) |
                ((uint32_t)descriptor[6] << 8u) |
                (uint32_t)descriptor[7];
            printf("FRAME342_AD21_POST_DISPATCH stream_slot1=%02x%02x%02x%02x "
                   "next=%08x rom_next_bytes=",
                   descriptor[0], descriptor[1],
                   descriptor[2], descriptor[3], (unsigned)next);
            if (next >= VF2_SOUND_AUDIO_ROM_BASE &&
                next < VF2_SOUND_AUDIO_ROM_BASE + rom_size &&
                (uint64_t)(next - VF2_SOUND_AUDIO_ROM_BASE) + 16u <= rom_size) {
                size_t j;
                for (j = 0; j < 16u; ++j) {
                    printf("%02x", rom[next - VF2_SOUND_AUDIO_ROM_BASE + j]);
                }
            } else {
                printf("out_of_range");
            }
            putchar('\n');
            memset(&maintenance, 0, sizeof(maintenance));
            c_tick = vf2_sound_board_maintain_streams(&board, &maintenance);
            vf2_m68k_regs_reset(&tick);
            tick.pc = UINT32_C(0x1f16);
            tick.a[4] = UINT32_C(0x2506);
            tick.a[5] = VF2_SCSP_CPU_BASE;
            tick.a[6] = UINT32_C(0x1000);
            tick.a[7] = UINT32_C(0x7f00);
            m_tick = vf2_m68k_run(&bus, &tick, UINT32_C(0x1f7a),
                                   0, 10000u, &tick_steps);
            printf("FRAME342_AD21_L2_PROBE C_status=%d C_refusal=%u "
                   "C_refusal_record=%u C_count=%u C_stream_count=%u "
                   "M_status=%d M_pc=%05x M_steps=%u "
                   "M_count=%u M_stream_count=%u "
                   "M_d0=%08x M_d1=%08x M_a2=%08x\n",
                   (int)c_tick, (unsigned)maintenance.refused_stage,
                   (unsigned)maintenance.refused_record,
                   (unsigned)board.sound_ram[VF2_SOUND_COMMAND_COUNT],
                   (unsigned)board.sound_ram[VF2_SOUND_DESCRIPTOR_BASE +
                             VF2_SOUND_STREAM_DESCRIPTOR_BYTES + 3u],
                   (int)m_tick, (unsigned)tick.pc, (unsigned)tick_steps,
                   (unsigned)step_ram[VF2_SOUND_COMMAND_COUNT],
                   (unsigned)step_ram[VF2_SOUND_DESCRIPTOR_BASE +
                             VF2_SOUND_STREAM_DESCRIPTOR_BYTES + 3u],
                   (unsigned)tick.d[0], (unsigned)tick.d[1],
                   (unsigned)tick.a[2]);
            {
                uint16_t end = (uint16_t)(
                    ((uint16_t)step_ram[VF2_SOUND_COMMAND_WRITE_CURSOR] << 8u) |
                     step_ram[VF2_SOUND_COMMAND_WRITE_CURSOR + 1u]);
                uint16_t start = (uint16_t)(
                    ((uint16_t)step_ram[VF2_SOUND_COMMAND_CURSOR] << 8u) |
                     step_ram[VF2_SOUND_COMMAND_CURSOR + 1u]);
                size_t packet, k;
                printf("FRAME342_AD21_L2_ORIGINAL_RING write_cursor=%04x "
                       "read_cursor=%04x count=%u ", end, start,
                       (unsigned)step_ram[VF2_SOUND_COMMAND_COUNT]);
                for (packet = 0; packet < 12u; ++packet) {
                    uint16_t at = (uint16_t)((start + packet * 4u) & 0xfffu);
                    printf("%s", packet != 0u ? " " : "");
                    for (k = 0; k < 4u; ++k) {
                        printf("%02x", (unsigned)step_ram[
                            VF2_SOUND_COMMAND_RING_BASE + ((at + k) & 0xfffu)]);
                    }
                }
                putchar('\n');
                printf("FRAME342_AD21_L2_ORIGINAL_DESCRIPTOR ");
                for (k = 0; k < VF2_SOUND_STREAM_DESCRIPTOR_BYTES; ++k) {
                    printf("%02x", (unsigned)step_ram[
                        VF2_SOUND_DESCRIPTOR_BASE +
                        VF2_SOUND_STREAM_DESCRIPTOR_BYTES + k]);
                }
                putchar('\n');
            }
            {
                size_t byte, l2_differences = 0u;
                size_t l2_first = VF2_SOUND_RAM_BYTES;
                for (byte = 0u; byte < VF2_SOUND_RAM_BYTES; ++byte) {
                    if (byte >= 0x7e00u && byte < 0x8000u) {
                        continue; /* Original interpreter's stack */
                    }
                    if (board.sound_ram[byte] != step_ram[byte]) {
                        if (l2_first == VF2_SOUND_RAM_BYTES) l2_first = byte;
                        if (l2_differences < 16u) {
                            printf("FRAME342_AD21_L2_DIFF addr=%05x C=%02x S=%02x\n",
                                   (unsigned)byte,
                                   (unsigned)board.sound_ram[byte],
                                   (unsigned)step_ram[byte]);
                        }
                        ++l2_differences;
                    }
                }
                printf("FRAME342_AD21_L2_PARITY diff=%u l2_first=%05x "
                       "scsp_regs_equal=%u c_packets=%u c_cursor=%04x\n",
                       (unsigned)l2_differences, (unsigned)l2_first,
                       (unsigned)(memcmp(board.scsp.registers,
                             step_scsp.registers,
                             VF2_SCSP_REGISTER_BYTES) == 0),
                       (unsigned)maintenance.packets_emitted,
                       (unsigned)maintenance.write_cursor_after);
                CHECK(c_tick == VF2_OK);
                CHECK(m_tick == VF2_OK);
                CHECK(tick.pc == UINT32_C(0x1f7a));
                CHECK(maintenance.packets_emitted == 12u);
                CHECK(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 12u);
                CHECK(step_ram[VF2_SOUND_COMMAND_COUNT] == 12u);
                CHECK(maintenance.write_cursor_after == 0x44u);
                CHECK(l2_differences == 0u);
                CHECK(memcmp(board.scsp.registers, step_scsp.registers,
                             VF2_SCSP_REGISTER_BYTES) == 0);
                probe_ad21_ba_dispatch(rom, rom_size, samples, sample_size);
            }
        }
    }
    if (command >= 1u && command <= 3u) {
        size_t descriptor = VF2_SOUND_COMMAND_DESCRIPTOR_BASE +
            (size_t)(cls & UINT8_C(0x0f)) *
            VF2_SOUND_COMMAND_DESCRIPTOR_BYTES + 2u;
        CHECK(board.sound_ram[descriptor] == step_ram[descriptor]);
    }
}

int main(int argc, char **argv)
{
    uint8_t *rom = NULL;
    uint8_t *samples = NULL;
    size_t rom_size = 0u, sample_size = 0u;
    size_t i;
    if ((argc != 2 && argc != 3) ||
        (argc == 3 && strcmp(argv[2], "--probe-ad21-l2") != 0 &&
         strcmp(argv[2], "--verify-ad21-ba") != 0)) {
        fprintf(stderr,
                "usage: %s <original-vf2-rom-directory> "
                "[--probe-ad21-l2|--verify-ad21-ba]\n",
                argv[0]);
        return 2;
    }
    probe_ad21_l2 = argc == 3;
    verify_ad21_ba = argc == 3 &&
                     strcmp(argv[2], "--verify-ad21-ba") == 0;
    if (vf2_romset_build_region(argv[1], VF2_REGION_AUDIOCPU,
                                &rom, &rom_size) != VF2_OK ||
        vf2_romset_build_region(argv[1], VF2_REGION_SAMPLES,
                                &samples, &sample_size) != VF2_OK ||
        rom == NULL || samples == NULL) {
        fprintf(stderr, "frame342: original sound ROM/sample bank unavailable\n");
        free(rom);
        free(samples);
        return 2;
    }
    if (!file_stream(rom, rom_size, samples, sample_size)) {
        free(rom);
        free(samples);
        return 1;
    }
    for (i = 0u; i < 5u; ++i) {
        isolate_command(i, rom, rom_size, samples, sample_size);
    }
    free(rom);
    free(samples);
    return failures == 0 ? 0 : 1;
}
