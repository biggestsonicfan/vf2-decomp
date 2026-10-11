#include "vf2/sound_board.h"

#include <string.h>

/* ROM table addressed by the 0x174c B0 handler's PC-relative lookup at
   audio-CPU address 0x179e. */
static const uint8_t sound_board_b0_entry_10_table[128] = {
    0x1fu, 0x1fu, 0x1eu, 0x1eu, 0x1eu, 0x1eu, 0x1du, 0x1du,
    0x1du, 0x1du, 0x1cu, 0x1cu, 0x1cu, 0x1cu, 0x1bu, 0x1bu,
    0x1bu, 0x1bu, 0x1au, 0x1au, 0x1au, 0x1au, 0x19u, 0x19u,
    0x19u, 0x19u, 0x18u, 0x18u, 0x18u, 0x18u, 0x17u, 0x17u,
    0x17u, 0x17u, 0x16u, 0x16u, 0x16u, 0x16u, 0x15u, 0x15u,
    0x15u, 0x15u, 0x14u, 0x14u, 0x14u, 0x14u, 0x13u, 0x13u,
    0x13u, 0x13u, 0x12u, 0x12u, 0x12u, 0x12u, 0x11u, 0x11u,
    0x11u, 0x11u, 0x10u, 0x10u, 0x10u, 0x10u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x01u, 0x01u, 0x01u, 0x01u, 0x02u, 0x02u,
    0x02u, 0x02u, 0x03u, 0x03u, 0x03u, 0x03u, 0x04u, 0x04u,
    0x04u, 0x04u, 0x05u, 0x05u, 0x05u, 0x05u, 0x06u, 0x06u,
    0x06u, 0x06u, 0x07u, 0x07u, 0x07u, 0x07u, 0x08u, 0x08u,
    0x08u, 0x08u, 0x09u, 0x09u, 0x09u, 0x09u, 0x0au, 0x0au,
    0x0au, 0x0au, 0x0bu, 0x0bu, 0x0bu, 0x0bu, 0x0cu, 0x0cu,
    0x0cu, 0x0cu, 0x0du, 0x0du, 0x0du, 0x0du, 0x0eu, 0x0eu,
    0x0eu, 0x0eu, 0x0fu, 0x0fu, 0x0fu, 0x0fu, 0xe3u, 0x08u
};

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
    const uint16_t slot = (uint16_t)(read_be16(voice) &
                                     (VF2_SCSP_SLOT_COUNT - 1u));
    uint16_t control = 0u;
    vf2_status status = vf2_scsp_read_u16(
        &board->scsp, (uint32_t)slot * VF2_SCSP_SLOT_BYTES, &control
    );
    if (status != VF2_OK) {
        return status;
    }
    return vf2_scsp_write_u16(
        &board->scsp,
        (uint32_t)slot * VF2_SCSP_SLOT_BYTES,
        (uint16_t)(control & (uint16_t)~UINT16_C(0x0800))
    );
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
    /* The ROM writes the SCSP timer/control words on entry. */
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

