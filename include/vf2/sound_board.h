#ifndef VF2_SOUND_BOARD_H
#define VF2_SOUND_BOARD_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/scsp.h"
#include "vf2/status.h"

enum {
    VF2_SOUND_RAM_BYTES = 0x080000u,
    /* Effective sound-RAM addresses after the ROM's A6 base (0x1000). */
    VF2_SOUND_COMMAND_RING_BASE = 0x001000u,
    VF2_SOUND_COMMAND_COUNT = 0x002506u,
    VF2_SOUND_COMMAND_WRITE_CURSOR = 0x002504u,
    VF2_SOUND_COMMAND_CURSOR = 0x002508u,
    VF2_SOUND_CURRENT_COMMAND = 0x00250eu,
    VF2_SOUND_LOOKUP_CURRENT = 0x00250fu,
    VF2_SOUND_LOOKUP_MODE = 0x002510u,
    VF2_SOUND_LOOKUP_CONTROL = 0x002511u,
    VF2_SOUND_LOOKUP_WORD_ALT = 0x002512u,
    VF2_SOUND_LOOKUP_BYTE = 0x002515u,
    VF2_SOUND_LOOKUP_WORD = 0x002516u,
    VF2_SOUND_LOOKUP_META = 0x002518u,
    VF2_SOUND_LOOKUP_RELEASE = 0x00250du,
    VF2_SOUND_SHARED_FACTOR_151F = 0x00251fu,
    VF2_SOUND_CHANNEL_TABLE_BASE = 0x004200u,
    VF2_SOUND_CHANNEL_CONTROL_BASE = 0x004220u,
    VF2_SOUND_CHANNEL_CONTROL_ALT_BASE = 0x004240u,
    /* Stream files live at absolute 0x3000/0x3020 (driver a2 bases
       0x2000(a6)/0x2020(a6) with A6 = 0x1000); absolute 0x2000 is the
       separate sample-upload control area and is unmodeled. */
    VF2_SOUND_DESCRIPTOR_BASE = 0x003000u,
    VF2_SOUND_STREAM_ALTERNATE_BASE = 0x003020u,
    VF2_SOUND_STREAM_DESCRIPTOR_BYTES = 0x20u,
    VF2_SOUND_STREAM_DESCRIPTOR_COUNT = 6u,
    VF2_SOUND_STREAM_MAINTENANCE_COUNT = 8u,
    VF2_SOUND_COMMAND_DESCRIPTOR_BASE = 0x004000u,
    VF2_SOUND_COMMAND_DESCRIPTOR_BYTES = 0x10u,
    VF2_SOUND_COMMAND_DESCRIPTOR_COUNT = 20u,
    /* L3 UART receiver ring files ($1500 page, a6-relative on the
     * 68k side): the filed staging bytes at $1500-$1503, the 12-bit
     * write cursor ($1504, shared with emit_command), the shared
     * count ($1506), and the $1526 dance flag. The assembled ring
     * entries land in the 0x1000 command ring the drain consumes
     * ($0000(a6) destination = absolute 0x1000). */
    VF2_SOUND_RING_STAGE_BASE = 0x002500u,
    VF2_SOUND_SHARED_COUNTER_1526 = 0x002526u,
    VF2_SOUND_VOICE_TABLE_BASE = 0x002800u,
    VF2_SOUND_SHARED_COUNTER_151E = 0x00251eu,
    VF2_SOUND_SHARED_COUNTER_152E = 0x00252eu,
    VF2_SOUND_VOICE_BYTES = 0x10u,
    VF2_SOUND_VOICE_COUNT = 32u,
    /* Voice-record layout (v0825 V3b, 16 bytes, stride 0x10 x32).
     * Every name is behaviorally grounded in >=2 committed
     * recoveries; bytes 0x07/0x0d-0x0f are untouched by all of
     * them and stay unnamed (honest gap, not padding claimed). */
    VF2_SOUND_VOICE_SLOT = 0x00u,
    VF2_SOUND_VOICE_FLAGS = 0x02u,
    VF2_SOUND_VOICE_BYTE = 0x03u,
    VF2_SOUND_VOICE_CHANNEL = 0x04u,
    VF2_SOUND_VOICE_AGE_RELEASE = 0x05u,
    VF2_SOUND_VOICE_SELECT = 0x06u,
    VF2_SOUND_VOICE_MODE = 0x08u,
    VF2_SOUND_VOICE_AGE = 0x09u,
    VF2_SOUND_VOICE_META = 0x0au,
    VF2_SOUND_VOICE_RATE = 0x0bu,
    VF2_SOUND_VOICE_SAMPLE = 0x0cu,
    VF2_SOUND_AUDIO_ROM_BASE = 0x600000u,
    VF2_SOUND_SAMPLE_ROM_BASE = 0x800000u,
    VF2_SOUND_SAMPLE_DIRECT_BYTES = 0x200000u
};

