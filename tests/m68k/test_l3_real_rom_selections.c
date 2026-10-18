#include "vf2/m68k_step.h"
#include "vf2/model2a.h"
#include "vf2/rom.h"
#include "vf2/sound_board.h"
#include "vf2/status.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Real ROM-backed selections, not claimed gameplay trigger events.
 * Compare the recovered C L3 UART receiver against the 68000 stepper
 * after EACH byte. The independent pre-state is copied before either runs.
 * No command is injected into the i960 gameplay producer.
 */
enum { L3_STEP_LIMIT = 20000u };
static vf2_sound_board board;
static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
static vf2_scsp step_scsp;

static uint32_t le32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8u) |
           ((uint32_t)bytes[2] << 16u) |
           ((uint32_t)bytes[3] << 24u);
}

static int table_command(const uint8_t *data, size_t size,
                         uint32_t profile, uint32_t column,
                         uint32_t *command)
{
    const size_t table = 0x1e44cu;
    uint32_t row_pointer = 0u;
    size_t row = 0u;

    if (data == NULL || command == NULL || profile == 0u ||
        profile > 18u || column >= 11u ||
        size < table + 19u * 4u ||
        le32(data + table) != 0u) {
        return 0;
    }
    row_pointer = le32(data + table + (size_t)profile * 4u);
    if (row_pointer < VF2_MAIN_DATA_BASE ||
        (row_pointer & UINT32_C(3)) != 0u) {
        return 0;
    }
    row = (size_t)(row_pointer - VF2_MAIN_DATA_BASE) +
          (size_t)column * 4u;
    if (row > size || size - row < 4u) {
        return 0;
    }
    *command = le32(data + row);
    return *command != 0u &&
           (*command >> 8u == UINT32_C(0x00ad23) ||
            *command >> 8u == UINT32_C(0x00ad24));
}

static int compare_ram(const uint8_t *a, const uint8_t *b, size_t byte_index)
{
    size_t i = 0u;
    for (i = 0u; i < VF2_SOUND_RAM_BYTES; ++i) {
        /* The 68000 JSR to 0x49e spills its return address here;
         * the C routine intentionally does not simulate stack traffic. */
        if (i >= 0x7efcu && i < 0x7f00u) {
            continue;
        }
        if (a[i] != b[i]) {
            fprintf(stderr,
                    "L3 byte %u: first RAM mismatch @0x%05x C=%02x stepper=%02x\n",
                    (unsigned)byte_index, (unsigned)i,
                    (unsigned)a[i], (unsigned)b[i]);
            return 0;
        }
    }
    return 1;
}