static vf2_status sound_board_process_stream_descriptor(
    vf2_sound_board *board,
    uint8_t *descriptor,
    vf2_sound_stream_maintenance_report *report
)
{
    uint16_t timer = 0u;
    uint32_t source_start = 0u;
    uint32_t source = 0u;
    uint8_t first = 0u;
    uint8_t command = 0u;
    uint8_t value = 0u;
    uint8_t packet[4] = {0u, 0u, 0u, 0u};
    uint8_t high_nibble = 0u;
    size_t pointer_reentries = 0u;
    vf2_status status = VF2_OK;

    if ((descriptor[0] & 0x80u) == 0u) {
        return VF2_OK;
    }
    timer = read_be16(descriptor + 2u);
    if (timer == 0u) {
        return VF2_OK;
    }
    --timer;
    write_be16(descriptor + 2u, timer);
    if (timer != 0u) {
        return VF2_OK;
    }
    ++report->timers_expired;

decode_source:
    source_start = read_be32(descriptor + 4u);
    source = source_start;
    if (read_audio_byte_absolute(board, source, &first) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    ++source;
    descriptor[0] = (uint8_t)(descriptor[0] & UINT8_C(0xfe));
    if ((first & 0x80u) == 0u) {
        command = descriptor[1u];
        source = source_start;
    } else {
        command = first;
    }
    packet[0] = command;
    high_nibble = (uint8_t)(command & 0xf0u);
    if (high_nibble == 0xf0u && command == 0xf7u) {
        uint8_t skip = 0u;
        if (read_audio_byte_absolute(board, source, &skip) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        source += 1u + skip;
        ++report->escape_records;
        if ((descriptor[0] & 1u) == 0u) {
            uint8_t next_timer = 0u;
            if (read_audio_byte_absolute(board, source, &next_timer) != VF2_OK) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
            ++source;
            if (next_timer != 0u) {
                if ((next_timer & 0x80u) != 0u) {
                    uint8_t low_timer = 0u;
                    if (read_audio_byte_absolute(board, source, &low_timer) != VF2_OK) {
                        return VF2_ERROR_OUT_OF_BOUNDS;
                    }
                    ++source;
                    timer = (uint16_t)(((uint16_t)(next_timer & 0x7fu) << 7u) |
                                       low_timer);
                } else {
                    timer = next_timer;
                }
                write_be16(descriptor + 2u, timer);
            }
        }
        write_be32(descriptor + 4u, source);
        report->bytes_consumed += (size_t)(source - source_start);
        return VF2_OK;
    }
    if (high_nibble == 0xf0u && command == 0xf0u) {
        /* The ROM's F0 record is a wait/search marker: consume source bytes
           through the next F7 sentinel, then resume at the continuation
           timer without putting a packet in the command ring. */
        do {
            if (read_audio_byte_absolute(board, source, &value) != VF2_OK) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
            ++source;
        } while (value != 0xf7u);
        ++report->escape_records;
        if ((descriptor[0] & 1u) == 0u) {
            uint8_t next_timer = 0u;
            if (read_audio_byte_absolute(board, source, &next_timer) != VF2_OK) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
            ++source;
            if (next_timer != 0u) {
                if ((next_timer & 0x80u) != 0u) {
                    uint8_t low_timer = 0u;
                    if (read_audio_byte_absolute(board, source, &low_timer) != VF2_OK) {
                        return VF2_ERROR_OUT_OF_BOUNDS;
                    }
                    ++source;
                    timer = (uint16_t)(((uint16_t)(next_timer & 0x7fu) << 7u) |
                                       low_timer);
                } else {
                    timer = next_timer;
                }
                write_be16(descriptor + 2u, timer);
            }
        }
        write_be32(descriptor + 4u, source);
        report->bytes_consumed += (size_t)(source - source_start);
        return VF2_OK;
    }
    if (high_nibble == 0xf0u && command == 0xffu) {
        uint8_t marker = 0u;
        uint8_t skip = 0u;
        if (read_audio_byte_absolute(board, source, &marker) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        ++source;
        if (marker == 0x2fu) {
            uint32_t chain_address = 0u;
            uint32_t chain_word = 0u;
            ++report->escape_records;
            if ((descriptor[0] & 0x08u) == 0u) {
                descriptor[0] = 0u;
                report->bytes_consumed += (size_t)(source - source_start);
                return VF2_OK;
            }
            chain_address = read_be32(descriptor + 8u);
            if (read_audio_be32(board, chain_address, &chain_word) != VF2_OK) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
            if (chain_word == UINT32_C(0xffffffff)) {
                /* 0x215a clears this stream when the chain is exhausted. */
                descriptor[0] = 0u;
                report->bytes_consumed += (size_t)(source - source_start);
                return VF2_OK;
            }
            if (chain_word == UINT32_C(0xfffffff2)) {
                /* The F2 sentinel clears all 64 longwords at 0x2000. */
                memset(
                    board->sound_ram + VF2_SOUND_DESCRIPTOR_BASE,
                    0,
                    VF2_SOUND_STREAM_MAINTENANCE_COUNT *
                        VF2_SOUND_STREAM_DESCRIPTOR_BYTES
                );
                report->bytes_consumed += (size_t)(source - source_start);
                return VF2_OK;
            }
            if (chain_word == UINT32_C(0xfffffff1)) {
                uint32_t nested_pointer = 0u;
                uint32_t pointed_source = 0u;
                if (read_audio_be32(
                        board, chain_address + 4u, &nested_pointer
                    ) != VF2_OK ||
                    read_audio_be32(
                        board, nested_pointer, &pointed_source
                    ) != VF2_OK) {
                    return VF2_ERROR_OUT_OF_BOUNDS;
                }
                write_be32(descriptor + 8u, nested_pointer + 4u);
                write_be32(descriptor + 4u, pointed_source);
                ++report->pointer_handoffs;
                report->bytes_consumed += (size_t)(source - source_start);
                if (++pointer_reentries > 8u) {
                    return VF2_ERROR_UNSUPPORTED;
                }
                /* 0x219c loads a3 from the nested source record and branches
                   back to the common 0x1f98 decoder. */
                goto decode_source;
            }
            write_be32(descriptor + 8u, chain_address + 4u);
            write_be32(descriptor + 4u, chain_word);
            ++report->pointer_handoffs;
            report->bytes_consumed += (size_t)(source - source_start);
            if (++pointer_reentries > 8u) {
                return VF2_ERROR_UNSUPPORTED;
            }
            /* 0x2188 loads a3 from the ordinary chain record and branches
               back to the common 0x1f98 decoder. */
            goto decode_source;
        }
        if (read_audio_byte_absolute(board, source, &skip) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        ++source;
        source += skip;
        ++report->escape_records;
        if ((descriptor[0] & 1u) == 0u) {
            uint8_t next_timer = 0u;
            if (read_audio_byte_absolute(board, source, &next_timer) != VF2_OK) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
            ++source;
            if (next_timer != 0u) {
                if ((next_timer & 0x80u) != 0u) {
                    uint8_t low_timer = 0u;
                    if (read_audio_byte_absolute(board, source, &low_timer) != VF2_OK) {
                        return VF2_ERROR_OUT_OF_BOUNDS;
                    }
                    ++source;
                    timer = (uint16_t)(((uint16_t)(next_timer & 0x7fu) << 7u) |
                                       low_timer);
                } else {
                    timer = next_timer;
                }
                write_be16(descriptor + 2u, timer);
            }
        }
        write_be32(descriptor + 4u, source);
        report->bytes_consumed += (size_t)(source - source_start);
        return VF2_OK;
    }
    if (high_nibble == 0xb0u) {
        if (read_audio_byte_absolute(board, source, &value) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        ++source;
        packet[1] = value;
        if (value == 0x10u) {
            if (read_audio_byte_absolute(board, source, &value) != VF2_OK) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
            ++source;
            if (descriptor[12u] != 0u) {
                value = descriptor[14u];
            } else {
                descriptor[14u] = (uint8_t)(value & 0x7fu);
                value = descriptor[14u];
                ++report->control_updates;
            }
        }
        packet[1] = (uint8_t)(packet[1] & 0x7fu);
        packet[2] = (uint8_t)(value & 0x7fu);
        if ((value & 0x80u) != 0u) {
            descriptor[0] |= 1u;
        }
    } else if (high_nibble == 0xf0u) {
        /* Remaining F0 escape/control records have separate pointer/control
           flows not yet modeled by this boundary. */
        return VF2_ERROR_UNSUPPORTED;
    } else if (high_nibble == 0x20u) {
        if (read_audio_byte_absolute(board, source, &value) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        ++source;
        packet[1] = (uint8_t)(value & 0x7fu);
        packet[2] = 0x7fu;
        if ((value & 0x80u) != 0u) {
            descriptor[0] |= 1u;
        }
    } else if (high_nibble == 0xc0u || high_nibble == 0xd0u) {
        if (read_audio_byte_absolute(board, source, &value) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        ++source;
        packet[1] = (uint8_t)(value & 0x7fu);
        if ((value & 0x80u) != 0u) {
            descriptor[0] |= 1u;
        }
    } else {
        if (read_audio_byte_absolute(board, source, &packet[1]) != VF2_OK ||
            read_audio_byte_absolute(board, source + 1u, &value) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        source += 2u;
        packet[2] = (uint8_t)(value & 0x7fu);
        if ((value & 0x80u) != 0u) {
            descriptor[0] |= 1u;
        }
    }
    status = vf2_sound_board_emit_command(board, packet);
    if (status != VF2_OK) {
        return status;
    }
    ++report->packets_emitted;

    if ((descriptor[0] & 1u) == 0u) {
        uint8_t next_timer = 0u;
        if (read_audio_byte_absolute(board, source, &next_timer) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        ++source;
        if (next_timer != 0u) {
            if ((next_timer & 0x80u) != 0u) {
                uint8_t low_timer = 0u;
                if (read_audio_byte_absolute(board, source, &low_timer) != VF2_OK) {
                    return VF2_ERROR_OUT_OF_BOUNDS;
                }
                ++source;
                timer = (uint16_t)(((uint16_t)(next_timer & 0x7fu) << 7u) |
                                   low_timer);
            } else {
                timer = next_timer;
            }
            write_be16(descriptor + 2u, timer);
        }
    }
    write_be32(descriptor + 4u, source);
    report->bytes_consumed += (size_t)(source - source_start);
    return VF2_OK;
}

vf2_status vf2_sound_board_maintain_streams(
    vf2_sound_board *board,
    vf2_sound_stream_maintenance_report *report
)
{
    vf2_sound_stream_maintenance_report local = {0};
    size_t descriptor_index = 0u;
    vf2_status status = VF2_OK;

    if (board == NULL || report == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    local.write_cursor_before = read_be16(
        board->sound_ram + VF2_SOUND_COMMAND_WRITE_CURSOR
    );
    for (descriptor_index = 0u;
         descriptor_index < VF2_SOUND_STREAM_MAINTENANCE_COUNT;
         ++descriptor_index) {
        uint8_t *descriptor = board->sound_ram + VF2_SOUND_DESCRIPTOR_BASE +
            descriptor_index * VF2_SOUND_STREAM_DESCRIPTOR_BYTES;
        ++local.descriptors_scanned;
        status = sound_board_process_stream_descriptor(
            board, descriptor, &local
        );
        if (status != VF2_OK) {
            *report = local;
            return status;
        }
    }
    local.write_cursor_after = read_be16(
        board->sound_ram + VF2_SOUND_COMMAND_WRITE_CURSOR
    );
    *report = local;
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

    for (index = 0u; index < VF2_SOUND_VOICE_COUNT; ++index) {
        uint8_t *voice = voices + index * VF2_SOUND_VOICE_BYTES;
        if ((voice[2u] & 0x40u) == 0u ||
            read_be16(voice + 4u) != (uint16_t)channel) {
            continue;
        }
        if (sound_board_key_off(board, voice) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        voice[2u] = 0u;
        voice[4u] = 0u;
        --board->sound_ram[VF2_SOUND_SHARED_COUNTER_151E];
        ++report->voices_cleared;
    }
    return VF2_OK;
}

static int sound_board_voice_helper_fast_path(
    vf2_sound_board *board,
    uint8_t *voice,
    uint8_t channel
)
{
    /* 0x11d0 returns immediately when the channel table's bit 7 is set. */
    if ((board->sound_ram[VF2_SOUND_CHANNEL_CONTROL_BASE + channel] &
         0x80u) == 0u) {
        return 0;
    }
    voice[2u] |= 0x02u;
    return 1;
}

static vf2_status sound_board_voice_helper_slow_path(
    vf2_sound_board *board,
    uint8_t *voice
)
{
    uint8_t age = 0u;
    size_t index = 0u;

    if (board == NULL || voice == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    /* 0x11e8 keeps a voice in the pending state while its per-voice
       countdown is non-zero.  The ROM's 0x4000 command is the SCSP key-off
       transition represented by the portable board boundary. */
    if (voice[0x0bu] != 0u) {
        if (sound_board_key_off(board, voice) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        voice[2u] |= 1u;
        return VF2_OK;
    }

    if (sound_board_key_off(board, voice) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    if ((voice[2u] & 8u) == 0u) {
        age = voice[9u];
        voice[9u] = 0u;
        --board->sound_ram[VF2_SOUND_SHARED_COUNTER_151E];
        voice[2u] = 0u;
        for (index = 0u; index < VF2_SOUND_VOICE_COUNT; ++index) {
            uint8_t *candidate = board->sound_ram + VF2_SOUND_VOICE_TABLE_BASE +
                index * VF2_SOUND_VOICE_BYTES;
            if (candidate[9u] != 0xffu &&
                (uint16_t)age < read_be16(candidate + 9u)) {
                --candidate[9u];
            }
        }
    } else {
        age = voice[5u];
        voice[9u] = 0u;
        voice[5u] = 0u;
        --board->sound_ram[VF2_SOUND_SHARED_COUNTER_152E];
        voice[2u] = 0u;
        for (index = 0u; index < VF2_SOUND_VOICE_COUNT; ++index) {
            uint8_t *candidate = board->sound_ram + VF2_SOUND_VOICE_TABLE_BASE +
                index * VF2_SOUND_VOICE_BYTES;
            if (candidate[9u] == 0xffu &&
                (uint16_t)age < read_be16(candidate + 5u)) {
                --candidate[5u];
            }
        }
    }
    return VF2_OK;
}

static vf2_status sound_board_update_voice_packed_register(
    vf2_sound_board *board,
    const uint8_t *voice,
    uint8_t entry,
    uint8_t payload
)
{
    const uint16_t slot = (uint16_t)(read_be16(voice) &
                                     (VF2_SCSP_SLOT_COUNT - 1u));
    const uint32_t address = (uint32_t)slot * VF2_SCSP_SLOT_BYTES + 0x13u;
    uint8_t register_value = 0u;
    vf2_status status = VF2_OK;

    if (entry != 1u && entry != 2u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_scsp_read_u8(&board->scsp, address, &register_value);
    if (status != VF2_OK) {
        return status;
    }
    if (entry == 1u) {
        register_value = (uint8_t)((register_value & 0x1fu) |
                                   ((uint8_t)(payload << 1u) & 0xe0u));
    } else {
        register_value = (uint8_t)((register_value & 0xf8u) |
                                   ((payload >> 4u) & 0x07u));
    }
    return vf2_scsp_write_u8(&board->scsp, address, register_value);
}

static vf2_status sound_board_dispatch_80(
    vf2_sound_board *board,
    const uint8_t *descriptor,
    uint8_t command,
    uint8_t payload,
    vf2_sound_dispatch_report *report
)
{
    const uint8_t selector = descriptor[2u];
    const uint8_t channel = (uint8_t)(command & 0x0fu);
    /* ROM 0x10d0: base and the high-path row step key on payload. */
    const uint16_t base_current = (uint16_t)((uint16_t)(payload & 0x7fu) +
                                             read_be16(descriptor + 4u));
    uint16_t current = base_current;
    uint16_t lookup = 0u;
    uint8_t lookup_byte = 0u;
    uint8_t table_index = 0u;
    uint32_t table_pointer = 0u;
    size_t record = 0u;
    size_t voice_index = 0u;

    board->sound_ram[VF2_SOUND_LOOKUP_CURRENT] = (uint8_t)current;
    if (selector >= 0xf0u) {
        const uint8_t table_number = (uint8_t)((selector - 0xf0u) & 0x7fu);
        if (read_audio_be32(
                board, UINT32_C(0x6032c6) + (uint32_t)table_number * 4u,
                &table_pointer
            ) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        table_pointer += (uint32_t)(payload & 0x7fu) * 6u;
        if (read_audio_be16(board, table_pointer, &lookup) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        table_index = (uint8_t)(current & 0x7fu);
    } else {
        if (read_audio_be32(
                board,
                UINT32_C(0x6053aa) + (uint32_t)(selector & 0x7fu) * 4u,
                &table_pointer
            ) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        for (record = 0u; record < 128u; ++record) {
            uint16_t limit = 0u;
            if (read_audio_be16(board, table_pointer, &limit) != VF2_OK) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
            if (current <= limit) {
                break;
            }
            table_pointer += 6u;
        }
        if (record == 128u ||
            read_audio_be16(board, table_pointer + 4u, &lookup) != VF2_OK ||
            read_audio_be16(board, table_pointer + 3u, &current) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        current = (uint16_t)(base_current + current);
        table_index = (uint8_t)(current & 0x7fu);
    }
    if (read_audio_byte_absolute(
            board, UINT32_C(0x60c3e6) + table_index, &lookup_byte
        ) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    write_be16(board->sound_ram + VF2_SOUND_LOOKUP_WORD, lookup);
    board->sound_ram[VF2_SOUND_LOOKUP_BYTE] = lookup_byte;
    report->channel = channel;
    report->selected_value = lookup_byte;
    report->lookup_word = lookup;
    report->signed_value = (int16_t)lookup;
    report->handler_address = UINT32_C(0x000010d0);
    for (voice_index = 0u; voice_index < VF2_SOUND_VOICE_COUNT;
         ++voice_index) {
        uint8_t *voice = board->sound_ram + VF2_SOUND_VOICE_TABLE_BASE +
            voice_index * VF2_SOUND_VOICE_BYTES;
        const uint32_t voice_value = ((uint32_t)voice[6u] << 24u) |
            ((uint32_t)voice[7u] << 16u) |
            ((uint32_t)voice[8u] << 8u) | voice[9u];
        if (voice[3u] == lookup_byte &&
            read_be16(voice + 4u) == (uint16_t)channel &&
            voice_value == lookup && (voice[2u] & 0x12u) == 0u &&
            (voice[2u] & 0x7fu) != 0u) {
            vf2_status status = VF2_OK;
            voice[2u] &= 0x7fu;
            if (sound_board_voice_helper_fast_path(board, voice, channel)) {
                ++report->voices_updated;
            } else {
                status = sound_board_voice_helper_slow_path(board, voice);
                if (status != VF2_OK) {
                    return status;
                }
                ++report->voices_updated;
            }
        }
    }
    return VF2_OK;
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
    uint8_t payload,
    vf2_sound_dispatch_report *report
)
{
    uint16_t maximum = 0u;
    /* ROM 0x1e76: the row is keyed by command, and the maximum and the
       row pointer by payload (ring byte 1). */
    uint32_t table_address = UINT32_C(0x608e12) +
        (uint32_t)(command & 0x7fu) * 4u;
    uint32_t pointer_table = 0u;
    uint32_t data_pointer = 0u;
    uint32_t source_pointer = 0u;
    uint8_t stream_flags = 0u;
    uint8_t *descriptor = NULL;
    size_t descriptor_index = 0u;

    report->handler_address = UINT32_C(0x00001e76);
    if (read_audio_be32(board, table_address, &pointer_table) != VF2_OK ||
        read_audio_be16(board, pointer_table, &maximum) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    if ((uint16_t)payload > maximum) {
        /* The 0x1eae arm initializes from a stale pointer held in a2
           (never loaded on this path), so it has no deterministic
           function of the command: fail closed. */
        return VF2_ERROR_UNSUPPORTED;
    }
    table_address = pointer_table + 2u + (uint32_t)payload * 4u;
    if (read_audio_be32(board, table_address, &data_pointer) != VF2_OK ||
        read_audio_byte_absolute(board, data_pointer, &stream_flags) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    if ((stream_flags & 0x80u) != 0u) {
        for (descriptor_index = 0u;
             descriptor_index < VF2_SOUND_STREAM_DESCRIPTOR_COUNT;
             ++descriptor_index) {
            descriptor = board->sound_ram + VF2_SOUND_STREAM_ALTERNATE_BASE +
                descriptor_index * VF2_SOUND_STREAM_DESCRIPTOR_BYTES;
            if ((descriptor[0] & 0x80u) != 0u) {
                continue;
            }
            memset(descriptor, 0, VF2_SOUND_STREAM_DESCRIPTOR_BYTES);
            write_be32(descriptor + 4u, data_pointer + 1u);
            write_be16(descriptor + 2u, 1u);
            descriptor[0] = 0x80u;
            ++report->descriptors_updated;
            return VF2_OK;
        }
        return VF2_OK;
    }
    descriptor = board->sound_ram + VF2_SOUND_DESCRIPTOR_BASE;
    source_pointer = data_pointer;
    /* ROM 0x1eb8: descriptor[4] is the longword AT the data pointer
       (a2 advances past it) and descriptor[8] the pointer past it. */
    if (read_audio_be32(board, source_pointer, &data_pointer) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    write_be32(descriptor + 4u, data_pointer);
    write_be32(descriptor + 8u, source_pointer + 4u);
    write_be16(descriptor + 2u, 1u);
    descriptor[0] = 0x88u;
    ++report->descriptors_updated;
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
    uint16_t combined = (uint16_t)((command & 0x7fu) |
                                   ((uint32_t)(payload & 0x7fu) << 7u));
    uint16_t transformed = (uint16_t)(combined >> 5u);
    uint8_t table_index = (uint8_t)transformed;
    uint8_t table_number = (uint8_t)(descriptor[7u] & 0x0fu);
    uint32_t table_pointer = 0u;
    uint8_t selected = 0u;
    uint16_t result = 0u;
    uint8_t channel = (uint8_t)(command & 0x0fu);
    size_t index = 0u;

    /* The zero case is the ROM's neg.b/subq.b normalization before lookup. */
    if ((transformed & 1u) == 0u && table_index == 0u) {
        table_index = UINT8_MAX;
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
    result = (transformed & 1u) != 0u
        ? selected
        : (uint16_t)(0u - selected);
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
    report->handler_address = UINT32_C(0x000012b4);

    if (channel >= 9u) {
        return VF2_OK;
    }
    for (index = 0u; index < VF2_SOUND_VOICE_COUNT; ++index) {
        const uint8_t *voice = board->sound_ram + VF2_SOUND_VOICE_TABLE_BASE +
            index * VF2_SOUND_VOICE_BYTES;
        if (read_be16(voice + 4u) == channel &&
            (voice[2u] & 0x7fu) != 0u) {
            ++report->voices_matched;
        }
    }
    /* The table-driven SCSP voice programming tail is kept explicit until
       its wider register window is modeled; the common no-voice path above
       is complete and deterministic. */
    return report->voices_matched == 0u ? VF2_OK : VF2_ERROR_UNSUPPORTED;
}

static vf2_status sound_board_dispatch_b0(
    vf2_sound_board *board,
    uint8_t *descriptor,
    uint8_t command,
    uint8_t payload,
    uint8_t ring_byte2,
    vf2_sound_dispatch_report *report
)
{
    const uint8_t channel = (uint8_t)(command & 0x0fu);
    const uint8_t entry = (uint8_t)(payload & 0x7fu);
    /* ROM B0 head: d2 = payload, d0 = ring byte 2 (peeked).  Value,
       scale, table-index and pack math all consume d0. */
    const uint8_t value = (uint8_t)((ring_byte2 >> 4u) & 0x07u);
    const uint32_t table = entry == 1u
        ? VF2_SOUND_CHANNEL_CONTROL_BASE
        : VF2_SOUND_CHANNEL_CONTROL_ALT_BASE;
    uint8_t *voices = board->sound_ram + VF2_SOUND_VOICE_TABLE_BASE;
    size_t index = 0u;

    if (entry == 0x40u) {
        board->sound_ram[VF2_SOUND_CHANNEL_CONTROL_BASE + channel] |= 0x80u;
        report->channel = channel;
        report->selected_value =
            board->sound_ram[VF2_SOUND_CHANNEL_CONTROL_BASE + channel];
        report->handler_address = UINT32_C(0x00001b4c);
        return VF2_OK;
    }
    if (entry == 0x7fu) {
        size_t voice_index = 0u;
        for (voice_index = 0u; voice_index < VF2_SOUND_VOICE_COUNT;
             ++voice_index) {
            uint8_t *voice = voices + voice_index * VF2_SOUND_VOICE_BYTES;
            const uint8_t previous_status = voice[2u];
            const uint8_t voice_channel = voice[4u];
            if ((previous_status & 0x40u) == 0u) {
                continue;
            }
            if (sound_board_key_off(board, voice) != VF2_OK) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
            voice[2u] = 0u;
            voice[4u] = 0u;
            if (voice_channel != 0x0fu &&
                (previous_status & 0x10u) == 0u) {
                --board->sound_ram[VF2_SOUND_SHARED_COUNTER_151E];
            }
            ++report->voices_cleared;
        }
        report->channel = channel;
        report->selected_value = entry;
        report->handler_address = UINT32_C(0x00001c0a);
        return VF2_OK;
    }
    if (entry == 0x7du || entry == 0x7eu) {
        const uint8_t first_channel = entry == 0x7du ? 9u : 0u;
        const uint8_t last_channel = entry == 0x7du ? 14u : 8u;
        size_t voice_index = 0u;
        uint8_t channel_index = 0u;

        for (voice_index = 0u; voice_index < VF2_SOUND_VOICE_COUNT;
             ++voice_index) {
            uint8_t *voice = voices + voice_index * VF2_SOUND_VOICE_BYTES;
            const uint8_t voice_channel = voice[4u];
            if ((voice[2u] & 0x40u) == 0u ||
                (entry == 0x7du
                    ? (voice_channel < 9u || voice_channel == 0x0fu)
                    : voice_channel != 0x0fu)) {
                continue;
            }
            if (sound_board_key_off(board, voice) != VF2_OK) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
            voice[2u] = 0u;
            voice[4u] = 0u;
            ++report->voices_cleared;
        }
        for (channel_index = first_channel;
             channel_index <= last_channel; ++channel_index) {
            board->sound_ram[VF2_SOUND_CHANNEL_CONTROL_BASE + channel_index] = 0u;
            board->sound_ram[VF2_SOUND_CHANNEL_CONTROL_ALT_BASE + channel_index] = 0u;
            board->sound_ram[VF2_SOUND_CHANNEL_TABLE_BASE +
                             (size_t)channel_index * 2u] = 0u;
            board->sound_ram[VF2_SOUND_CHANNEL_TABLE_BASE +
                             (size_t)channel_index * 2u + 1u] = 0u;
        }
        if (entry == 0x7du) {
            board->sound_ram[VF2_SOUND_SHARED_COUNTER_152E] = 0u;
        } else {
            board->sound_ram[VF2_SOUND_SHARED_COUNTER_151E] = 0u;
        }
        report->channel = channel;
        report->selected_value = entry;
        report->handler_address = UINT32_C(0x00001c0a);
        return VF2_OK;
    }
    if (entry != 1u && entry != 2u && entry != 7u && entry != 10u &&
        entry != 16u &&
        entry != 0x29u && entry != 0x2au && entry != 0x2bu) {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (entry == 0x29u || entry == 0x2au || entry == 0x2bu) {
        uint8_t packed = (uint8_t)(((ring_byte2 & 0x0fu) << 3u) |
                                    ((ring_byte2 & 0x70u) >> 4u));
        size_t descriptor_index = 0u;
        for (descriptor_index = 0u;
             descriptor_index < VF2_SOUND_COMMAND_DESCRIPTOR_COUNT;
             ++descriptor_index) {
            uint8_t *candidate = board->sound_ram +
                VF2_SOUND_COMMAND_DESCRIPTOR_BASE +
                descriptor_index * VF2_SOUND_COMMAND_DESCRIPTOR_BYTES;
            if (read_be16(candidate + 1u) != (uint16_t)channel) {
                continue;
            }
            if (entry == 0x29u) {
                candidate[13u] = packed;
            } else if (entry == 0x2au) {
                candidate[12u] = (uint8_t)((candidate[12u] & 0x78u) |
                                           ((ring_byte2 >> 4u) & 0x07u));
            } else {
                candidate[12u] = (uint8_t)((candidate[12u] & 0x07u) |
                                           (packed & 0x78u));
            }
            ++report->descriptors_updated;
        }
        report->handler_address = entry == 0x29u
            ? UINT32_C(0x00001912)
            : entry == 0x2au
                ? UINT32_C(0x0000194a)
                : UINT32_C(0x00001986);
        report->channel = channel;
        report->selected_value = packed;
        return VF2_OK;
    }
    if (entry == 7u) {
        uint16_t scaled = (uint16_t)((uint16_t)ring_byte2 << 1u);
        if ((scaled & 0x00ffu) != 0u) {
            ++scaled;
            descriptor[3u] = (uint8_t)scaled;
        }
        if ((descriptor[0] & 1u) == 0u) {
            scaled = (uint16_t)(((uint32_t)(scaled & 0xffu) *
                                 board->sound_ram[VF2_SOUND_SHARED_FACTOR_151F])
                                >> 8u);
        } else {
            scaled = (uint16_t)(((uint32_t)(scaled & 0xffu) * 0xffu) >> 8u);
        }
        if ((scaled & 0xffu) != 0u) {
            descriptor[10u] = (uint8_t)(scaled + 1u);
        }
        report->selected_value = descriptor[10u];
        report->handler_address = UINT32_C(0x000016ba);
    } else if (entry == 10u) {
        descriptor[6u] = sound_board_b0_entry_10_table[ring_byte2 & 0x7fu];
        report->selected_value = descriptor[6u];
        report->handler_address = UINT32_C(0x0000174c);
    } else if (entry == 16u) {
        uint16_t scaled = (uint16_t)((uint16_t)payload << 1u);
        uint8_t factor = UINT8_C(0xff);
        if (scaled != 0u) {
            ++scaled;
            descriptor[3u] = (uint8_t)scaled;
        }
        if ((descriptor[0] & 1u) == 0u) {
            factor = board->sound_ram[VF2_SOUND_SHARED_FACTOR_151F];
        }
        scaled = (uint16_t)(((uint32_t)(scaled & 0xffu) * factor) >> 8u);
        if ((scaled & 0xffu) != 0u) {
            descriptor[10u] = (uint8_t)(scaled + 1u);
        }
        report->selected_value = descriptor[10u];
        report->handler_address = UINT32_C(0x0000181c);
    } else {
        board->sound_ram[table + channel] =
            (uint8_t)((board->sound_ram[table + channel] & 0x80u) | value);
        report->selected_value = board->sound_ram[table + channel];
        report->handler_address = entry == 1u
            ? UINT32_C(0x000015de)
            : UINT32_C(0x0000164c);
    }
    report->channel = channel;
    for (index = 0u; index < VF2_SOUND_VOICE_COUNT; ++index) {
        uint8_t *voice = voices + index * VF2_SOUND_VOICE_BYTES;
        const int voice_match = entry == 7u
            ? ((voice[2u] & 0x40u) != 0u &&
               read_be16(voice + 4u) == (uint16_t)channel)
            : ((voice[2u] & 0x7fu) != 0u &&
               read_be16(voice + 4u) == (uint16_t)channel);
        if (voice_match) {
            if (entry == 1u || entry == 2u) {
                vf2_status status = sound_board_update_voice_packed_register(
                    board, voice, entry, payload
                );
                if (status != VF2_OK) {
                    return status;
                }
                ++report->voices_updated;
            } else {
                ++report->voices_matched;
            }
        }
    }
    /* Entries 1/2 now include the ROM's packed per-voice SCSP write. */
    return report->voices_matched == 0u ? VF2_OK : VF2_ERROR_UNSUPPORTED;
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
                board, local.command, local.payload, &local
            );
            if (status != VF2_OK) {
                *report = local;
                return status;
            }
        } else if (local.command_class == 0xb0u) {
            vf2_status status = sound_board_dispatch_b0(
                board, descriptor, local.command, local.payload,
                ram[VF2_SOUND_COMMAND_RING_BASE +
                    ((cursor + 2u) & UINT16_C(0x0fffu))], &local
            );
            if (status != VF2_OK) {
                *report = local;
                return status;
            }
        } else if (local.command_class == 0xc0u) {
            descriptor[2] = local.payload;
            if (descriptor[2] >= 0x70u) {
                descriptor[2] = (uint8_t)(descriptor[2] | 0x80u);
            }
            local.handler_address = UINT32_C(0x00001e5a);
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
