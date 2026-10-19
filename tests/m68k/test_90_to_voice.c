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

static int run_case(const uint8_t *rom, size_t rom_size,
                    uint8_t cmd, uint8_t payload, uint8_t second,
                    uint8_t mode, uint16_t word, uint8_t loopb)
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
    return 1;
}

int main(int argc, char **argv)
{
    uint8_t *rom = NULL;
    uint8_t *samples = NULL;
    size_t size = 0u, samples_size = 0u;
    int ok;
    if (argc != 2) {
        fprintf(stderr, "usage: %s /path/to/original/vf2/ROMs\n", argv[0]);
        return 2;
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
                  0xfdu, 0x0242u, 0xd6u);
    ok = run_case(rom, size, 0x9au, 0x01u, 0x7fu,
                  0xffu, 0x0261u, 0x81u) && ok;
    free(rom);
    free(samples);
    return ok ? 0 : 1;
}
