#include "vf2/m68k_step.h"
#include "vf2/rom.h"
#include "vf2/sound_board.h"
#include "vf2/status.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Requires owner-local VF2 ROMs; original 68000 from 0x7a6
 * to 0xfac, then direct BRA to recovered voice author at
 * 0xa8c. No model writes are injected into the reference.
 * Does NOT claim gameplay reachability or audible SCSP key-on.
 */
static vf2_sound_board board;
static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
static vf2_scsp step_scsp;
static const uint8_t *sample_bank;
static size_t sample_bank_size;

static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24u) |
           ((uint32_t)p[1] << 16u) |
           ((uint32_t)p[2] << 8u) |
           (uint32_t)p[3];
}

/* WAV is a derivative of the owner's local ROM and is never an
 * artifact committed to this repository or uploaded by CI. Explicit
 * --wav-dir only; keep default and --probe-engine tests read-only. */
static void little16(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)value;
    dst[1] = (uint8_t)(value >> 8u);
}

static void little32(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)value;
    dst[1] = (uint8_t)(value >> 8u);
    dst[2] = (uint8_t)(value >> 16u);
    dst[3] = (uint8_t)(value >> 24u);
}

static int write_wav(const char *directory, uint8_t cmd,
                     const int16_t *left, const int16_t *right,
                     size_t frames)
{
    uint8_t header[44] = {0u};
    char destination[1024];
    const char *filename =
        cmd == 0x9bu ? "vf2_ad23_9b4278_scsp_model.wav" :
                       "vf2_ad24_9a017f_scsp_model.wav";
    FILE *out;
    size_t i;
    int length;
    if (directory == NULL || left == NULL || right == NULL ||
        frames > (size_t)(UINT32_MAX / 4u)) {
        return 0;
    }
    length = snprintf(destination, sizeof(destination), "%s/%s",
                      directory, filename);
    if (length < 0 || (size_t)length >= sizeof(destination)) {
        return 0;
    }
    out = fopen(destination, "wb");
    if (out == NULL) {
        fprintf(stderr, "Cannot create WAV %s\n", destination);
        return 0;
    }
    memcpy(header, "RIFF", 4u);
    little32(header + 4u, UINT32_C(36) + (uint32_t)frames * 4u);
    memcpy(header + 8u, "WAVEfmt ", 8u);
    little32(header + 16u, 16u);
    little16(header + 20u, 1u);       /* signed integer PCM */
    little16(header + 22u, 2u);       /* stereo */
    little32(header + 24u, 44100u);  /* SCSP native sample rate */
    little32(header + 28u, 44100u * 4u);
    little16(header + 32u, 4u);
    little16(header + 34u, 16u);
    memcpy(header + 36u, "data", 4u);
    little32(header + 40u, (uint32_t)frames * 4u);
    if (fwrite(header, 1u, sizeof(header), out) != sizeof(header)) {
        (void)fclose(out);
        return 0;
    }
    for (i = 0u; i < frames; ++i) {
        uint8_t sample[4];
        little16(sample, (uint16_t)left[i]);
        little16(sample + 2u, (uint16_t)right[i]);
        if (fwrite(sample, 1u, 4u, out) != 4u) {
            (void)fclose(out);
            return 0;
        }
    }
    if (fclose(out) != 0) {
        return 0;
    }
    printf("WAV saved: %s (%lu frames, 44.1 kHz stereo PCM)\n",
           destination, (unsigned long)frames);
    return 1;
}

