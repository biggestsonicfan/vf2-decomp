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
static const uint8_t *sample_bank;
static size_t sample_bank_size;

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

static uint32_t read_be32(const uint8_t *source)
{
    return ((uint32_t)source[0] << 24u) |
           ((uint32_t)source[1] << 16u) |
           ((uint32_t)source[2] << 8u) | (uint32_t)source[3];
}

/* A0 is not recovered past 0x1e98. Pin precisely that frontier on
 * two authentic ROM command selectors, with one controlled descriptor
 * matching channel 0xd. The interpreter stops BEFORE the unknown body.
 */
/* Immediately after A0 initialized one real-ROM sound descriptor,
 * L2 is the next blocker: count +2 is 1, which the recovered C
 * stream worker intentionally refuses BEFORE mutation. Independently
 * step the original 68000 countdown instructions to PC 0x1f92,
 * where the still-unrecovered worker consumes the record stream.
 */
static int check_l2_handoff(uint32_t command, const uint8_t *rom,
                            size_t rom_size)
{
    vf2_sound_stream_maintenance_report tick;
    vf2_status cstatus, sstatus;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    uint32_t steps = 0u;
    const uint32_t active = VF2_SOUND_DESCRIPTOR_BASE +
                            UINT32_C(3) * VF2_SOUND_STREAM_DESCRIPTOR_BYTES;
    uint8_t *ram = board.sound_ram;
    const uint8_t *packets = command == UINT32_C(0x00ad2342) ?
                            (const uint8_t *)"\xbb\x21\x06\x9b\x42\x78" :
                            (const uint8_t *)"\xba\x21\x07\x9a\x01\x7f";
    size_t i;
    /* The three incoming L3 bytes already advanced this sound
     * board's write cursor by four. Never assume ring offset zero. */
    const uint16_t before = (uint16_t)(
        ((uint16_t)ram[VF2_SOUND_COMMAND_WRITE_CURSOR] << 8u) |
        ram[VF2_SOUND_COMMAND_WRITE_CURSOR + 1u]);
    const uint16_t after = (uint16_t)((before + 8u) & 0x0fffu);

    memcpy(step_ram, ram, sizeof(step_ram));
    memcpy(&step_scsp, &board.scsp, sizeof(step_scsp));
    memset(&tick, 0, sizeof(tick));
    cstatus = vf2_sound_board_maintain_streams(&board, &tick);
    if (cstatus != VF2_OK || tick.packets_emitted != 2u ||
        tick.bytes_consumed != 10u || tick.refused_stage != 0u ||
        ram[VF2_SOUND_COMMAND_COUNT] != 2u ||
        ram[VF2_SOUND_COMMAND_WRITE_CURSOR] != (uint8_t)(after >> 8u) ||
        ram[VF2_SOUND_COMMAND_WRITE_CURSOR + 1u] != (uint8_t)after ||
        ram[active] != 0u || ram[active + 1u] != 0xffu ||
        ram[active + 2u] != 0u || ram[active + 3u] != 0u) {
        fprintf(stderr, "L2 %08x: native B/90 expansion did not complete\n",
                (unsigned)command);
        return 0;
    }
    for (i = 0u; i < 6u; ++i) {
        size_t at = VF2_SOUND_COMMAND_RING_BASE +
                    (((size_t)before + (i / 3u) * 4u +
                      (i % 3u)) & (size_t)0x0fffu);
        if (ram[at] != packets[i]) {
            fprintf(stderr, "L2 %08x: packet byte[%u] got %02x expected %02x\n",
                    (unsigned)command, (unsigned)i,
                    (unsigned)ram[at], (unsigned)packets[i]);
            return 0;
        }
    }

    /* Independent reference prestate: the ROM worker starts at 0x1f7c.
     * Stop BEFORE RTS at 0x2198 after FF 2F termination. No C-only
     * SCSP L2 timer pokes are falsely compared to this worker subset. */
    memset(&bus, 0, sizeof(bus));
    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = rom;
    bus.rom_size = rom_size;
    bus.sample_rom = sample_bank;
    bus.sample_rom_size = sample_bank_size;
    bus.scsp = &step_scsp;
    vf2_m68k_regs_reset(&regs);
    regs.pc = UINT32_C(0x1f7c);
    regs.a[2] = active;
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    sstatus = vf2_m68k_run(&bus, &regs, UINT32_C(0x2198),
                           0, 300u, &steps);
    fprintf(stderr,
            "L2 %08x detailed: count C/S %u/%u cursor C/S %04x/%04x "
            "flags C/S %02x/%02x latch C/S %02x/%02x "
            "countdown C/S %02x%02x/%02x%02x\n",
            (unsigned)command,
            (unsigned)ram[VF2_SOUND_COMMAND_COUNT],
            (unsigned)step_ram[VF2_SOUND_COMMAND_COUNT],
            (unsigned)after,
            (unsigned)(((uint16_t)step_ram[VF2_SOUND_COMMAND_WRITE_CURSOR] << 8u) |
                       step_ram[VF2_SOUND_COMMAND_WRITE_CURSOR + 1u]),
            (unsigned)ram[active], (unsigned)step_ram[active],
            (unsigned)ram[active+1u], (unsigned)step_ram[active+1u],
            (unsigned)ram[active+2u], (unsigned)ram[active+3u],
            (unsigned)step_ram[active+2u], (unsigned)step_ram[active+3u]);
    {
        size_t ix;
        for (ix = 0u; ix < VF2_SOUND_STREAM_DESCRIPTOR_BYTES; ++ix) {
            if (ram[active+ix] != step_ram[active+ix]) {
                fprintf(stderr,"L2 first descriptor diff +%02x C=%02x S=%02x\n",
                        (unsigned)ix,
                        (unsigned)ram[active+ix],
                        (unsigned)step_ram[active+ix]);
                break;
            }
        }
        for (ix = 0u; ix < 0x1000u; ++ix) {
            if (ram[VF2_SOUND_COMMAND_RING_BASE+ix] !=
                step_ram[VF2_SOUND_COMMAND_RING_BASE+ix]) {
                fprintf(stderr,"L2 first ring diff +%03x C=%02x S=%02x\n",
                        (unsigned)ix,
                        (unsigned)ram[VF2_SOUND_COMMAND_RING_BASE+ix],
                        (unsigned)step_ram[VF2_SOUND_COMMAND_RING_BASE+ix]);
                break;
            }
        }
    }
    if (sstatus != VF2_OK || regs.pc != UINT32_C(0x2198) ||
        step_ram[VF2_SOUND_COMMAND_COUNT] != 2u ||
        step_ram[VF2_SOUND_COMMAND_WRITE_CURSOR] != (uint8_t)(after >> 8u) ||
        step_ram[VF2_SOUND_COMMAND_WRITE_CURSOR + 1u] != (uint8_t)after ||
        memcmp(ram + active, step_ram + active,
               VF2_SOUND_STREAM_DESCRIPTOR_BYTES) != 0 ||
        memcmp(ram + VF2_SOUND_COMMAND_RING_BASE,
               step_ram + VF2_SOUND_COMMAND_RING_BASE, 0x1000u) != 0) {
        fprintf(stderr,
                "L2 %08x: ROM differential NOT proven: S=%d PC=%05x steps=%u\n",
                (unsigned)command, (int)sstatus, (unsigned)regs.pc,
                (unsigned)steps);
        return 0;
    }
    fprintf(stderr, "MATCH AD L2 %08x: B and 90 command packets; "
                    "68000 0x2198 and C ring/descriptor agree\n",
            (unsigned)command);
    return 1;
}