typedef struct vf2_sound_voice_maintenance_report {
    size_t voices_scanned;
    size_t voices_expired;
    size_t normal_expirations;
    size_t release_expirations;
    size_t voices_aged;
    uint8_t counter_151e_before;
    uint8_t counter_151e_after;
    uint8_t counter_152e_before;
    uint8_t counter_152e_after;
} vf2_sound_voice_maintenance_report;

typedef struct vf2_sound_stream_maintenance_report {
    size_t descriptors_scanned;
    size_t timers_expired;
    size_t packets_emitted;
    size_t escape_records;
    size_t pointer_handoffs;
    size_t control_updates;
    size_t bytes_consumed;
    uint16_t write_cursor_before;
    uint16_t write_cursor_after;
} vf2_sound_stream_maintenance_report;

typedef struct vf2_sound_dispatch_report {
    uint8_t command;
    uint8_t payload;
    uint8_t command_class;
    uint16_t cursor_before;
    uint16_t cursor_after;
    size_t descriptors_scanned;
    size_t descriptors_matched;
    size_t descriptors_updated;
    size_t voices_cleared;
    size_t voices_matched;
    size_t voices_allocated;
    uint8_t channel;
    uint8_t selected_value;
    int16_t signed_value;
    uint32_t lookup_word;
    size_t voices_updated;
    uint32_t handler_address;
    int handled;
} vf2_sound_dispatch_report;

typedef struct vf2_sound_board {
    uint8_t sound_ram[VF2_SOUND_RAM_BYTES];
    const uint8_t *audio_rom;
    size_t audio_rom_size;
    const uint8_t *sample_rom;
    size_t sample_rom_size;
    uint16_t control;
    vf2_scsp scsp;
} vf2_sound_board;

vf2_status vf2_sound_board_initialize(
    vf2_sound_board *board,
    const uint8_t *audio_rom,
    size_t audio_rom_size,
    const uint8_t *sample_rom,
    size_t sample_rom_size
);

vf2_status vf2_sound_board_read_u8(
    vf2_sound_board *board,
    uint32_t address,
    uint8_t *value
);

vf2_status vf2_sound_board_write_u8(
    vf2_sound_board *board,
    uint32_t address,
    uint8_t value
);

vf2_status vf2_sound_board_read_u16(
    vf2_sound_board *board,
    uint32_t address,
    uint16_t *value
);

vf2_status vf2_sound_board_write_u16(
    vf2_sound_board *board,
    uint32_t address,
    uint16_t value
);

/* Execute the instruction-aligned 0x5fe voice-maintenance handler. */
vf2_status vf2_sound_board_maintain_voices(
    vf2_sound_board *board,
    vf2_sound_voice_maintenance_report *report
);

/* Execute the bounded 0x1f7c stream-interpreter packet paths. */
vf2_status vf2_sound_board_maintain_streams(
    vf2_sound_board *board,
    vf2_sound_stream_maintenance_report *report
);

/* Consume one ROM-format four-byte command-ring entry. */
vf2_status vf2_sound_board_dispatch_next(
    vf2_sound_board *board,
    vf2_sound_dispatch_report *report
);

/* Run the ROM 0xa8c setup-authoring arm on one descriptor/voice
 * pair (S3c): the shared head (+2 seed, slot key, +6 word) then
 * the HIGH (desc[2] >= 0xf0) or LOW authoring branch. Effects
 * applied, UNSUPPORTED at the 0xc1e common-tail door.
 * Indices (not pointers) keep RAM ownership inside the board.
 * Drain-unreachable bodies (LOW branch, ch-0xf seeding) are
 * proven through this entry with synthetic-entry differentials;
 * dispatch_next drives the HIGH/ch!=0xf shape. */
vf2_status vf2_sound_board_setup_author(
    vf2_sound_board *board,
    size_t descriptor_index,
    size_t voice_index,
    vf2_sound_dispatch_report *report
);

/* Emit one four-byte packet through the ROM stream-interpreter ring state. */
vf2_status vf2_sound_board_emit_command(
    vf2_sound_board *board,
    const uint8_t packet[4]
);

/* File one received UART byte the way the ROM L3 receiver ISR
 * (0x526, autovector 26) does: 0xff is the sound-CPU reboot door
 * (refused cleanly -- reboot is out of scope); >= 0xf0 is
 * ignored with no state change; otherwise the $1500 ring file,
 * $1502 age, $1503 file, $1504 cursor, $1506 count and the $1526
 * dance update run, with the $400001 system-port poke deferred
 * (unmodeled port, cf04 precedent: body applied, poke skipped).
 * The byte is the ISR's already-read d0 (the $0405(a5) hardware
 * read stays on silicon). */
vf2_status vf2_sound_board_file_uart_byte(
    vf2_sound_board *board,
    uint8_t byte
);

vf2_status vf2_sound_board_render(
    vf2_sound_board *board,
    int16_t *left,
    int16_t *right,
    size_t frames
);

#endif
