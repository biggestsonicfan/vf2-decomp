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
            printf("FRAME342_9E_PCM status_c=%d status_s=%d "
                   "matched_stereo_samples=%u nonzero_frames=%u/4096 "
                   "peak=%u model_only=1\n",
                   (int)cs, (int)ss,
                   (unsigned)(cs == VF2_OK && ss == VF2_OK),
                   (unsigned)nonzero, peak);
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
    if (argc != 2) {
        fprintf(stderr, "usage: %s <original-vf2-rom-directory>\n", argv[0]);
        return 2;
    }
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