static int check_one(uint32_t command, const uint8_t *rom, size_t rom_size)
{
    const uint8_t bytes[3] = {
        (uint8_t)(command >> 16u),
        (uint8_t)(command >> 8u),
        (uint8_t)command
    };
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    uint32_t steps = 0u;
    size_t i = 0u;

    if (vf2_sound_board_initialize(&board, rom, rom_size, NULL, 0u) !=
        VF2_OK) {
        fprintf(stderr, "L3 initialization failed\n");
        return 0;
    }
    memcpy(step_ram, board.sound_ram, sizeof(step_ram));
    memcpy(&step_scsp, &board.scsp, sizeof(step_scsp));
    memset(&bus, 0, sizeof(bus));
    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = rom;
    bus.rom_size = rom_size;
    bus.scsp = &step_scsp;

    for (i = 0u; i < 3u; ++i) {
        uint8_t *ram = board.sound_ram;
        vf2_status c_status = vf2_sound_board_file_uart_byte(&board, bytes[i]);
        vf2_status s_status = VF2_OK;

        vf2_m68k_regs_reset(&regs);
        regs.d[0] = bytes[i];
        regs.a[6] = UINT32_C(0x1000);
        regs.a[7] = UINT32_C(0x7f00);
        regs.pc = UINT32_C(0x52e);
        bus.event_count = 0u;
        s_status = vf2_m68k_run(&bus, &regs, UINT32_C(0x5ea),
                                0, L3_STEP_LIMIT, &steps);
        if (c_status != VF2_OK || s_status != VF2_OK ||
            regs.pc != UINT32_C(0x5ea) ||
            !compare_ram(ram, step_ram, i) ||
            memcmp(&board.scsp, &step_scsp, sizeof(step_scsp)) != 0) {
            fprintf(stderr,
                    "L3 command %08x byte[%u]=%02x: C=%d S=%d pc=%05x steps=%u\n",
                    (unsigned)command, (unsigned)i, (unsigned)bytes[i],
                    (int)c_status, (int)s_status, (unsigned)regs.pc,
                    (unsigned)steps);
            return 0;
        }
        /* The receiver only assembles the ring after all three bytes. */
        if (ram[VF2_SOUND_COMMAND_COUNT] != (uint8_t)(i == 2u) ||
            bus.event_count != (size_t)(i == 2u)) {
            fprintf(stderr, "L3 %08x byte %u: early/late ring file or poke\n",
                    (unsigned)command, (unsigned)i);
            return 0;
        }
        if (i == 2u &&
            (bus.events[0].address != UINT32_C(0x400001) ||
             bus.events[0].is_write == 0u)) {
            fprintf(stderr, "L3 %08x: missing final hardware poke\n",
                    (unsigned)command);
            return 0;
        }
    }
    if (board.sound_ram[VF2_SOUND_COMMAND_RING_BASE] != bytes[0] ||
        board.sound_ram[VF2_SOUND_COMMAND_RING_BASE + 1u] != bytes[1] ||
        board.sound_ram[VF2_SOUND_COMMAND_RING_BASE + 2u] != bytes[2] ||
        board.sound_ram[VF2_SOUND_COMMAND_RING_BASE + 3u] != 0u ||
        board.sound_ram[VF2_SOUND_COMMAND_WRITE_CURSOR] != 0u ||
        board.sound_ram[VF2_SOUND_COMMAND_WRITE_CURSOR + 1u] != 4u ||
        board.sound_ram[VF2_SOUND_COMMAND_COUNT] != 1u) {
        fprintf(stderr, "L3 %08x: resulting four-byte command ring differs\n",
                (unsigned)command);
        return 0;
    }
    fprintf(stderr, "MATCH C/68000 L3 %08x: %02x %02x %02x, ring count 1\n",
            (unsigned)command, (unsigned)bytes[0],
            (unsigned)bytes[1], (unsigned)bytes[2]);
    return 1;
}

int main(int argc, char **argv)
{
    static const struct {
        uint32_t profile, column, expected;
    } legs[] = {
        { 1u, 5u, UINT32_C(0x00ad2342) },
        { 9u, 0u, UINT32_C(0x00ad2401) }
    };
    uint8_t *audio = NULL;
    uint8_t *data = NULL;
    size_t audio_size = 0u, data_size = 0u;
    size_t i = 0u;
    int valid = 1;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <vf2-rom-directory>\n", argv[0]);
        return 2;
    }
    if (vf2_romset_build_region(argv[1], VF2_REGION_AUDIOCPU,
                                &audio, &audio_size) != VF2_OK ||
        vf2_romset_build_region(argv[1], VF2_REGION_MAIN_DATA,
                                &data, &data_size) != VF2_OK ||
        audio == NULL || data == NULL) {
        fprintf(stderr, "cannot load verified sound/main-data regions\n");
        free(audio);
        free(data);
        return 2;
    }
    for (i = 0u; i < sizeof(legs) / sizeof(legs[0]); ++i) {
        uint32_t command = 0u;
        if (!table_command(data, data_size, legs[i].profile,
                           legs[i].column, &command) ||
            command != legs[i].expected ||
            !check_one(command, audio, audio_size)) {
            fprintf(stderr, "FAIL: profile %u column %u, got %08x\n",
                    (unsigned)legs[i].profile,
                    (unsigned)legs[i].column, (unsigned)command);
            valid = 0;
        }
    }
    free(audio);
    free(data);
    return valid ? 0 : 1;
}
