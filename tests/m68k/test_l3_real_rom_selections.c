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
    /* The original UART receiver treats C0..DF as TWO-byte
     * commands: byte 2 files the ring; an F0..FE byte 3 is
     * ignored. AD/9E commands file only after byte 3. */
    const int short_form = bytes[0] >= 0xc0u &&
        bytes[0] < 0xe0u && bytes[1] < 0x80u &&
        bytes[2] >= 0xf0u && bytes[2] < 0xffu;
    const size_t file_index = short_form ? 1u : 2u;

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
        /* Original 68000 files CD74 / CA7E / CB7F at byte 2;
         * F8 is an ignored trailer, not a third payload byte. */
        if (ram[VF2_SOUND_COMMAND_COUNT] !=
                (uint8_t)(i >= file_index) ||
            bus.event_count != (size_t)(i == file_index)) {
            fprintf(stderr, "L3 %08x byte %u: early/late ring file or poke\n",
                    (unsigned)command, (unsigned)i);
            return 0;
        }
        if (i == file_index &&
            (bus.events[0].address != UINT32_C(0x400001) ||
             bus.events[0].is_write == 0u)) {
            fprintf(stderr, "L3 %08x: missing final hardware poke\n",
                    (unsigned)command);
            return 0;
        }
    }
    if (board.sound_ram[VF2_SOUND_COMMAND_RING_BASE] != bytes[0] ||
        board.sound_ram[VF2_SOUND_COMMAND_RING_BASE + 1u] != bytes[1] ||
        board.sound_ram[VF2_SOUND_COMMAND_RING_BASE + 2u] !=
        (short_form ? 0u : bytes[2]) ||
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

/* Replay an EXACT UART byte stream without invented fixed three-byte
 * packet boundaries. The ORIGINAL 68000 L3 distinguishes 2-byte Cx
 * commands, ignored F0-FE bytes and 3-byte AD/9E commands. Compare
 * C-vs-original memory/SCSP state and actual hardware-poke count at
 * every individual byte. This remains an isolated 68000 L3 fixture;
 * source-of-frame and hardware interrupt timing are separate oracles. */
/* Owner-ROM SOUND TEST #11: original i960 command 00 AD 10 04.
 * This pins the genuine A0 ROM record-selection boundary for Akira's
 * theme. It does NOT claim that the music sequencer is recovered.
 * A single channel-D matching descriptor is explicitly staged on
 * BOTH sides, just as the existing AD23/24 owner-ROM A0 frontier test
 * does; this is controlled driver exploration, not gameplay timing. */
static int probe_akira_music(const uint8_t *rom, size_t rom_size)
{
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report report;
    vf2_status c_status, to_record, tail_status;
    uint32_t steps = 0u, tail_steps = 0u, row, record;
    size_t i;
    unsigned active = 0u;
    uint8_t *desc = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    if (!check_one(UINT32_C(0x00ad1004), rom, rom_size, 1)) {
        return 0;
    }
    if (rom_size < 0x9c60u) {
        fprintf(stderr, "AKIRA_ROM invalid truncated program\n");
        return 0;
    }
    row = read_be32(rom + 0x8e12u + 0x10u * 4u);
    record = read_be32(rom + (size_t)(row - VF2_SOUND_AUDIO_ROM_BASE) +
                       2u + 4u * 4u);
    if (row != UINT32_C(0x608f24) ||
        record != UINT32_C(0x609c36) ||
        read_be32(rom + (size_t)(record - VF2_SOUND_AUDIO_ROM_BASE)) !=
            UINT32_C(0x10101010)) {
        fprintf(stderr, "AKIRA_ROM unexpected AD10/04 music record\n");
        return 0;
    }
    memset(desc, 0, VF2_SOUND_COMMAND_DESCRIPTOR_COUNT *
                    VF2_SOUND_COMMAND_DESCRIPTOR_BYTES);
    desc[0] = UINT8_C(0x80);
    desc[1] = UINT8_C(0x0d);
    memcpy(step_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE, desc,
           VF2_SOUND_COMMAND_DESCRIPTOR_COUNT *
           VF2_SOUND_COMMAND_DESCRIPTOR_BYTES);
    if (vf2_scsp_attach_sample_ram(&board.scsp, board.sound_ram,
                                   sizeof(board.sound_ram)) != VF2_OK ||
        vf2_scsp_attach_sample_ram(&step_scsp, step_ram,
                                   sizeof(step_ram)) != VF2_OK) {
        return 0;
    }
    memset(&report, 0, sizeof(report));
    c_status = vf2_sound_board_dispatch_next(&board, &report);
    memset(&bus, 0, sizeof(bus));
    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = rom;
    bus.rom_size = rom_size;
    bus.sample_rom = sample_bank;
    bus.sample_rom_size = sample_bank_size;
    bus.scsp = &step_scsp;
    vf2_m68k_regs_reset(&regs);
    regs.pc = UINT32_C(0x7a6);
    regs.a[4] = UINT32_C(0x2506);
    regs.a[5] = VF2_SCSP_CPU_BASE;
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x5000);
    to_record = vf2_m68k_run(&bus, &regs, UINT32_C(0x1e98),
                             0, 30000u, &steps);
    printf("AKIRA_A0_FRONTIER command=00ad1004 row=%06x record=%06x "
           "C_status=%d C_handler=%05x M_status=%d M_pc=%05x "
           "M_steps=%u descriptor_matches=%u\n",
           (unsigned)row, (unsigned)record,
           (int)c_status, (unsigned)report.handler_address,
           (int)to_record, (unsigned)regs.pc, (unsigned)steps,
           (unsigned)report.descriptors_matched);
    if (c_status != VF2_ERROR_UNSUPPORTED ||
        report.handler_address != UINT32_C(0x1e98) ||
        to_record != VF2_OK || regs.pc != UINT32_C(0x1e98)) {
        fprintf(stderr, "AKIRA_A0_FRONTIER failed: no original ROM join\n");
        return 0;
    }
    /* Beyond here is a DIAGNOSTIC of the previously unmodeled
     * low-header A0 body. Original success/unsupported are reported,
     * never laundered into native-C/SCSP music parity. */
    tail_status = vf2_m68k_run(&bus, &regs, UINT32_C(0x265c),
                               0, 100000u, &tail_steps);
    for (i = 0u; i < VF2_SCSP_SLOT_COUNT; ++i) {
        active += (unsigned)(step_scsp.slot_active[i] != 0u);
    }
    printf("AKIRA_A0_TAIL original_status=%d pc=%05x steps=%u "
           "queue=%u active_slots=%u C_unrecovered=1 "
           "music_pcm_not_proven=1\n",
           (int)tail_status, (unsigned)regs.pc,
           (unsigned)tail_steps,
           (unsigned)step_ram[VF2_SOUND_COMMAND_COUNT], active);
    if (tail_status == VF2_OK && regs.pc == UINT32_C(0x265c)) {
        size_t slot, tick;
        /* Show exact original descriptor pointers after AD10/04,
         * without dumping copyright-protected stream content. */
        for (slot = 0u; slot < 8u; ++slot) {
            const size_t at = VF2_SOUND_DESCRIPTOR_BASE +
                              slot * VF2_SOUND_STREAM_DESCRIPTOR_BYTES;
            const uint32_t next = read_be32(step_ram + at + 4u);
            printf("AKIRA_A0_STREAM slot=%u flags=%02x%02x%02x%02x "
                   "next=%08x countdown=%02x%02x\n",
                   (unsigned)slot,
                   (unsigned)step_ram[at], (unsigned)step_ram[at + 1u],
                   (unsigned)step_ram[at + 2u],
                   (unsigned)step_ram[at + 3u], (unsigned)next,
                   (unsigned)step_ram[at + 12u],
                   (unsigned)step_ram[at + 13u]);
        }
        /* Original L2 IRQ probe: independent CPU invocation for a
         * finite number of ticks; never label these as real-time
         * scheduling or synthesize the recovered-C sequence. */
        for (tick = 0u; tick < 16u; ++tick) {
            uint32_t original_steps = 0u;
            vf2_status status;
            unsigned keyed = 0u;
            size_t voice;
            vf2_m68k_regs_reset(&regs);
            regs.pc = UINT32_C(0x1f16);
            regs.a[4] = UINT32_C(0x2506);
            regs.a[5] = VF2_SCSP_CPU_BASE;
            regs.a[6] = UINT32_C(0x1000);
            regs.a[7] = UINT32_C(0x5000); /* original reset SSP */
            status = vf2_m68k_run(&bus, &regs, UINT32_C(0x1f7a),
                                  0, 100000u, &original_steps);
            for (voice = 0u; voice < VF2_SCSP_SLOT_COUNT; ++voice) {
                keyed += (unsigned)(step_scsp.slot_active[voice] != 0u);
            }
            printf("AKIRA_MUSIC_L2 tick=%u status=%d pc=%05x "
                   "steps=%u active_slots=%u queue=%u no_playback_claim=1\n",
                   (unsigned)tick, (int)status, (unsigned)regs.pc,
                   (unsigned)original_steps, keyed,
                   (unsigned)step_ram[VF2_SOUND_COMMAND_COUNT]);
            if (status != VF2_OK || regs.pc != UINT32_C(0x1f7a)) {
                break; /* preserve original failure frontier */
            }
            {
                size_t dispatched = 0u;
                unsigned first_failed = 0u;
                while (step_ram[VF2_SOUND_COMMAND_COUNT] > 0u &&
                       dispatched < 64u) {
                    const uint8_t queued = step_ram[VF2_SOUND_COMMAND_COUNT];
                    const uint16_t cursor =
                        (uint16_t)(((uint16_t)step_ram[VF2_SOUND_COMMAND_CURSOR]
                          << 8u) | step_ram[VF2_SOUND_COMMAND_CURSOR + 1u]);
                    const uint16_t at = (uint16_t)(cursor & UINT16_C(0x0fff));
                    uint8_t bytes[3];
                    uint32_t dispatch_steps = 0u;
                    vf2_status dispatch_status;
                    size_t v;
                    unsigned now_active = 0u;
                    bytes[0] = step_ram[VF2_SOUND_COMMAND_RING_BASE + at];
                    bytes[1] = step_ram[VF2_SOUND_COMMAND_RING_BASE +
                                        ((at + 1u) & UINT16_C(0x0fff))];
                    bytes[2] = step_ram[VF2_SOUND_COMMAND_RING_BASE +
                                        ((at + 2u) & UINT16_C(0x0fff))];
                    vf2_m68k_regs_reset(&regs);
                    regs.pc = UINT32_C(0x7a6);
                    regs.a[4] = UINT32_C(0x2506);
                    regs.a[5] = VF2_SCSP_CPU_BASE;
                    regs.a[6] = UINT32_C(0x1000);
                    regs.a[7] = UINT32_C(0x5000);
                    dispatch_status = vf2_m68k_run(
                        &bus, &regs, UINT32_C(0x265c),
                        0, 100000u, &dispatch_steps);
                    for (v = 0u; v < VF2_SCSP_SLOT_COUNT; ++v) {
                        now_active += (unsigned)
                            (step_scsp.slot_active[v] != 0u);
                    }
                    if (dispatched < 12u ||
                        dispatch_status != VF2_OK || now_active > 0u) {
                        printf("AKIRA_MUSIC_DRAIN tick=%u index=%u "
                               "bytes=%02x%02x%02x status=%d pc=%05x "
                               "steps=%u count_before=%u count_after=%u "
                               "active_slots=%u\n",
                               (unsigned)tick, (unsigned)dispatched,
                               (unsigned)bytes[0], (unsigned)bytes[1],
                               (unsigned)bytes[2], (int)dispatch_status,
                               (unsigned)regs.pc, (unsigned)dispatch_steps,
                               (unsigned)queued,
                               (unsigned)step_ram[VF2_SOUND_COMMAND_COUNT],
                               now_active);
                    }
                    ++dispatched;
                    if (dispatch_status != VF2_OK ||
                        regs.pc != UINT32_C(0x265c) ||
                        step_ram[VF2_SOUND_COMMAND_COUNT] >= queued) {
                        first_failed = 1u;
                        break;
                    }
                }
                printf("AKIRA_MUSIC_DRAIN_SUMMARY tick=%u drained=%u "
                       "queue=%u failed=%u original_only=1\n",
                       (unsigned)tick, (unsigned)dispatched,
                       (unsigned)step_ram[VF2_SOUND_COMMAND_COUNT],
                       first_failed);
                if (first_failed != 0u) break;
            }
        }
    }
    return 1;
}