/* Real-ROM A0 record allocation: C runs the complete SOUND dispatch,
 * while the original 68000 stepper stops BEFORE the 0x1f02 RTS.
 * Compare only descriptor state at this common semantic boundary;
 * ring counters differ because C has already completed its drain.
 */
static int check_a0_join(uint32_t command, const uint8_t *rom,
                         size_t rom_size)
{
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report report;
    vf2_status cstatus, sstatus;
    uint32_t steps = 0u;
    uint32_t row, record;
    uint32_t expected_row = command == UINT32_C(0x00ad2342)
                            ? UINT32_C(0x60959c) : UINT32_C(0x60979e);
    uint32_t expected_record = command == UINT32_C(0x00ad2342)
                               ? UINT32_C(0x60b4fe) : UINT32_C(0x60b7f2);
    uint32_t row_index = (command >> 8u) & UINT32_C(0x7f);
    uint32_t selector = command & UINT32_C(0xff);
    uint8_t *ram = board.sound_ram;
    uint8_t *target = ram + VF2_SOUND_DESCRIPTOR_BASE +
                      3u * VF2_SOUND_STREAM_DESCRIPTOR_BYTES;
    uint32_t expected_postinc;
    size_t i;

    if ((command != UINT32_C(0x00ad2342) &&
         command != UINT32_C(0x00ad2401)) ||
        0x8e12u + (size_t)row_index * 4u + 4u > rom_size) {
        return 0;
    }
    row = read_be32(rom + 0x8e12u + (size_t)row_index * 4u);
    if (row < VF2_SOUND_AUDIO_ROM_BASE ||
        (size_t)(row - VF2_SOUND_AUDIO_ROM_BASE) + 2u +
        4u * ((size_t)selector + 1u) > rom_size) {
        return 0;
    }
    record = read_be32(rom + (size_t)(row - VF2_SOUND_AUDIO_ROM_BASE) +
                       2u + (size_t)selector * 4u);
    if (row != expected_row || record != expected_record ||
        rom[(size_t)(record - VF2_SOUND_AUDIO_ROM_BASE)] != 0x8fu) {
        fprintf(stderr, "A0 ROM row or selected record invalid\n");
        return 0;
    }
    expected_postinc = record + 1u;

    /* Pristine copy; only slot 2 is free among six candidates,
     * with deliberate nonzero contents to verify the 0x1f04 clear. */
    memset(ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE, 0,
           VF2_SOUND_COMMAND_DESCRIPTOR_COUNT *
           VF2_SOUND_COMMAND_DESCRIPTOR_BYTES);
    ram[VF2_SOUND_COMMAND_DESCRIPTOR_BASE] = 0x80u;
    ram[VF2_SOUND_COMMAND_DESCRIPTOR_BASE + 1u] = 0x0du;
    memset(ram + VF2_SOUND_DESCRIPTOR_BASE, 0,
           8u * VF2_SOUND_STREAM_DESCRIPTOR_BYTES);
    for (i = 0u; i < 6u; ++i) {
        ram[VF2_SOUND_DESCRIPTOR_BASE +
            (i+1u) * VF2_SOUND_STREAM_DESCRIPTOR_BYTES] = 0x80u;
    }
    memset(target, 0x5a, VF2_SOUND_STREAM_DESCRIPTOR_BYTES);
    target[0] = 0u;

    memcpy(step_ram, ram, VF2_SOUND_RAM_BYTES);
    memcpy(&step_scsp, &board.scsp, sizeof(step_scsp));
    memset(&report, 0, sizeof(report));
    cstatus = vf2_sound_board_dispatch_next(&board, &report);

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
    sstatus = vf2_m68k_run(&bus, &regs, UINT32_C(0x1f02), 0,
                           L3_STEP_LIMIT, &steps);

    {
        const size_t off = (size_t)(target - ram);
        size_t pos;
        int found = 0;
        for (pos = 0u; pos < VF2_SOUND_STREAM_DESCRIPTOR_BYTES; ++pos) {
            if (target[pos] != step_ram[off + pos]) {
                fprintf(stderr,
                        "A0 %08x first descriptor mismatch +0x%02x C=%02x S=%02x\n",
                        (unsigned)command, (unsigned)pos,
                        (unsigned)target[pos], (unsigned)step_ram[off + pos]);
                found = 1;
                break;
            }
        }
        if (!found) {
            fprintf(stderr,
                    "A0 %08x descriptor matches; other predicates: "
                    "C=%d handled=%d ring(C/S)=%u/%u SCSP=%d "
                    "voice=%02x ptr=%08x\n",
                    (unsigned)command, (int)cstatus, report.handled,
                    (unsigned)ram[VF2_SOUND_COMMAND_COUNT],
                    (unsigned)step_ram[VF2_SOUND_COMMAND_COUNT],
                    memcmp(&board.scsp, &step_scsp, sizeof(step_scsp)),
                    (unsigned)target[0], (unsigned)read_be32(target + 4u));
        }
    }
    if (cstatus != VF2_OK || report.handler_address != UINT32_C(0x1f02) ||
        report.handled != 1 || ram[VF2_SOUND_COMMAND_COUNT] != 0u ||
        sstatus != VF2_OK || regs.pc != UINT32_C(0x1f02) ||
        step_ram[VF2_SOUND_COMMAND_COUNT] != 0u ||
        memcmp(target, step_ram + (size_t)(target-ram),
               VF2_SOUND_STREAM_DESCRIPTOR_BYTES) != 0 ||
        memcmp(&board.scsp, &step_scsp, sizeof(step_scsp)) != 0 ||
        target[0] != 0x80u || target[2] != 0u || target[3] != 1u ||
        read_be32(target + 4u) != expected_postinc) {
        fprintf(stderr,
                "A0 %08x: C=%d handler=%05x handled=%d; "
                "stepper=%d pc=%05x steps=%u; slot2=%02x ptr=%08x\n",
                (unsigned)command, (int)cstatus,
                (unsigned)report.handler_address, report.handled,
                (int)sstatus, (unsigned)regs.pc, (unsigned)steps,
                (unsigned)target[0], (unsigned)read_be32(target+4u));
        return 0;
    }
    fprintf(stderr,
            "MATCH A0 %08x: stepper 0x1f02, slot2 initialized "
            "ptr=%08x / clear32, C consumed ring\n",
            (unsigned)command, (unsigned)expected_postinc);
    return check_l2_handoff(command, rom, rom_size);
}