static int run_case(const uint8_t *rom, size_t rom_size,
                    uint8_t cmd, uint8_t payload, uint8_t second,
                    uint8_t mode, uint16_t word, uint8_t loopb,
                    int probe_engine, const char *wav_dir)
{
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report report;
    vf2_status c, s;
    uint32_t steps = 0u;
    size_t i;
    static const uint32_t filed[] = {
        VF2_SOUND_LOOKUP_CURRENT, VF2_SOUND_LOOKUP_MODE,
        VF2_SOUND_LOOKUP_CONTROL, VF2_SOUND_LOOKUP_WORD_ALT,
        VF2_SOUND_LOOKUP_WORD_ALT + 1u, VF2_SOUND_LOOKUP_WORD,
        VF2_SOUND_LOOKUP_WORD + 1u, VF2_SOUND_LOOKUP_RELEASE,
        VF2_SOUND_LOOKUP_BYTE,
    };
    if (vf2_sound_board_initialize(&board, rom, rom_size,
                                   sample_bank, sample_bank_size) != VF2_OK) {
        fprintf(stderr, "A0 90 followup: boot failed\n");
        return 0;
    }
    board.sound_ram[VF2_SOUND_COMMAND_RING_BASE] = cmd;
    board.sound_ram[VF2_SOUND_COMMAND_RING_BASE + 1u] = payload;
    board.sound_ram[VF2_SOUND_COMMAND_RING_BASE + 2u] = second;
    board.sound_ram[VF2_SOUND_COMMAND_RING_BASE + 3u] = 0u;
    board.sound_ram[VF2_SOUND_COMMAND_COUNT] = 1u;
    board.sound_ram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    board.sound_ram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    if (board.sound_ram[VF2_SOUND_VOICE_TABLE_BASE + 2u] != 0u ||
        be32(board.sound_ram + UINT32_C(0x251a)) != UINT32_C(0x2800)) {
        fprintf(stderr, "A0 90 followup: unexpected boot voice state\n");
        return 0;
    }
    memcpy(step_ram, board.sound_ram, sizeof(step_ram));
    memcpy(&step_scsp, &board.scsp, sizeof(step_scsp));
    memset(&report, 0, sizeof(report));
    c = vf2_sound_board_dispatch_next(&board, &report);
    if (c != VF2_ERROR_UNSUPPORTED ||
        report.handler_address != UINT32_C(0xfac) ||
        report.selected_value != mode ||
        report.descriptors_matched != 1u ||
        report.cursor_before != 0u || report.cursor_after != 0u ||
        board.sound_ram[VF2_SOUND_LOOKUP_CURRENT] != payload ||
        board.sound_ram[VF2_SOUND_LOOKUP_MODE] != mode ||
        board.sound_ram[VF2_SOUND_LOOKUP_WORD] != (uint8_t)(word >> 8u) ||
        board.sound_ram[VF2_SOUND_LOOKUP_WORD + 1u] != (uint8_t)word ||
        board.sound_ram[VF2_SOUND_LOOKUP_BYTE] != loopb ||
        be32(board.sound_ram + UINT32_C(0x251a)) != UINT32_C(0x2810)) {
        fprintf(stderr, "A0 90 %02x: C did not reach known 0xfac boundary\n",
                (unsigned)cmd);
        return 0;
    }
    memset(&bus, 0, sizeof(bus));
    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = rom;
    bus.rom_size = rom_size;
    bus.sample_rom = sample_bank;
    bus.sample_rom_size = sample_bank_size;
    bus.scsp = &step_scsp;
    vf2_m68k_regs_reset(&regs);
    regs.a[4] = UINT32_C(0x2506);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x7a6);
    s = vf2_m68k_run(&bus, &regs, UINT32_C(0xfac),
                     0, 20000u, &steps);
    if (s != VF2_OK || regs.pc != UINT32_C(0xfac) ||
        regs.a[1] != UINT32_C(0x2800) ||
        be32(step_ram + UINT32_C(0x251a)) != UINT32_C(0x2810)) {
        fprintf(stderr, "A0 90 %02x: original not at 0xfac: status=%d pc=%x a1=%x steps=%u\n",
                (unsigned)cmd, (int)s, (unsigned)regs.pc,
                (unsigned)regs.a[1], (unsigned)steps);
        return 0;
    }
    for (i = 0u; i < sizeof(filed)/sizeof(filed[0]); ++i) {
        if (board.sound_ram[filed[i]] != step_ram[filed[i]]) {
            fprintf(stderr, "A0 90 %02x: first C/68000 RAM mismatch @%x C=%02x S=%02x\n",
                    (unsigned)cmd, (unsigned)filed[i],
                    (unsigned)board.sound_ram[filed[i]],
                    (unsigned)step_ram[filed[i]]);
            return 0;
        }
    }
    s = vf2_m68k_run(&bus, &regs, UINT32_C(0xa8c),
                     0, 3u, &steps);
    if (s != VF2_OK || regs.pc != UINT32_C(0xa8c)) {
        fprintf(stderr, "A0 90 %02x: expected original 0xfac BRA -> 0xa8c\n",
                (unsigned)cmd);
        return 0;
    }
    s = vf2_m68k_run(&bus, &regs, UINT32_C(0xa92),
                     0, 3u, &steps);
    if (s != VF2_OK || regs.pc != UINT32_C(0xa92) ||
        step_ram[VF2_SOUND_VOICE_TABLE_BASE + 2u] != 0x40u) {
        fprintf(stderr, "A0 90 %02x: original 0xa8c voice status store absent\n",
                (unsigned)cmd);
        return 0;
    }
    printf("A0 90 %02x: original 0xfac->0xa8c, first voice +2=0x40; C held at 0xfac\n",
           (unsigned)cmd);
    if (probe_engine) {
        /* Optional RESEARCH PROBE: original CPU only. Report its
         * next reached frontier, without promoting unobserved
         * engine, key-on or audible events into recovered C. */
        s = vf2_m68k_run(&bus, &regs, UINT32_C(0xc68),
                         0, 50000u, &steps);
        printf("A0 90 %02x optional original engine probe: status=%d pc=0x%05x "
               "steps=%u voice0+2=%02x voice0+3=%02x "
               "SCSP[0]=%02x SCSP[0x0b]=%02x\n",
               (unsigned)cmd, (int)s, (unsigned)regs.pc, (unsigned)steps,
               (unsigned)step_ram[VF2_SOUND_VOICE_TABLE_BASE + 2u],
               (unsigned)step_ram[VF2_SOUND_VOICE_TABLE_BASE + 3u],
               (unsigned)step_scsp.registers[0],
               (unsigned)step_scsp.registers[0x0bu]);
        if (s == VF2_OK && regs.pc == UINT32_C(0xc68)) {
            const size_t at = VF2_SOUND_VOICE_TABLE_BASE;
            uint16_t select = (uint16_t)(((uint16_t)step_ram[at + 6u] << 8u) |
                                         step_ram[at + 7u]);
            uint32_t table = UINT32_C(0x5000) +
                ((uint32_t)(select & UINT16_C(0x07ff)) * UINT32_C(20));
            uint32_t t0 = be32(step_ram + table);
            uint32_t t12 = be32(step_ram + table + 12u);
            uint32_t target = (t0 & UINT32_C(0x80000000)) != 0u ?
                              UINT32_C(0xf10) :
                              (t12 != 0u ? UINT32_C(0xdf8) : UINT32_C(0xcb2));
            printf("A0 90 %02x original table: select=%04x entry=%05x "
                   "gate[0]=%08x gate[12]=%08x next_stop=%05x\n",
                   (unsigned)cmd, (unsigned)select, (unsigned)table,
                   (unsigned)t0, (unsigned)t12, (unsigned)target);
            s = vf2_m68k_run(&bus, &regs, target, 0, 20000u, &steps);
            printf("A0 90 %02x engine continuation: status=%d pc=%05x "
                   "steps=%u SCSP slot0[0]=%02x slot0[1]=%02x "
                   "sound stream[0x2000]=%02x\n",
                   (unsigned)cmd, (int)s, (unsigned)regs.pc,
                   (unsigned)steps,
                   (unsigned)step_scsp.registers[0],
                   (unsigned)step_scsp.registers[1],
                   (unsigned)step_ram[0x2000u]);
            if (s == VF2_OK && regs.pc == UINT32_C(0xcb2)) {
                uint32_t extra = 0u;
                /* 0x48D2 is legal MOVEM reg->(An) per Motorola PRM.
                 * Probe its ORIGINAL bus address instead of assuming
                 * an illegal instruction at PC=0xCB6. */
                printf("A0 90 %02x before copy: A0=%08x A2=%08x "
                       "D0=%08x D7=%08x\n",
                       (unsigned)cmd, (unsigned)regs.a[0],
                       (unsigned)regs.a[2], (unsigned)regs.d[0],
                       (unsigned)regs.d[7]);
                s = vf2_m68k_run(&bus, &regs, UINT32_C(0xffffff),
                                 0, 1200u, &extra);
                printf("A0 90 %02x post-copy: status=%d pc=%05x "
                       "steps=%u A2=%08x SCSP-active0=%u "
                       "SCSP[0]=%02x stream[0x2000]=%02x\n",
                       (unsigned)cmd, (int)s, (unsigned)regs.pc,
                       (unsigned)extra, (unsigned)regs.a[2],
                       (unsigned)step_scsp.slot_active[0],
                       (unsigned)step_scsp.registers[0],
                       (unsigned)step_ram[0x2000u]);
                if (step_scsp.slot_active[0] != 0u) {
                    static int16_t left[22050], right[22050];
                    const size_t frames = wav_dir != NULL ? 22050u : 4096u;
                    size_t sample;
                    unsigned peak = 0u;
                    size_t nonzero = 0u;
                    vf2_status render_status =
                        vf2_scsp_render(&step_scsp, left, right, frames);
                    if (render_status == VF2_OK) {
                        for (sample = 0u; sample < frames; ++sample) {
                            int l = (int)left[sample];
                            int r = (int)right[sample];
                            unsigned la = (unsigned)(l < 0 ? -l : l);
                            unsigned ra = (unsigned)(r < 0 ? -r : r);
                            if (la > peak) peak = la;
                            if (ra > peak) peak = ra;
                            if (l != 0 || r != 0) ++nonzero;
                        }
                    }
                    printf("A0 90 %02x SCSP model PCM after real 68000 "
                           "keyon: render_status=%d peak=%u "
                           "nonzero_stereo_frames=%u/%lu\n",
                           (unsigned)cmd, (int)render_status,
                           peak, (unsigned)nonzero,
                           (unsigned long)frames);
                    if (wav_dir != NULL &&
                        (render_status != VF2_OK ||
                         !write_wav(wav_dir, cmd, left, right, frames))) {
                        return 0;
                    }
                }
            }
        }
    }
    return 1;
}

