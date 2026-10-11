#include "vf2/sound_board.h"

#include <string.h>

/* ROM table addressed by the 0x174c B0 handler's PC-relative lookup at
   audio-CPU address 0x179e. */

static uint16_t read_be16(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8u) | data[1]);
}

static uint32_t read_be32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24u) |
           ((uint32_t)data[1] << 16u) |
           ((uint32_t)data[2] << 8u) |
           data[3];
}

static void write_be16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value >> 8u);
    data[1] = (uint8_t)value;
}

static void write_be32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value >> 24u);
    data[1] = (uint8_t)(value >> 16u);
    data[2] = (uint8_t)(value >> 8u);
    data[3] = (uint8_t)value;
}

static vf2_status sound_board_key_off(
    vf2_sound_board *board,
    const uint8_t *voice
)
{
    /* ROM L1-ISR key portion (0x636/0x684): d1 = voice[0], the slot
       word (slot*0x20, v0794-proven, used directly as the SCSP/RAM
       stride with no masking). SCSP[slot+0x0b] = 0x1f (max release
       rate) and sound_ram[0x2000+slot] = 0. No slot-control
       read-modify-write exists on ROM; the earlier control-bit form
       came from a misread of indexed move.b #imm as absolute. */
    const uint16_t slot_word = read_be16(voice);
    const uint32_t scsp_address = (uint32_t)slot_word + 0x0bu;
    const uint32_t ram_address =
        UINT32_C(0x2000) + (uint32_t)slot_word;
    if (vf2_scsp_write_u8(&board->scsp, scsp_address,
                          UINT8_C(0x1f)) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    if (ram_address >= VF2_SOUND_RAM_BYTES) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    board->sound_ram[ram_address] = 0u;
    return VF2_OK;
}

static vf2_status read_rom_byte(
    const uint8_t *rom,
    size_t rom_size,
    uint32_t offset,
    uint8_t *value
)
{
    if (rom == NULL || (size_t)offset >= rom_size) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    *value = rom[offset];
    return VF2_OK;
}

static vf2_status read_audio_be32(
    const vf2_sound_board *board,
    uint32_t address,
    uint32_t *value
)
{
    const uint32_t offset = address - VF2_SOUND_AUDIO_ROM_BASE;

    if (board == NULL || value == NULL || address < VF2_SOUND_AUDIO_ROM_BASE ||
        (size_t)offset + 4u > board->audio_rom_size ||
        board->audio_rom == NULL) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    *value = ((uint32_t)board->audio_rom[offset] << 24u) |
             ((uint32_t)board->audio_rom[offset + 1u] << 16u) |
             ((uint32_t)board->audio_rom[offset + 2u] << 8u) |
             board->audio_rom[offset + 3u];
    return VF2_OK;
}

static vf2_status read_audio_byte_absolute(
    const vf2_sound_board *board,
    uint32_t address,
    uint8_t *value
);

static vf2_status read_audio_be16(
    const vf2_sound_board *board,
    uint32_t address,
    uint16_t *value
)
{
    uint8_t high = 0u;
    uint8_t low = 0u;

    if (value == NULL || read_audio_byte_absolute(board, address, &high) != VF2_OK ||
        read_audio_byte_absolute(board, address + 1u, &low) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    *value = (uint16_t)(((uint16_t)high << 8u) | low);
    return VF2_OK;
}

static vf2_status read_audio_byte_absolute(
    const vf2_sound_board *board,
    uint32_t address,
    uint8_t *value
)
{
    if (address < VF2_SOUND_AUDIO_ROM_BASE) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    return read_rom_byte(
        board->audio_rom, board->audio_rom_size,
        address - VF2_SOUND_AUDIO_ROM_BASE, value
    );
}

/* ROM 0x1ac0/0x422 sample uploader, shared by boot init and B0 entry
   0x3a. First-copy stages d7 bytes from ROM 0x605b28 to RAM $5000;
   the second phase walks one row of the 0x608df0 table (row selected
   by the caller) and copies sample records to RAM $50000 with the
   $6FFFF guard. d1 < 0 skips the second phase (boot dance dropped:
   it writes only an unmodeled system port). check_7f arms B0's
   0x7f trap: first-copy effects persist, then UNSUPPORTED. */
static vf2_status sound_board_sample_upload(
    vf2_sound_board *board,
    uint32_t row_address,
    int check_7f,
    uint8_t payload,
    int check_gate
)
{
    uint16_t first_count = 0u;
    uint16_t record_index = 0u;
    uint32_t table = 0u;
    uint32_t row = 0u;
    /* a2 is the fixed $5000 sample-slot table (absolute short). */
    uint32_t slot_base = UINT32_C(0x5000);
    uint32_t target = 0u;
    int16_t gate = 0;
    size_t index = 0u;

    if (read_audio_be16(board, UINT32_C(0x605b26), &first_count) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    for (index = 0u; index < first_count; ++index) {
        uint8_t staged = 0u;
        if (read_audio_byte_absolute(
                board, UINT32_C(0x605b28) + (uint32_t)index, &staged
            ) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        if (UINT32_C(0x5000) + index >= VF2_SOUND_RAM_BYTES) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        board->sound_ram[0x5000u + index] = staged;
    }
    if (check_7f && (payload & 0x7fu) == 0x7fu) {
        /* ROM beq lands mid-dbf (stale +2): trap on HW. The staged
           bytes above are the only effects; refuse. */
        return VF2_ERROR_UNSUPPORTED;
    }
    if (read_audio_be32(board, row_address, &row) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    /* Boot gates on the row pointer's high word (the d1 = (a1)+ word
       at 0x608df0); B0 has no gate (it has the 0x7f trap instead). */
    gate = (int16_t)(row >> 16u);
    if (check_gate && gate < 0) {
        return VF2_OK;
    }
    if (read_audio_be32(board, UINT32_C(0x605b28), &table) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    if (read_audio_be16(board, row, &first_count) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    row += 2u;
    target = UINT32_C(0x50000);
    for (record_index = 0u; record_index < first_count; ++record_index) {
        uint16_t selector = 0u;
        uint32_t entry = 0u;
        uint32_t source = 0u;
        uint32_t length = 0u;
        uint32_t stored = 0u;
        uint16_t count = 0u;
        if (read_audio_be16(board, row, &selector) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        row += 2u;
        entry = (uint32_t)(uint16_t)selector * UINT32_C(0x14);
        if (read_audio_be32(board, table + entry, &source) != VF2_OK ||
            read_audio_be32(board, table + entry + 8u, &stored) != VF2_OK ||
            read_audio_be32(board, table + entry + 4u, &length) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        length += UINT32_C(0x1ff);
        stored = (stored & UINT32_C(0xffffff00)) |
            ((stored - source) & UINT32_C(0xff));
        if (slot_base + 0x1008u + 4u > VF2_SOUND_RAM_BYTES ||
            slot_base + (uint16_t)entry + 4u > VF2_SOUND_RAM_BYTES) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        write_be32(board->sound_ram + slot_base + 0x1008u, stored);
        write_be32(board->sound_ram + slot_base + (uint16_t)entry, target);
        board->sound_ram[slot_base + (uint16_t)entry] |= 0x80u;
        count = (uint16_t)(length & UINT32_C(0xffff));
        for (index = 0u; ; ++index) {
            uint8_t sample = 0u;
            if (target >= VF2_SOUND_RAM_BYTES) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
            if (vf2_sound_board_read_u8(board, source + (uint32_t)index,
                                            &sample) !=
                VF2_OK) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
            board->sound_ram[target] = sample;
            ++target;
            if (index >= count) {
                break;
            }
        }
        /* ROM cmpa.l #$6ffff,a3 / bgt halt: fail closed. */
        if (target > UINT32_C(0x6ffff)) {
            return VF2_ERROR_UNSUPPORTED;
        }
    }
    return VF2_OK;
}

vf2_status vf2_sound_board_initialize(
    vf2_sound_board *board,
    const uint8_t *audio_rom,
    size_t audio_rom_size,
    const uint8_t *sample_rom,
    size_t sample_rom_size
)
{
    static const uint16_t scsp_block[7] = {0u, 0xc8u, 0u, 0x2c8u,
                                           0x88u, 0x48u, 0u};
    uint32_t blob = 0u;
    uint32_t image = 0u;
    uint32_t volume_table = 0u;
    size_t index = 0u;
    size_t voice = 0u;

    if (board == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(board, 0, sizeof(*board));
    board->audio_rom = audio_rom;
    board->audio_rom_size = audio_rom_size;
    board->sample_rom = sample_rom;
    board->sample_rom_size = sample_rom_size;
    vf2_scsp_initialize(&board->scsp);
    if (sample_rom != NULL && sample_rom_size != 0u) {
        (void)vf2_scsp_attach_sample_rom(&board->scsp,
                                          sample_rom, sample_rom_size);
    }
    /* ROM 0x28e SCSP init. $400001 writes are an unmodeled system
       port outside the audio contract; $0405 reads are waste. */
    if (vf2_scsp_write_u8(&board->scsp, UINT32_C(0x401), 0u) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    if (read_audio_be32(board, UINT32_C(0x604940), &blob) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    for (index = 0u; index < UINT32_C(0x500); ++index) {
        uint8_t wave = 0u;
        if (read_audio_byte_absolute(board, blob + (uint32_t)index, &wave) !=
            VF2_OK ||
            vf2_scsp_write_u8(&board->scsp, UINT32_C(0x700) + (uint32_t)index,
                              wave) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
    }
    if (vf2_scsp_write_u16(&board->scsp, UINT32_C(0x402), UINT16_C(0x138)) !=
        VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    for (index = 0u; index < VF2_SCSP_SLOT_COUNT; ++index) {
        uint32_t slot = (uint32_t)index * VF2_SCSP_SLOT_BYTES;
        if (vf2_scsp_write_u8(&board->scsp, slot + 0x0bu, UINT8_C(0xff)) !=
            VF2_OK ||
            vf2_scsp_write_u8(&board->scsp, slot + 0x0du, UINT8_C(0xff)) !=
            VF2_OK ||
            vf2_scsp_write_u8(&board->scsp, slot, UINT8_C(0x10)) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
    }
    for (index = 0u; index < 7u; ++index) {
        if (vf2_scsp_write_u16(&board->scsp,
                               UINT32_C(0x41c) + (uint32_t)index * 2u,
                               scsp_block[index]) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
    }
    if (vf2_scsp_write_u16(&board->scsp, UINT32_C(0x418), UINT16_C(0xce)) !=
        VF2_OK ||
        vf2_scsp_write_u16(&board->scsp, UINT32_C(0x41a), UINT16_C(0x3c0)) !=
        VF2_OK ||
        vf2_scsp_write_u16(&board->scsp, UINT32_C(0x422), UINT16_C(0x40)) !=
        VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    /* ROM 0x320: the 0x1000+0x3F00 clear is subsumed by the memset
       above; only $1526 = 0xef escapes it. */
    board->sound_ram[VF2_SOUND_SHARED_COUNTER_151E + 8u] = 0xefu;
    /* ROM 0x336 command-descriptor images. */
    if (read_audio_be32(board, UINT32_C(0x603066), &image) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    for (index = 0u; index < UINT32_C(0xa0); ++index) {
        uint8_t staged = 0u;
        if (read_audio_byte_absolute(board, image + (uint32_t)index, &staged) !=
            VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        board->sound_ram[VF2_SOUND_COMMAND_DESCRIPTOR_BASE + index] = staged;
    }
    if (read_audio_be32(board, UINT32_C(0x6030a6), &image) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    for (index = 0u; index < UINT32_C(0xa0); ++index) {
        uint8_t staged = 0u;
        if (read_audio_byte_absolute(board, image + (uint32_t)index, &staged) !=
            VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        board->sound_ram[VF2_SOUND_COMMAND_DESCRIPTOR_BASE + 0xa0u + index] =
            staged;
    }
    board->sound_ram[VF2_SOUND_SHARED_FACTOR_151F] = 0xffu;
    for (index = 0u; index < 20u; ++index) {
        uint8_t *descriptor = board->sound_ram +
            VF2_SOUND_COMMAND_DESCRIPTOR_BASE +
            index * VF2_SOUND_COMMAND_DESCRIPTOR_BYTES;
        uint16_t scaled = (uint16_t)(((uint16_t)descriptor[3u] * 0xffu) >> 8u);
        /* ROM d3 = 0xff either way ($151f was just stored). */
        if ((scaled & 0xffu) != 0u) {
            descriptor[0x0au] = (uint8_t)(scaled + 1u);
        }
    }
    /* ROM $151a(a6): voice-table base pointer for the compactor. */
    write_be32(board->sound_ram + UINT32_C(0x251a), UINT32_C(0x2800));
    /* ROM 0x3a8 voice records: slot word = index*0x20, sample longs. */
    for (voice = 0u; voice < VF2_SOUND_VOICE_COUNT; ++voice) {
        uint8_t *record = board->sound_ram + VF2_SOUND_VOICE_TABLE_BASE +
            voice * VF2_SOUND_VOICE_BYTES;
        uint16_t slot = 0u;
        uint32_t sample = 0u;
        if (read_audio_be16(board, UINT32_C(0x6006e6) + (uint32_t)voice * 2u,
                            &slot) != VF2_OK ||
            read_audio_be32(board, UINT32_C(0x600726) + (uint32_t)voice * 4u,
                            &sample) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        write_be16(record, slot);
        write_be32(record + 0x0cu, sample);
    }
    /* ROM 0x3cc stream records. */
    for (voice = 0u; voice < 32u; ++voice) {
        uint8_t *record = board->sound_ram + UINT32_C(0x2000) +
            voice * UINT32_C(0x20);
        write_be32(record + 0x0eu,
                   UINT32_C(0x10000) + (uint32_t)voice * UINT32_C(0x2000));
        record[0x12u] = (uint8_t)((uint8_t)voice * 2u);
        record[0x16u] = (uint8_t)(8u + (uint8_t)voice * 8u);
    }
    /* ROM 0x402 per-slot volumes from the 0x605368 table. */
    if (read_audio_be32(board, UINT32_C(0x60536a), &volume_table) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    for (index = 0u; index < 16u; ++index) {
        uint8_t level = 0u;
        if (read_audio_byte_absolute(board, volume_table + (uint32_t)index,
                                     &level) != VF2_OK ||
            vf2_scsp_write_u8(&board->scsp,
                              UINT32_C(0x17) + (uint32_t)index *
                                  VF2_SCSP_SLOT_BYTES,
                              level) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
    }
    /* ROM 0x422 sample staging (d1 < 0 skips; dance has no RAM effect). */
    return sound_board_sample_upload(board, UINT32_C(0x608df0), 0, 0u, 1);
}

vf2_status vf2_sound_board_read_u8(
    vf2_sound_board *board,
    uint32_t address,
    uint8_t *value
)
{
    if (board == NULL || value == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (address < VF2_SOUND_RAM_BYTES) {
        *value = board->sound_ram[address];
        return VF2_OK;
    }
    if (address >= VF2_SCSP_CPU_BASE &&
        address < VF2_SCSP_CPU_BASE + VF2_SCSP_REGISTER_BYTES) {
        return vf2_scsp_read_u8(&board->scsp,
                                address - VF2_SCSP_CPU_BASE, value);
    }
    if (address == VF2_SCSP_CONTROL_BASE ||
        address == VF2_SCSP_CONTROL_BASE + 1u) {
        *value = (uint8_t)(address == VF2_SCSP_CONTROL_BASE
            ? board->control >> 8u : board->control);
        return VF2_OK;
    }
    if (address >= VF2_SOUND_AUDIO_ROM_BASE &&
        address < VF2_SOUND_AUDIO_ROM_BASE + 0x080000u) {
        return read_rom_byte(board->audio_rom, board->audio_rom_size,
                             address - VF2_SOUND_AUDIO_ROM_BASE, value);
    }
    if (address >= VF2_SOUND_SAMPLE_ROM_BASE &&
        address < VF2_SOUND_SAMPLE_ROM_BASE + VF2_SOUND_SAMPLE_DIRECT_BYTES) {
        return read_rom_byte(board->sample_rom, board->sample_rom_size,
                             address - VF2_SOUND_SAMPLE_ROM_BASE, value);
    }
    return VF2_ERROR_OUT_OF_BOUNDS;
}

vf2_status vf2_sound_board_write_u8(
    vf2_sound_board *board,
    uint32_t address,
    uint8_t value
)
{
    if (board == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (address < VF2_SOUND_RAM_BYTES) {
        board->sound_ram[address] = value;
        return VF2_OK;
    }
    if (address >= VF2_SCSP_CPU_BASE &&
        address < VF2_SCSP_CPU_BASE + VF2_SCSP_REGISTER_BYTES) {
        return vf2_scsp_write_u8(&board->scsp,
                                 address - VF2_SCSP_CPU_BASE, value);
    }
    if (address == VF2_SCSP_CONTROL_BASE) {
        board->control = (uint16_t)((board->control & 0x00ffu) |
                                    ((uint32_t)value << 8u));
        return VF2_OK;
    }
    if (address == VF2_SCSP_CONTROL_BASE + 1u) {
        board->control = (uint16_t)((board->control & 0xff00u) | value);
        return VF2_OK;
    }
    return VF2_ERROR_OUT_OF_BOUNDS;
}

vf2_status vf2_sound_board_read_u16(
    vf2_sound_board *board,
    uint32_t address,
    uint16_t *value
)
{
    uint8_t high = 0u;
    uint8_t low = 0u;
    vf2_status status = VF2_OK;
    if (board == NULL || value == NULL || (address & 1u) != 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (address >= VF2_SCSP_CPU_BASE &&
        address < VF2_SCSP_CPU_BASE + VF2_SCSP_REGISTER_BYTES - 1u) {
        return vf2_scsp_read_u16(&board->scsp,
                                 address - VF2_SCSP_CPU_BASE, value);
    }
    status = vf2_sound_board_read_u8(board, address, &high);
    if (status == VF2_OK) {
        status = vf2_sound_board_read_u8(board, address + 1u, &low);
    }
    if (status == VF2_OK) {
        *value = (uint16_t)(((uint16_t)high << 8u) | low);
    }
    return status;
}

vf2_status vf2_sound_board_write_u16(
    vf2_sound_board *board,
    uint32_t address,
    uint16_t value
)
{
    vf2_status status = VF2_OK;
    if (board == NULL || (address & 1u) != 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (address >= VF2_SCSP_CPU_BASE &&
        address < VF2_SCSP_CPU_BASE + VF2_SCSP_REGISTER_BYTES - 1u) {
        return vf2_scsp_write_u16(&board->scsp,
                                  address - VF2_SCSP_CPU_BASE, value);
    }
    status = vf2_sound_board_write_u8(board, address, (uint8_t)(value >> 8u));
    if (status == VF2_OK) {
        status = vf2_sound_board_write_u8(board, address + 1u,
                                          (uint8_t)value);
    }
    return status;
}

vf2_status vf2_sound_board_maintain_voices(
    vf2_sound_board *board,
    vf2_sound_voice_maintenance_report *report
)
{
    vf2_sound_voice_maintenance_report local = {0};
    uint8_t *voices = NULL;
    size_t index = 0u;

    if (board == NULL || report == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    /* The ROM writes the SCSP timer/control words on entry (L1 ISR
       0x602: $041a = 0x3c0, $0422 = 0x80; the L2 driver pokes $0418 /
       $0422 per tick and is otherwise an unmodeled interrupt
       boundary). */
    if (vf2_scsp_write_u16(
            &board->scsp, VF2_SCSP_MASTER_BASE + 0x001au, 0x03c0u
        ) != VF2_OK ||
        vf2_scsp_write_u16(
            &board->scsp, VF2_SCSP_MASTER_BASE + 0x0022u, 0x0080u
        ) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    local.counter_151e_before =
        board->sound_ram[VF2_SOUND_SHARED_COUNTER_151E];
    local.counter_152e_before =
        board->sound_ram[VF2_SOUND_SHARED_COUNTER_152E];
    voices = board->sound_ram + VF2_SOUND_VOICE_TABLE_BASE;
    for (index = 0u; index < VF2_SOUND_VOICE_COUNT; ++index) {
        uint8_t *voice = voices + index * VF2_SOUND_VOICE_BYTES;
        uint8_t age = 0u;
        int release_path = 0;
        size_t other = 0u;

        ++local.voices_scanned;
        if ((voice[2u] & 1u) == 0u) {
            continue;
        }
        --voice[0x0bu];
        if (voice[0x0bu] != 0u) {
            continue;
        }
        release_path = (voice[2u] & 8u) != 0u;
        {
            vf2_status status = sound_board_key_off(board, voice);
            if (status != VF2_OK) {
                return status;
            }
        }
        voice[2u] = 0u;
        ++local.voices_expired;
        if (release_path) {
            ++local.release_expirations;
        } else {
            ++local.normal_expirations;
        }

        if (!release_path) {
            age = voice[9u];
            voice[9u] = 0u;
            --board->sound_ram[VF2_SOUND_SHARED_COUNTER_151E];
            /* ROM compares the full d2.W (move.b-loaded age over
               interrupt-residue high bytes) against the word at +9;
               the C keeps the clean zero-extended age. Residue modeling
               would need the guest register file, and no 68k oracle
               exists to measure it: documented, not modeled. */
            for (other = 0u; other < VF2_SOUND_VOICE_COUNT; ++other) {
                uint8_t *candidate = voices + other * VF2_SOUND_VOICE_BYTES;
                if (candidate[9u] != 0xffu &&
                    (uint16_t)age < read_be16(candidate + 9u)) {
                    --candidate[9u];
                    ++local.voices_aged;
                }
            }
        } else {
            age = voice[5u];
            voice[9u] = 0u;
            voice[5u] = 0u;
            --board->sound_ram[VF2_SOUND_SHARED_COUNTER_152E];
            for (other = 0u; other < VF2_SOUND_VOICE_COUNT; ++other) {
                uint8_t *candidate = voices + other * VF2_SOUND_VOICE_BYTES;
                if (candidate[9u] == 0xffu &&
                    (uint16_t)age < read_be16(candidate + 5u)) {
                    --candidate[5u];
                    ++local.voices_aged;
                }
            }
        }
    }
    local.counter_151e_after =
        board->sound_ram[VF2_SOUND_SHARED_COUNTER_151E];
    local.counter_152e_after =
        board->sound_ram[VF2_SOUND_SHARED_COUNTER_152E];
    *report = local;
    return VF2_OK;
}

vf2_status vf2_sound_board_maintain_streams(
    vf2_sound_board *board,
    vf2_sound_stream_maintenance_report *report
)
{
    /* The ROM sequence engine (driver 0x1f16, worker 0x1f7c, prescaler
       0x21ba, appliers, tail) never runs: the driver has no static
       entry anywhere in the image, and every slot's +0x0c byte is
       boot-cleared and never stored nonzero, so the prescaler would
       abort to the main loop at slot 0 regardless. Packet flow on ROM
       is main-CPU-authored (drain-consumed); the 68k never sequences.
       The old per-descriptor worker model matched none of the ROM
       applier bytes and is removed. Faithful behavior here is no
       state change; the cursors are reported to document stillness. */
    if (board == NULL || report == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *report = (vf2_sound_stream_maintenance_report){0};
    report->write_cursor_before = read_be16(
        board->sound_ram + VF2_SOUND_COMMAND_WRITE_CURSOR
    );
    report->write_cursor_after = report->write_cursor_before;
    return VF2_OK;
}

static vf2_status sound_board_clear_c0_voices(
    vf2_sound_board *board,
    uint8_t channel,
    vf2_sound_dispatch_report *report
)
{
    uint8_t *voices = board->sound_ram + VF2_SOUND_VOICE_TABLE_BASE;
    size_t index = 0u;

    /* ROM 0x1de0 head: channel control/table clear. d1 = channel
       ($150e&0xf = packet command&0xf, set by the drain). */
    board->sound_ram[VF2_SOUND_CHANNEL_CONTROL_BASE + channel] = 0u;
    board->sound_ram[VF2_SOUND_CHANNEL_CONTROL_ALT_BASE + channel] = 0u;
    write_be16(board->sound_ram + VF2_SOUND_CHANNEL_TABLE_BASE +
               (size_t)channel * 2u, 0u);
    for (index = 0u; index < VF2_SOUND_VOICE_COUNT; ++index) {
        uint8_t *voice = voices + index * VF2_SOUND_VOICE_BYTES;
        uint16_t slot_word = 0u;
        uint32_t scsp_base = 0u;
        uint32_t stream_slot = 0u;
        if ((voice[2u] & 0x40u) == 0u ||
            read_be16(voice + 4u) != (uint16_t)channel) {
            continue;
        }
        /* ROM 0x1e24-0x1e38 (V5 key form): d4 = voice[0], the slot
           word, used directly as the stride. SCSP[slot+0x0b] = 0x1f,
           SCSP[slot+0x00] = 0x10, sound_ram[0x2000+slot] = 0 (a3 =
           0x1000(a6)). The earlier absolute-write form came from the
           same indexed-#imm decoder misread as the key_off fiction.
           The $151e decrement is unconditional; there is no age
           rebalance on this path. */
        slot_word = read_be16(voice);
        scsp_base = (uint32_t)slot_word;
        stream_slot = UINT32_C(0x2000) + (uint32_t)slot_word;
        if (vf2_scsp_write_u8(&board->scsp, scsp_base + 0x0bu,
                              UINT8_C(0x1f)) != VF2_OK ||
            vf2_scsp_write_u8(&board->scsp, scsp_base,
                              UINT8_C(0x10)) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        if (stream_slot >= VF2_SOUND_RAM_BYTES) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        board->sound_ram[stream_slot] = 0u;
        voice[2u] = 0u;
        voice[4u] = 0u;
        --board->sound_ram[VF2_SOUND_SHARED_COUNTER_151E];
        ++report->voices_cleared;
    }
    return VF2_OK;
}

static vf2_status sound_board_dispatch_80(
    vf2_sound_board *board,
    const uint8_t *descriptor,
    uint8_t command,
    uint8_t payload,
    vf2_sound_dispatch_report *report
)
{
    const uint8_t channel = (uint8_t)(command & 0x0fu);
    /* ROM 0x10d2 (the switch skips the fetch primer): d2 = command.
       Base = (command&0x7f) + desc[4..5]-word; d0.upper is clean
       (drain andi.l + move.b chain), so the word add is exact and the
       low byte escapes to $150f. Payload/byte2 are never read. */
    const uint16_t base =
        (uint16_t)((uint16_t)(command & 0x7fu) + read_be16(descriptor + 4u));

    (void)payload;
    report->handler_address = UINT32_C(0x000010d2);
    report->channel = channel;
    board->sound_ram[VF2_SOUND_LOOKUP_CURRENT] = (uint8_t)base;
    /* The only matchable shape (drain matches desc[1..2].w ==
       channel <= 15, so desc[1] = 0 and desc[2] <= 15) takes the low
       arm, which is a stale branch into mid-lea (bcs lands inside
       the row-table lea). The high arm needs desc[2] >= 0xf0 and
       cannot match the drain. Everything past (row/$1515/voice
       update) is unreachable. Refuse. */
    return VF2_ERROR_UNSUPPORTED;
}

static vf2_status sound_board_dispatch_90(
    vf2_sound_board *board,
    uint8_t *descriptor,
    uint8_t command,
    uint8_t payload,
    uint8_t ring_byte2,
    vf2_sound_dispatch_report *report
)
{
    /* ROM class switch: 0x90-class enters at 0x85a with d2 = command;
       default classes run the 0x858 primer with d2 = payload. The
       second byte is payload (0x90, a3 = +1 peek) or ring byte 2
       (default, a3 = +2 after the primer fetch). */
    const uint8_t channel = (uint8_t)(command & 0x0fu);
    const int high_class = (command & 0xf0u) == 0x90u;
    const uint8_t head = high_class ? command : payload;
    const uint8_t second = high_class ? payload : ring_byte2;
    uint16_t base_full = 0u;
    uint8_t mode = 0u;

    report->handler_address =
        high_class ? UINT32_C(0x0000085a) : UINT32_C(0x00000858);
    report->channel = channel;
    /* ROM 0x85c: base = d2 + desc[4] (d3.mid = 0 proven: boot clr.l,
       only byte-writes and the 0x23d2 record-long reload on the loop
       path, whose +0x17 byte is never stored). Low byte escapes. */
    base_full = (uint16_t)((uint16_t)head + descriptor[4u]);
    board->sound_ram[VF2_SOUND_LOOKUP_CURRENT] = (uint8_t)base_full;
    /* ROM 0x866: second == 0 continues into the 80 body
       mid-instruction (0x10e4): trap on HW. */
    if (second == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    /* ROM 0x86c mode table. d0.mid = ((head+desc[4])>>8) with a clean
       upper (drain andi.l + move.b chain), so the long index is exact
       even past the 128-byte table. */
    if (read_audio_byte_absolute(
            board,
            UINT32_C(0x60c6a6) +
                ((uint32_t)(base_full >> 8u) << 16u) + second,
            &mode
        ) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    board->sound_ram[VF2_SOUND_LOOKUP_MODE] = mode;
    report->selected_value = mode;
    /* What follows in ROM is never reachable on a matched descriptor:
       the low arm (desc[2] < 0xf0, the only matchable shape since the
       drain matches desc[1..2].w == channel <= 15) is a stale branch
       into mid-lea, and the high arm needs desc[2] >= 0xf0, which
       cannot match. Everything past (scan/compact/kill/alloc, and the
       0x8d2 mid-instruction trap) is post-trap. Refuse. */
    return VF2_ERROR_UNSUPPORTED;
}

static vf2_status sound_board_dispatch_a0(
    vf2_sound_board *board,
    uint8_t command,
    vf2_sound_dispatch_report *report
)
{
    (void)board;
    /* ROM switch jmps to the 0x1e74 fetch primer, but the body splits
       on cmp.l d2,d0 with d2 never loaded on the A0 path: d2.upper is
       main-loop/driver residue (worker chain-loads reach nonzero high
       words), so stale-vs-primary mis-splits by history and the stale
       arm then stores through residue-a2. Unmodelable: refuse at
       entry. Formulas (row = payload&0x7f, value = byte2, no-op
       primary, slot0-stale, bit7 trap) kept in the master note. */
    report->handler_address = UINT32_C(0x00001e74);
    report->channel = (uint8_t)(command & 0x0fu);
    return VF2_ERROR_UNSUPPORTED;
}

/* ROM 0x1356 per-voice tail (e0, channel < 9 matches). Residue audit
   (master note): d0/d2/d5/d6 lows are deterministic (mem, table, or
   cleared); stale uppers die in andi.w masks or in the discarded d2/d4
   words. Writes SCSP+0x10 and sound_ram[0x601a] (0x2000+0x401a: live
   RAM, not ROM space). The lsl.b #5 offset wraps at the byte. */
static vf2_status sound_board_e0_voice_tail(
    vf2_sound_board *board,
    const uint8_t *voice,
    const uint8_t *descriptor,
    uint16_t shared
)
{
    int16_t addend = 0;
    int32_t slide = 0;
    uint8_t voice_nibble = 0u;
    uint32_t table = 0u;
    int64_t slot_at = 0;
    uint16_t picker = 0u;
    uint16_t shaped = 0u;
    uint8_t row = 0u;
    uint32_t row_table = 0u;
    uint32_t offset = 0u;
    int64_t store_at = 0;
    uint16_t stored = 0u;

    /* d3 = voice[10] + desc[5..6]-word, sign-extended; d5 = $1524-word
       plus d3, sign-extended (ext.l drops the caller-upper). */
    addend = (int16_t)(uint16_t)((uint16_t)voice[0x0au] +
                                 read_be16(descriptor + 5u));
    slide = (int32_t)(int16_t)(uint16_t)(shared + (uint16_t)addend);
    voice_nibble = (uint8_t)(voice[3u] & 0x0fu);
    if (read_audio_be32(board, UINT32_C(0x602676) +
                        (uint32_t)voice_nibble * 4u, &table) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    slot_at = (int64_t)table + 2LL * (int64_t)slide;
    if (slot_at < (int64_t)VF2_SOUND_AUDIO_ROM_BASE ||
        (uint64_t)slot_at + 2u >
            (uint64_t)VF2_SOUND_AUDIO_ROM_BASE + board->audio_rom_size) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    if (read_audio_be16(board, (uint32_t)slot_at, &picker) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    /* d0 = (voice[3]&0xf0)<<7; add.l table-word; andi.w #$7bff (upper
       never escapes: only the low word is stored). */
    shaped = (uint16_t)((((uint16_t)(voice[3u] & 0xf0u) << 7u) + picker) &
                        UINT16_C(0x7bff));
    if (vf2_scsp_write_u16(&board->scsp, UINT32_C(0x10), shaped) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    row = (uint8_t)(((uint8_t)(voice[3u] & 0xf0u) >> 2u) & 0x3cu);
    if (read_audio_be32(board, UINT32_C(0x60c726) + row, &row_table) !=
        VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    offset = ((uint32_t)(voice[3u] & 0x0fu) << 5u) & 0xffu;
    store_at = (int64_t)row_table + (int64_t)offset + 2LL * (int64_t)slide;
    if (store_at < (int64_t)VF2_SOUND_AUDIO_ROM_BASE ||
        (uint64_t)store_at + 2u >
            (uint64_t)VF2_SOUND_AUDIO_ROM_BASE + board->audio_rom_size) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    if (read_audio_be16(board, (uint32_t)store_at, &stored) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    write_be16(board->sound_ram + UINT32_C(0x601a), stored);
    return VF2_OK;
}

static vf2_status sound_board_dispatch_e0(
    vf2_sound_board *board,
    uint8_t *descriptor,
    uint8_t command,
    uint8_t payload,
    vf2_sound_dispatch_report *report
)
{
    /* ROM switch enters at 0x12b6 (primer skipped): d2 = command,
       d0 = payload peek. Combined = (command&0x7f)|((payload&0x7f)<<7),
       >> 5. d0's upper at the table read is ((payload&0x7f)<<7)&0xff00
       (drain-zeroed upper + move.b preserves): it rides into the
       stored word on BOTH paths, negated on the even path. */
    uint16_t combined = (uint16_t)((command & 0x7fu) |
                                   ((uint32_t)(payload & 0x7fu) << 7u));
    uint16_t transformed = (uint16_t)(combined >> 5u);
    uint16_t stale =
        (uint16_t)(((uint32_t)(payload & 0x7fu) << 7u) & 0xff00u);
    uint8_t table_index = (uint8_t)transformed;
    uint8_t table_number = (uint8_t)(descriptor[7u] & 0x0fu);
    uint32_t table_pointer = 0u;
    uint8_t selected = 0u;
    uint16_t result = 0u;
    uint16_t with_stale = 0u;
    uint8_t channel = (uint8_t)(command & 0x0fu);
    size_t index = 0u;

    /* ROM neg.b/subq.b normalization: the index is the POST-negation
       low byte (0xFF when it normalizes to zero). */
    if ((transformed & 1u) == 0u) {
        table_index = (uint8_t)(0u - table_index);
        if (table_index == 0u) {
            table_index = UINT8_MAX;
        }
    }
    if (read_audio_be32(
            board,
            UINT32_C(0x602b26) + (uint32_t)table_number * 4u,
            &table_pointer
        ) != VF2_OK ||
        read_audio_byte_absolute(
            board, table_pointer + table_index, &selected
        ) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    with_stale = (uint16_t)(stale | selected);
    result = (transformed & 1u) != 0u
        ? with_stale
        : (uint16_t)(0u - with_stale);
    board->sound_ram[VF2_SOUND_SHARED_COUNTER_151E + 6u] =
        (uint8_t)(result >> 8u);
    board->sound_ram[VF2_SOUND_SHARED_COUNTER_151E + 7u] =
        (uint8_t)result;
    board->sound_ram[VF2_SOUND_CHANNEL_TABLE_BASE +
                     (size_t)channel * 2u] =
        (uint8_t)(result >> 8u);
    board->sound_ram[VF2_SOUND_CHANNEL_TABLE_BASE + 1u +
                     (size_t)channel * 2u] =
        (uint8_t)result;
    report->channel = channel;
    report->selected_value = selected;
    report->signed_value = (int16_t)result;
    report->handler_address = UINT32_C(0x000012b6);

    /* ROM bcc on channel >= 9 falls into the B0 primer, whose table
       byte 0 is 0x00 on all 16 tables (measured): B0-default rts.
       Benign; the early return stands on that evidence. */
    if (channel >= 9u) {
        return VF2_OK;
    }
    for (index = 0u; index < VF2_SOUND_VOICE_COUNT; ++index) {
        uint8_t *voice = board->sound_ram + VF2_SOUND_VOICE_TABLE_BASE +
            index * VF2_SOUND_VOICE_BYTES;
        if (read_be16(voice + 4u) == channel &&
            (voice[2u] & 0x7fu) != 0u) {
            vf2_status status =
                sound_board_e0_voice_tail(board, voice, descriptor, result);
            if (status != VF2_OK) {
                return status;
            }
            ++report->voices_matched;
        }
    }
    return VF2_OK;
}

static vf2_status sound_board_dispatch_b0(
    vf2_sound_board *board,
    uint8_t command,
    uint8_t payload,
    vf2_sound_dispatch_report *report
)
{
    /* ROM switch enters at 0x13cc (primer skipped): d2 = command. The
       table index is command&0x7f; for B0-class commands that is
       0x30-0x3f, of which only 0x3a is live (-> 0x1ac0 sample-upload,
       19-target table measured). Payload/byte2 select nothing here:
       the 0x1ac0 body reads d0 = payload (the (a3) peek). */
    const uint8_t channel = (uint8_t)(command & 0x0fu);
    const uint8_t entry = (uint8_t)(command & 0x7fu);

    report->handler_address = UINT32_C(0x000013cc);
    report->channel = channel;
    report->selected_value = entry;
    if (entry != 0x3au) {
        /* Table default (0x13dc rts): consume, no effects. */
        return VF2_OK;
    }
    /* ROM 0x1ac0: andi.w #$7f, 0x7f-trap (mid-dbf, stale +2), row by
       payload, shared sample uploader. */
    return sound_board_sample_upload(
        board, UINT32_C(0x608df0) + (uint32_t)(payload & 0x7fu) * 4u,
        1, payload, 0);
}

vf2_status vf2_sound_board_dispatch_next(
    vf2_sound_board *board,
    vf2_sound_dispatch_report *report
)
{
    vf2_sound_dispatch_report local = {0};
    uint8_t *ram = NULL;
    uint16_t cursor = 0u;
    uint8_t count = 0u;
    size_t index = 0u;

    if (board == NULL || report == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    ram = board->sound_ram;
    count = ram[VF2_SOUND_COMMAND_COUNT];
    cursor = (uint16_t)(read_be16(ram + VF2_SOUND_COMMAND_CURSOR) &
                        UINT16_C(0x0fffu));
    local.cursor_before = cursor;
    local.cursor_after = cursor;
    if (count == 0u) {
        local.cursor_after = cursor;
        *report = local;
        return VF2_OK;
    }
    local.command = ram[VF2_SOUND_COMMAND_RING_BASE + cursor];
    local.payload = ram[VF2_SOUND_COMMAND_RING_BASE +
                        ((cursor + 1u) & UINT16_C(0x0fffu))];
    local.command_class = (uint8_t)(local.command & 0xf0u);
    ram[VF2_SOUND_CURRENT_COMMAND] = local.command;
    for (index = 0u; index < VF2_SOUND_COMMAND_DESCRIPTOR_COUNT; ++index) {
        uint8_t *descriptor = ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE +
            index * VF2_SOUND_COMMAND_DESCRIPTOR_BYTES;
        ++local.descriptors_scanned;
        if ((descriptor[0] & 0x80u) == 0u ||
            read_be16(descriptor + 1u) !=
                (uint16_t)(local.command & 0x0fu)) {
            continue;
        }
        ++local.descriptors_matched;
        /* The ROM class switch (0x81a) has no ignore arm: any class
           other than 0x80/0xa0/0xb0/0xc0/0xe0 falls through into the
           0x90 head at 0x858 (bne.w to 0x858).  There is no
           fail-closed class gate in the original. */
        if (local.command_class == 0x80u) {
            vf2_status status = sound_board_dispatch_80(
                board, descriptor, local.command, local.payload, &local
            );
            if (status != VF2_OK) {
                *report = local;
                return status;
            }
        } else if (local.command_class == 0x90u ||
                   /* ROM default arm: every other class, including
                      0x70/0xd0/0xf0, enters the 0x90 head. */
                   (local.command_class != 0x80u &&
                    local.command_class != 0xa0u &&
                    local.command_class != 0xb0u &&
                    local.command_class != 0xc0u &&
                    local.command_class != 0xe0u)) {
            vf2_status status = sound_board_dispatch_90(
                board, descriptor, local.command, local.payload,
                ram[VF2_SOUND_COMMAND_RING_BASE +
                    ((cursor + 2u) & UINT16_C(0x0fffu))], &local
            );
            if (status != VF2_OK) {
                *report = local;
                return status;
            }
        } else if (local.command_class == 0xa0u) {
            vf2_status status = sound_board_dispatch_a0(
                board, local.command, &local
            );
            if (status != VF2_OK) {
                *report = local;
                return status;
            }
        } else if (local.command_class == 0xb0u) {
            vf2_status status = sound_board_dispatch_b0(
                board, local.command, local.payload, &local
            );
            if (status != VF2_OK) {
                *report = local;
                return status;
            }
        } else if (local.command_class == 0xc0u) {
            /* ROM 0x1e5c (primer skipped): d2 = command. bset #7 for
               command >= 0x70, always true for c0-class (the bcs arm
               is unreachable by routing). */
            descriptor[2] = (uint8_t)(local.command | 0x80u);
            local.handler_address = UINT32_C(0x00001e5c);
            if (sound_board_clear_c0_voices(
                    board, (uint8_t)(local.command & 0x0fu), &local
                ) != VF2_OK) {
                *report = local;
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
        } else {
            vf2_status status = sound_board_dispatch_e0(
                board, descriptor, local.command, local.payload, &local
            );
            if (status != VF2_OK) {
                *report = local;
                return status;
            }
        }
    }
    ram[VF2_SOUND_COMMAND_COUNT] = (uint8_t)(count - 1u);
    cursor = (uint16_t)((cursor + 4u) & UINT16_C(0x0fffu));
    ram[VF2_SOUND_COMMAND_CURSOR] = (uint8_t)(cursor >> 8u);
    ram[VF2_SOUND_COMMAND_CURSOR + 1u] = (uint8_t)cursor;
    local.cursor_after = cursor;
    local.handled = 1;
    *report = local;
    return VF2_OK;
}

vf2_status vf2_sound_board_emit_command(
    vf2_sound_board *board,
    const uint8_t packet[4]
)
{
    uint8_t *ram = NULL;
    uint16_t cursor = 0u;
    size_t index = 0u;

    if (board == NULL || packet == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (board->sound_ram[VF2_SOUND_COMMAND_COUNT] == UINT8_MAX) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    ram = board->sound_ram;
    cursor = (uint16_t)(read_be16(ram + VF2_SOUND_COMMAND_WRITE_CURSOR) &
                        UINT16_C(0x0fffu));
    for (index = 0u; index < 4u; ++index) {
        ram[VF2_SOUND_COMMAND_RING_BASE +
            ((cursor + (uint16_t)index) & UINT16_C(0x0fffu))] = packet[index];
    }
    cursor = (uint16_t)((cursor + 4u) & UINT16_C(0x0fffu));
    ram[VF2_SOUND_COMMAND_WRITE_CURSOR] = (uint8_t)(cursor >> 8u);
    ram[VF2_SOUND_COMMAND_WRITE_CURSOR + 1u] = (uint8_t)cursor;
    ++ram[VF2_SOUND_COMMAND_COUNT];
    return VF2_OK;
}

vf2_status vf2_sound_board_render(
    vf2_sound_board *board,
    int16_t *left,
    int16_t *right,
    size_t frames
)
{
    if (board == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    return vf2_scsp_render(&board->scsp, left, right, frames);
}