static int check_one(uint32_t command, const uint8_t *rom, size_t rom_size,
                     int l3_only)
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

    if (vf2_sound_board_initialize(&board, rom, rom_size,
                                    sample_bank, sample_bank_size) !=
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
    bus.sample_rom = sample_bank;
    bus.sample_rom_size = sample_bank_size;
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
    if (!l3_only && !check_a0_join(command, rom, rom_size)) {
        return 0;
    }
    fprintf(stderr, "MATCH C/68000 L3%s %08x: %02x %02x %02x, ring count 1\n",
            l3_only ? " (isolated packet fixture)" : "",
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
    uint8_t *samples = NULL;
    size_t audio_size = 0u, data_size = 0u, samples_size = 0u;
    size_t i = 0u;
    int valid = 1;
    int l3_only = 0;
    uint32_t l3_packet = 0u;

    /*
     * Opt-in 24-bit UART packet replay. Caller-supplied bytes can come
     * from a verified local i960 trace, but this test only proves the
     * isolated 68000 L3 receiver agrees with the recovered C receiver.
     * It does NOT prove a natural event, timing, dispatcher or KEYON.
     * Keep the two original A0/L2 ROM-table tests unchanged by default.
     */
    if (argc != 2 && !(argc == 4 &&
                       strcmp(argv[2], "--l3-packet") == 0)) {
        fprintf(stderr,
                "usage: %s <vf2-rom-directory> [--l3-packet <six-hex-digits>]\n",
                argv[0]);
        return 2;
    }
    if (argc == 4) {
        const char *hex = argv[3];
        size_t j;
        l3_only = 1;
        if (hex == NULL || strlen(hex) != 6u) {
            fprintf(stderr, "--l3-packet requires exactly six hex digits\n");
            return 2;
        }
        for (j = 0u; j < 6u; ++j) {
            unsigned digit;
            if (hex[j] >= '0' && hex[j] <= '9') {
                digit = (unsigned)(hex[j] - '0');
            } else if (hex[j] >= 'a' && hex[j] <= 'f') {
                digit = 10u + (unsigned)(hex[j] - 'a');
            } else if (hex[j] >= 'A' && hex[j] <= 'F') {
                digit = 10u + (unsigned)(hex[j] - 'A');
            } else {
                fprintf(stderr, "--l3-packet has a non-hex digit\n");
                return 2;
            }
            l3_packet = (l3_packet << 4u) | (uint32_t)digit;
        }
    }
    if (vf2_romset_build_region(argv[1], VF2_REGION_AUDIOCPU,
                                &audio, &audio_size) != VF2_OK ||
        vf2_romset_build_region(argv[1], VF2_REGION_MAIN_DATA,
                                &data, &data_size) != VF2_OK ||
        vf2_romset_build_region(argv[1], VF2_REGION_SAMPLES,
                                &samples, &samples_size) != VF2_OK ||
        audio == NULL || data == NULL || samples == NULL) {
        fprintf(stderr, "cannot load verified sound/main-data regions\n");
        free(audio);
        free(data);
        free(samples);
        return 2;
    }
    sample_bank = samples;
    sample_bank_size = samples_size;
    if (l3_only) {
        valid = check_one(l3_packet, audio, audio_size, 1);
    } else {
        for (i = 0u; i < sizeof(legs) / sizeof(legs[0]); ++i) {
            uint32_t command = 0u;
            if (!table_command(data, data_size, legs[i].profile,
                               legs[i].column, &command) ||
                command != legs[i].expected ||
                !check_one(command, audio, audio_size, 0)) {
                fprintf(stderr, "FAIL: profile %u column %u, got %08x\n",
                        (unsigned)legs[i].profile,
                        (unsigned)legs[i].column, (unsigned)command);
                valid = 0;
            }
        }
    }
    free(audio);
    free(data);
    free(samples);
    return valid ? 0 : 1;
}