int main(int argc, char **argv)
{
    uint8_t *rom = NULL;
    uint8_t *samples = NULL;
    size_t size = 0u, samples_size = 0u;
    int ok;
    int probe_engine;
    const char *wav_dir = NULL;
    if ((argc != 2 && argc != 3 && argc != 4) ||
        (argc == 3 && strcmp(argv[2], "--probe-engine") != 0) ||
        (argc == 4 && strcmp(argv[2], "--wav-dir") != 0)) {
        fprintf(stderr,
                "usage: %s /path/to/original/vf2/ROMs "
                "[--probe-engine | --wav-dir DIRECTORY]\n", argv[0]);
        return 2;
    }
    probe_engine = argc >= 3;
    if (argc == 4) {
        wav_dir = argv[3];
    }
    if (vf2_romset_build_region(argv[1], VF2_REGION_AUDIOCPU,
                                &rom, &size) != VF2_OK ||
        vf2_romset_build_region(argv[1], VF2_REGION_SAMPLES,
                                &samples, &samples_size) != VF2_OK ||
        rom == NULL || samples == NULL) {
        fprintf(stderr, "A0 90: failed to reconstruct owner-local audio ROM\n");
        free(rom);
        free(samples);
        return 2;
    }
    sample_bank = samples;
    sample_bank_size = samples_size;
    ok = run_case(rom, size, 0x9bu, 0x42u, 0x78u,
                  0xfdu, 0x0242u, 0xd6u, probe_engine, wav_dir);
    ok = run_case(rom, size, 0x9au, 0x01u, 0x7fu,
                  0xffu, 0x0261u, 0x81u, probe_engine, wav_dir) && ok;
    free(rom);
    free(samples);
    return ok ? 0 : 1;
}
