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
    VF2_SOUND_DESCRIPTOR_BASE = 0x002000u,
    VF2_SOUND_STREAM_ALTERNATE_BASE = 0x002020u,
    VF2_SOUND_STREAM_DESCRIPTOR_BYTES = 0x20u,
    VF2_SOUND_STREAM_DESCRIPTOR_COUNT = 6u,
    VF2_SOUND_STREAM_MAINTENANCE_COUNT = 8u,
    VF2_SOUND_COMMAND_DESCRIPTOR_BASE = 0x004000u,
    VF2_SOUND_COMMAND_DESCRIPTOR_BYTES = 0x10u,
    VF2_SOUND_COMMAND_DESCRIPTOR_COUNT = 20u,
    VF2_SOUND_VOICE_TABLE_BASE = 0x002800u,
    VF2_SOUND_SHARED_COUNTER_151E = 0x00251eu,
    VF2_SOUND_SHARED_COUNTER_152E = 0x00252eu,
    VF2_SOUND_VOICE_BYTES = 0x10u,
    VF2_SOUND_VOICE_COUNT = 32u,
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

void vf2_sound_board_initialize(
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

/* Emit one four-byte packet through the ROM stream-interpreter ring state. */
vf2_status vf2_sound_board_emit_command(
    vf2_sound_board *board,
    const uint8_t packet[4]
);

vf2_status vf2_sound_board_render(
    vf2_sound_board *board,
    int16_t *left,
    int16_t *right,
    size_t frames
);

#endif