static int check_uart_stream(
    const uint8_t *bytes,
    size_t length,
    const uint8_t *rom,
    size_t rom_size
)
{
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    size_t i;
    uint32_t steps = 0u;
    if (bytes == NULL || length == 0u ||
        vf2_sound_board_initialize(&board, rom, rom_size,
                                    sample_bank, sample_bank_size) !=
            VF2_OK) {
        fprintf(stderr, "L3 UART stream initialization failed\n");
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

    for (i = 0u; i < length; ++i) {
        vf2_status c_status, s_status;
        uint8_t before = board.sound_ram[VF2_SOUND_COMMAND_COUNT];
        uint8_t after;
        uint8_t newly_filed;
        size_t event_index;
        c_status = vf2_sound_board_file_uart_byte(&board, bytes[i]);
        vf2_m68k_regs_reset(&regs);
        regs.d[0] = bytes[i];
        regs.a[6] = UINT32_C(0x1000);
        regs.a[7] = UINT32_C(0x7f00);
        regs.pc = UINT32_C(0x52e);
        bus.event_count = 0u;
        s_status = vf2_m68k_run(&bus, &regs, UINT32_C(0x5ea),
                                0, L3_STEP_LIMIT, &steps);
        after = board.sound_ram[VF2_SOUND_COMMAND_COUNT];
        newly_filed = (uint8_t)(after - before);
        if (c_status != VF2_OK || s_status != VF2_OK ||
            regs.pc != UINT32_C(0x5ea) ||
            !compare_ram(board.sound_ram, step_ram, i) ||
            memcmp(&board.scsp, &step_scsp, sizeof(step_scsp)) != 0 ||
            bus.event_count != (size_t)newly_filed) {
            fprintf(stderr,
                    "L3 UART stream byte %u/%u = %02x differs:"
                    " C=%d S=%d pc=%05x count=%u->%u events=%u\n",
                    (unsigned)i, (unsigned)length, (unsigned)bytes[i],
                    (int)c_status, (int)s_status, (unsigned)regs.pc,
                    (unsigned)before, (unsigned)after,
                    (unsigned)bus.event_count);
            return 0;
        }
        for (event_index = 0u; event_index < bus.event_count; ++event_index) {
            if (bus.events[event_index].address != UINT32_C(0x400001) ||
                bus.events[event_index].is_write == 0u) {
                fprintf(stderr, "L3 UART stream unexpected hardware poke"
                        " at byte %u\n", (unsigned)i);
                return 0;
            }
        }
    }
    fprintf(stderr,
            "MATCH C/68000 original UART stream bytes=%u ring_count=%u"
            " write_cursor=%02x%02x\n",
            (unsigned)length,
            (unsigned)board.sound_ram[VF2_SOUND_COMMAND_COUNT],
            (unsigned)board.sound_ram[VF2_SOUND_COMMAND_WRITE_CURSOR],
            (unsigned)board.sound_ram[VF2_SOUND_COMMAND_WRITE_CURSOR + 1u]);
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
    int l3_matrix = 0;
    int l3_stream = 0;
    int akira_music_probe = 0;
    uint8_t stream_bytes[64] = {0u};
    size_t stream_length = 0u;
    uint32_t l3_packet = 0u;

    /*
     * Opt-in 24-bit UART packet replay. Caller-supplied bytes can come
     * from a verified local i960 trace, but this test only proves the
     * isolated 68000 L3 receiver agrees with the recovered C receiver.
     * It does NOT prove a natural event, timing, dispatcher or KEYON.
     * Keep the two original A0/L2 ROM-table tests unchanged by default.
     */
    if (argc != 2 && !(argc == 4 &&
                       (strcmp(argv[2], "--l3-packet") == 0 ||
                        strcmp(argv[2], "--uart-stream") == 0)) &&
        !(argc == 3 &&
          (strcmp(argv[2], "--matrix-l3") == 0 ||
           strcmp(argv[2], "--probe-akira-music") == 0))) {
        fprintf(stderr,
                "usage: %s <vf2-rom-directory> [--l3-packet <six-hex-digits> "
                "| --uart-stream <even-hex-digits> | --matrix-l3 "
                "| --probe-akira-music]\n",
                argv[0]);
        return 2;
    }
    l3_matrix = argc == 3 && strcmp(argv[2], "--matrix-l3") == 0;
    akira_music_probe = argc == 3 &&
                        strcmp(argv[2], "--probe-akira-music") == 0;
    l3_stream = argc == 4 &&
        strcmp(argv[2], "--uart-stream") == 0;
    if (argc == 4) {
        const char *hex = argv[3];
        size_t j, length = hex != NULL ? strlen(hex) : 0u;
        l3_only = !l3_stream;
        if (length == 0u || (length & 1u) != 0u ||
            length > sizeof(stream_bytes) * 2u ||
            (!l3_stream && length != 6u)) {
            fprintf(stderr, "UART input requires an even 2..128 hex digits "
                            "(--l3-packet exactly six)\n");
            return 2;
        }
        for (j = 0u; j < length; ++j) {
            unsigned digit;
            if (hex[j] >= '0' && hex[j] <= '9') {
                digit = (unsigned)(hex[j] - '0');
            } else if (hex[j] >= 'a' && hex[j] <= 'f') {
                digit = 10u + (unsigned)(hex[j] - 'a');
            } else if (hex[j] >= 'A' && hex[j] <= 'F') {
                digit = 10u + (unsigned)(hex[j] - 'A');
            } else {
                fprintf(stderr, "UART input has a non-hex digit\n");
                return 2;
            }
            if (l3_stream) {
                if ((j & 1u) == 0u) {
                    stream_bytes[j / 2u] = (uint8_t)(digit << 4u);
                } else {
                    stream_bytes[j / 2u] |= (uint8_t)digit;
                }
            } else {
                l3_packet = (l3_packet << 4u) | (uint32_t)digit;
            }
        }
        stream_length = length / 2u;
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
    if (akira_music_probe) {
        valid = probe_akira_music(audio, audio_size);
        free(audio);
        free(data);
        free(samples);
        return valid ? 0 : 1;
    }
    if (l3_matrix) {
        /*
         * The 18x11 original ROM table contains 107 occupied cells and
         * 98 distinct AD23/AD24 commands in the supported VF2.1 set.
         * A ROM-table cell is not evidence of a naturally triggered SFX.
         * Replay all distinct commands through isolated L3 C/68000 only.
         */
        uint32_t seen[18u * 11u] = {0u};
        size_t unique_count = 0u;
        size_t occupied_count = 0u;
        size_t passed_count = 0u;
        uint32_t profile;
        for (profile = 1u; profile <= 18u; ++profile) {
            uint32_t column;
            for (column = 0u; column < 11u; ++column) {
                uint32_t command = 0u;
                size_t entry;
                if (!table_command(data, data_size, profile,
                                   column, &command)) {
                    /* Valid empty cell is harmless. A nonempty foreign
                     * command family must never be skipped silently. */
                    if (command != 0u) {
                        fprintf(stderr,
                                "matrix: unsupported ROM command %08x at %u,%u\n",
                                (unsigned)command,
                                (unsigned)profile, (unsigned)column);
                        valid = 0;
                    }
                    continue;
                }
                ++occupied_count;
                for (entry = 0u; entry < unique_count; ++entry) {
                    if (seen[entry] == command) {
                        break;
                    }
                }
                if (entry != unique_count) {
                    continue;
                }
                seen[unique_count++] = command;
                if (check_one(command, audio, audio_size, 1)) {
                    ++passed_count;
                } else {
                    valid = 0;
                    fprintf(stderr, "matrix: L3 mismatch command %08x from %u,%u\n",
                            (unsigned)command,
                            (unsigned)profile, (unsigned)column);
                }
            }
        }
        printf("ROM sound-matrix L3: %u occupied, %u unique, %u passed\n",
               (unsigned)occupied_count, (unsigned)unique_count,
               (unsigned)passed_count);
        if (occupied_count != 107u || unique_count != 98u ||
            passed_count != unique_count) {
            fprintf(stderr,
                    "matrix: expected 107 occupied/98 unique with all L3 matches\n");
            valid = 0;
        }
    } else if (l3_only) {
        valid = check_one(l3_packet, audio, audio_size, 1);
    } else if (l3_stream) {
        valid = check_uart_stream(
            stream_bytes, stream_length, audio, audio_size);
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
