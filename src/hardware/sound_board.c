#include "vf2/sound_board.h"

#include <string.h>

/* ROM table addressed by the 0x174c B0 handler's PC-relative lookup at
   audio-CPU address 0x179e. */

static uint16_t read_be16(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8u) | data[1]);
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

/* ROM 0x1ac0 (B0 entry 0x3a) / 0x422 (boot) sample uploader,
   re-recovered v0822. Both entries converge on one body; only the
   prologues differ (B0: SR set + payload 0x7f arm; boot: d0 = 0).
   Silicon shape (byte-verified):
   - first-copy: d7 = word@0x605b26, `dbf` copies d7+1 bytes
     0x605b28+i -> $5000+i (the C's old `d7 bytes` was short one);
   - B0 0x7f arm: `beq` targets the 0x1b46 epilogue (SR restore +
     rts), a clean exit -- the old "mid-dbf trap" was +4 damage;
   - row pick: d1 = max word@row_base, `cmp.w d1,d0; bgt` exits
     clean (B0 never modeled this gate); else row = long at
     row_base+2+d0*4 (the C's direct be32(row_base) read the max
     word AS a pointer -- wrong mechanism and address);
   - records: d7 = row word, `dbf` runs count+1 records;
   - entries at the CONSTANT base 0x605b28 + selector*0x14 (the C's
     `table = be32(0x605b28)` mistook entry 0's source field for
     the base -- they coincide numerically in real ROM only);
   - stored slot word is the FULL `stored - source` long (the C's
     low-byte mask was invented);
   - inner copy runs (length+0x1ff).w+1 bytes; $6FFFF guard halts.
   Row/selector words address through the bus: a RAM-mapped row
   pointer (real boot row 0 IS rom, but the reader must not assume)
   reads RAM; unmapped addresses fail closed like the stepper. */
static vf2_status read_bus_be16(
    vf2_sound_board *board,
    uint32_t address,
    uint16_t *value
)
{
    if (board == NULL || value == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (address + 1u < VF2_SOUND_RAM_BYTES) {
        *value = read_be16(board->sound_ram + address);
        return VF2_OK;
    }
    return read_audio_be16(board, address, value);
}

static vf2_status sound_board_sample_upload(
    vf2_sound_board *board,
    uint32_t row_table_base,
    int check_7f,
    uint8_t payload
)
{
    uint16_t first_count = 0u;
    uint16_t record_total = 0u;
    uint16_t record_index = 0u;
    uint16_t max_index = 0u;
    uint16_t d0 = 0u;
    uint32_t row = 0u;
    /* a2 is the fixed $5000 sample-slot table (absolute short). */
    uint32_t slot_base = UINT32_C(0x5000);
    uint32_t target = 0u;
    size_t index = 0u;

    if (read_audio_be16(board, UINT32_C(0x605b26), &first_count) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    /* ROM dbf: d7+1 iterations (0x1ad0-0x1ad2 / 0x42e-0x430). */
    for (index = 0u; index <= (size_t)first_count; ++index) {
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
        /* ROM 0x1ada-0x1ade: beq $1b46, the SR-restore/rts epilogue.
           Staged bytes persist; clean OK. */
        return VF2_OK;
    }
    /* ROM 0x1ae2-0x1af5 / 0x438-0x44b: d1 = max word, bgt exits. */
    if (read_audio_be16(board, row_table_base, &max_index) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    d0 = check_7f ? (uint16_t)(payload & 0x7fu) : 0u;
    if (d0 > max_index) {
        return VF2_OK;
    }
    if (read_audio_be32(board,
                        row_table_base + 2u + (uint32_t)d0 * 4u,
                        &row) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    if (read_bus_be16(board, row, &record_total) != VF2_OK) {
        return VF2_ERROR_UNSUPPORTED;
    }
    row += 2u;
    target = UINT32_C(0x50000);
    /* ROM dbf: count+1 records (0x1b42 / 0x498). */
    for (record_index = 0u; ; ++record_index) {
        uint16_t selector = 0u;
        uint32_t entry = 0u;
        uint32_t source = 0u;
        uint32_t length = 0u;
        uint32_t stored = 0u;
        uint16_t count = 0u;
        if (read_bus_be16(board, row, &selector) != VF2_OK) {
            return VF2_ERROR_UNSUPPORTED;
        }
        row += 2u;
        entry = (uint32_t)selector * UINT32_C(0x14);
        /* Entry structs live at the constant base 0x605b28 (a0);
           the +0/+4/+8 longs are source/length/stored. */
        if (read_audio_be32(board, UINT32_C(0x605b28) + entry,
                            &source) != VF2_OK ||
            read_audio_be32(board, UINT32_C(0x605b28) + entry + 8u,
                            &stored) != VF2_OK ||
            read_audio_be32(board, UINT32_C(0x605b28) + entry + 4u,
                            &length) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        length += UINT32_C(0x1ff);
        /* ROM sub.l d2,d3: full-long difference (no masking). */
        stored = stored - source;
        if (slot_base + 8u + entry + 4u > VF2_SOUND_RAM_BYTES ||
            slot_base + entry + 4u > VF2_SOUND_RAM_BYTES) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        /* ROM 0x1b24: move.l d3,$08(a2,d1.w) -- entry-indexed, NOT
           a fixed slot (v0822: the old fixed-$6008 form collapsed
           every record's stored word onto one address). */
        write_be32(board->sound_ram + slot_base + 8u + entry, stored);
        write_be32(board->sound_ram + slot_base + entry, target);
        board->sound_ram[slot_base + entry] =
            (uint8_t)(board->sound_ram[slot_base + entry] | 0x80u);
        count = (uint16_t)(length & UINT32_C(0xffff));
        for (index = 0u; ; ++index) {
            uint8_t sample = 0u;
            vf2_status sample_status = VF2_OK;
            if (target >= VF2_SOUND_RAM_BYTES) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
            sample_status = vf2_sound_board_read_u8(
                board, source + (uint32_t)index, &sample);
            if (sample_status != VF2_OK) {
                /* Silicon bus error on unmapped sample reads; the
                   stepper refuses the same way (status-equivalence,
                   as the old P0c up-first contract). */
                return sample_status;
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
        if (record_index >= record_total) {
            break;
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
    /* ROM 0x422 boot staging: d0 = 0, same max-gate + indexed pick
       (0x438-0x44b). The old high-word gate matched the skip outcome
       on its fixture by coincidence; the unified gate covers it. */
    return sound_board_sample_upload(board, UINT32_C(0x608df0), 0, 0u);
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
    /* Sample window follows the ATTACHED size (v0822): the stepper
       maps SAMPLE_BASE + offset < sample_rom_size, and real upload
       sources reach ~0x23e226 past the old 2MB DIRECT cap. Capping
       at DIRECT_BYTES made the C refuse what silicon reads. */
    if (address >= VF2_SOUND_SAMPLE_ROM_BASE &&
        address - VF2_SOUND_SAMPLE_ROM_BASE < board->sample_rom_size) {
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
    /* ROM L1 voice ISR 0x5fe (v0821, +2-base reaudit): a full 32-voice
       scan, stride 0x10, every voice visited exactly once. Per voice:
       skip unless bit0 of +2; decrement +0xb; on nonzero skip; bit3
       of +2 picks the release arm (0x684) or the normal arm (0x636).
       Both arms key off (d1 = voice word 0; SCSP[slot+0x0b] = 0x1f,
       ram[0x1000+slot] = 0), clear +2, snapshot d2 (normal: +9,
       release: +5), clear +9 (release also +5), decrement $151e /
       $152e, then rebalance all 32 voices (normal: +9 bytes != 0xff
       with mem > d2 decrement; release: +5 bytes of voices whose +9
       == 0xff with mem > d2 decrement). No traps: the ISR always
       reaches rte with a balanced stack (stepper 140 steps, a7
       balanced on every L1 leg).
       Byte domain (v0821 correction to the reaudit note): the
       rebalance compares are cmp.B (opcodes 0xB412/0xB42A, opmode
       000 = byte-verified), and d2 is loaded by move.b, so the ISR
       entry residue in d2's upper bytes never participates. The
       MAME d2-variance data stands; its "C must take d2 as input"
       conclusion is void. The C is deterministic over RAM. */
    for (index = 0u; index < VF2_SOUND_VOICE_COUNT; ++index) {
        uint8_t *voice = voices + index * VF2_SOUND_VOICE_BYTES;
        int release_path = 0;
        uint8_t age = 0u;
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
        if (sound_board_key_off(board, voice) != VF2_OK) {
            local.counter_151e_after =
                board->sound_ram[VF2_SOUND_SHARED_COUNTER_151E];
            local.counter_152e_after =
                board->sound_ram[VF2_SOUND_SHARED_COUNTER_152E];
            *report = local;
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        voice[2u] = 0u;
        ++local.voices_expired;
        if (!release_path) {
            /* Normal arm 0x636-0x680. */
            age = voice[9u];
            voice[9u] = 0u;
            --board->sound_ram[VF2_SOUND_SHARED_COUNTER_151E];
            ++local.normal_expirations;
            for (other = 0u; other < VF2_SOUND_VOICE_COUNT; ++other) {
                uint8_t *target =
                    voices + other * VF2_SOUND_VOICE_BYTES + 9u;
                /* ROM 0x666-0x674: skip 0xff slots; bcc skips when
                   d2 >= mem (unsigned); else decrement. The expired
                   voice itself (now 0) always skips via bcc. */
                if (*target == UINT8_MAX || age >= *target) {
                    continue;
                }
                --(*target);
                ++local.voices_aged;
            }
        } else {
            /* Release arm 0x684-0x6d2. */
            age = voice[5u];
            voice[9u] = 0u;
            voice[5u] = 0u;
            --board->sound_ram[VF2_SOUND_SHARED_COUNTER_152E];
            ++local.release_expirations;
            for (other = 0u; other < VF2_SOUND_VOICE_COUNT; ++other) {
                uint8_t *slot9 =
                    voices + other * VF2_SOUND_VOICE_BYTES + 9u;
                uint8_t *slot5 =
                    voices + other * VF2_SOUND_VOICE_BYTES + 5u;
                /* ROM 0x6b6-0x6c8: only 0xff-aged slots; then the
                   same unsigned d2 >= mem skip; else decrement +5. */
                if (*slot9 != UINT8_MAX || age >= *slot5) {
                    continue;
                }
                --(*slot5);
                ++local.voices_aged;
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
            /* ROM 0x1e1c is cmp.B (like the 0x1332 scan match): the
               channel test is the single voice[4] byte. */
            voice[4u] != channel) {
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
    /* ROM 0x10d0 (v0825 S2b primer correction): the switch's beq
       lands ON the primer (`move.b (a3)+,d0` with a3 at ring+1 as
       the drain leaves it), so d2 = PAYLOAD, not command. Base =
       (payload&0x7f) + desc[4]-byte: ROM 0x10da is `add.b`
       (disassembled, same byte-add shape as the 90 arm's 0x860),
       so desc[5] never participates -- the old word-add was a
       print-era assumption. The 0x10d4 andi.l #$7f is LONG, so
       d0's upper is zeroed entry-independently and the low byte
       escapes to $150f clean. Payload/byte2 are never read past
       the head. */
    const uint16_t base =
        (uint16_t)(((uint16_t)(payload & 0x7fu) + descriptor[4u]) &
                   0xffu);

    report->handler_address = UINT32_C(0x000010d0);
    report->channel = channel;
    board->sound_ram[VF2_SOUND_LOOKUP_CURRENT] = (uint8_t)base;
    /* Split at 0x10e2 (`move.b desc[2],d3` / cmpi #$f0 / bcs):
       the low arm is a stale branch into mid-lea and the high arm
       needs desc[2] >= 0xf0, which byte-match (S2a: desc[1] ==
       channel <= 15) leaves free to differ -- but NEITHER arm is
       recovered yet. Everything past (row/$1515/voice update) is
       post-refusal. Refuse with the head filed. */
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
    /* ROM 0x858 (v0825 S2b primer correction): the switch's beq
       lands ON the primer (`move.b (a3)+,d0` with a3 at ring+1 as
       the drain leaves it -- stepper-proven switch-90 leg enters at
       0x858 in 4 steps), so d2 = PAYLOAD (ring byte 1) and the
       second byte is ring byte 2 (the (a3) peek with a3 at ring+2).
       The old "d2 = command / second = payload" matched the C's own
       head-entry staging, not the drain's reality -- the same trap
       class as the S2a e0 primer fix, caught the same way (a
       drain-entry differential, not a re-read of the print).
       Channel (report only -- the body itself never reads the
       command byte) stays command & 0xf. */
    const uint8_t channel = (uint8_t)(command & 0x0fu);
    const uint8_t head = payload;
    const uint8_t second = ring_byte2;
    uint16_t base_full = 0u;
    uint8_t mode = 0u;

    report->handler_address = UINT32_C(0x00000858);
    report->channel = channel;
    /* ROM 0x85c: base = head + desc[4]-byte (`move.b`/`add.b`; only
       the low byte adds). d0's upper is zero on entry (the drain's
       0x7be/0x7c8 andi.l chain clears it and every move.b preserves
       the zero), so the byte add is exact and the low byte escapes
       to $150f. */
    base_full = (uint16_t)((uint16_t)head + descriptor[4u]);
    board->sound_ram[VF2_SOUND_LOOKUP_CURRENT] = (uint8_t)base_full;
    /* ROM 0x866: second == 0 continues into the 80 body
       mid-instruction (0x10e4): trap on HW. */
    if (second == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    /* ROM 0x86c mode table: `move.b (a1,d0.l)` with d0.l == second
       exactly (upper zero per the chain above -- no masking exists
       on this path, none is needed). */
    if (read_audio_byte_absolute(
            board,
            UINT32_C(0x60c6a6) + second,
            &mode
        ) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    board->sound_ram[VF2_SOUND_LOOKUP_MODE] = mode;
    report->selected_value = mode;
    /* Split at 0x878 (`move.b desc[2],d3` / cmpi.b #$f0 / bcs):
       - < 0xf0 -> 0xa3e low path (row-table walk, files
         $1516/$1518/$1515/$1511, bra $f12 -- never Loop-B);
       - >= 0xf0 -> HIGH table path (0x884+: stride payload&0x7f*6,
         0x8a6 gate, 0x8b2 file $1511/$1512/$1516, 0x8d2/0x8e4 file
         $150d, $1515, fall-through 0x8fe into Loop-B 0x902).
       The d3 upper survives subi.b/andi.w as long-residue, but the
       table pick reads d3.w only (0x894 `move.l (a1,d3.w)`), so the
       index is exact. NEITHER arm is recovered yet (S2c owns the
       HIGH path + Loop-B door; S3 the setup arms). Refuse with head
       filed and the packet held. */
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
   words (`move.w` uses the low word for value and index alike). d4 =
   voice[0] is the slot word; both stores are d4-indexed (the old
   absolute forms, and a later d0-scatter misread of the same ext
   word, came from indexed-dst print bugs): SCSP[d4+0x10] = d0 and
   ram[0x201a+d4] = picked word (stream-slot timer). An odd slot word
   fails closed (address-error shape on ROM). The lsl.b #5 offset
   wraps at the byte. */
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

    /* d3 = voice[10] + desc[5]-byte, sign-extended (TRUE add.b at
       ROM 0x135e; only the low byte adds, ext.w sign-extends it).
       d5 = $1524-word plus d3, sign-extended (ext.l drops the
       caller-upper). */
    addend = (int16_t)(int8_t)(uint8_t)(voice[0x0au] + descriptor[5u]);
    slide = (int32_t)(int16_t)(uint16_t)(shared +
                                         (uint16_t)(uint16_t)addend);
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
    {
        /* ROM 0x1396: move.w d0,$10(a5,d4.w). Slot+0x10 word = d0. */
        const uint32_t slot_base = (uint32_t)read_be16(voice);
        vf2_status voice_status = VF2_OK;
        if ((slot_base & 1u) != 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        voice_status =
            vf2_scsp_write_u16(&board->scsp, slot_base + UINT32_C(0x10),
                               shaped);
        if (voice_status != VF2_OK) {
            return voice_status;
        }
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
    {
        /* ROM 0x13c4: move.w d2,$1a(a2,d4.w) with a2 = 0x2000 and d4
           = voice[0] (slot word). Stream-slot timer, not ram[0x601a]. */
        const uint32_t timer_at = UINT32_C(0x201a) +
            (uint32_t)read_be16(voice);
        if (timer_at + 2u > (uint32_t)VF2_SOUND_RAM_BYTES) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        write_be16(board->sound_ram + timer_at, stored);
    }
    return VF2_OK;
}

static vf2_status sound_board_dispatch_e0(
    vf2_sound_board *board,
    uint8_t *descriptor,
    uint8_t command,
    uint8_t payload,
    uint8_t ring_byte2,
    vf2_sound_dispatch_report *report
)
{
    /* ROM e0 entry 0x12b4 (v0825 S2a primer correction): the primer
       is `move.b (a3)+,d0` / `move.b d0,d2` / `move.b (a3),d0` with
       a3 = ring+cursor+1 as the drain leaves it -- so d2 = PAYLOAD
       (ring byte 1) and d0 = ring byte 2, NOT command/payload. The
       S7b model (d2 = command via a stream staged {cmd,payload})
       matched its own staging, not the drain's reality; the
       drain-e0 differential caught it (shared 0xfff2 vs 0xfff1).
       Combined = (payload&0x7f)|((byte2&0x7f)<<7), >> 5. The
       0x12ba/0x12c0 andi.l #$7f are LONG (byte-verified): both
       uppers are zeroed entry-independently, so NO residue rides
       into the stored word. The stored word is ±selected, negated
       when transformed-bit8 is clear. Channel (voice match +
       channel-table word) stays command & 0xf. */
    uint16_t combined = (uint16_t)((payload & 0x7fu) |
                                   ((uint32_t)(ring_byte2 & 0x7fu) << 7u));
    uint16_t transformed = (uint16_t)(combined >> 5u);
    uint8_t table_index = (uint8_t)transformed;
    uint8_t table_number = (uint8_t)(descriptor[7u] & 0x0fu);
    uint32_t table_pointer = 0u;
    uint8_t selected = 0u;
    uint16_t result = 0u;
    uint8_t channel = (uint8_t)(command & 0x0fu);
    size_t index = 0u;

    /* ROM neg.b/subq.b normalization (0x12d2-0x12dc, btst #8): the
       index is the POST-negation low byte (0xFF when it normalizes
       to zero). Both negs key on transformed-bit8 (0x100), coupled:
       bit8 set skips both, bit8 clear runs both. */
    if ((transformed & 0x100u) == 0u) {
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
    result = (transformed & 0x100u) != 0u
        ? (uint16_t)selected
        : (uint16_t)(0u - (uint16_t)selected);
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

    /* ROM bcc on channel >= 9 exits via rts at 0x13c8 (stepper leg
       e0-gate @34 steps). Benign; the early return stands on that
       evidence. */
    if (channel >= 9u) {
        return VF2_OK;
    }
    for (index = 0u; index < VF2_SOUND_VOICE_COUNT; ++index) {
        uint8_t *voice = board->sound_ram + VF2_SOUND_VOICE_TABLE_BASE +
            index * VF2_SOUND_VOICE_BYTES;
        /* ROM 0x1332 is cmp.B voice+4 (byte-verified opcode; the old
           cmp.w print hid it): match on the single channel byte, not
           a word. voice[5] is ignored by the scan. */
        if (voice[4u] == channel &&
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
    /* ROM 0x1ac0: SR prologue, first-copy, andi.w #$7f 0x7f arm
       (beq to the 0x1b46 epilogue -- clean exit, verified target),
       max-gate + indexed row pick by payload, shared uploader. */
    return sound_board_sample_upload(
        board, UINT32_C(0x608df0),
        1, payload);
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
        /* ROM drain scan 0x7dc-0x7ea (v0825 S2a): `btst #7,$0000`
           gate, then `cmp.b $0001(a0),d0` with d0 = command & 0xf
           (0x7d2 `andi.b`) -- a BYTE match on descriptor[1]. The old
           word-match (be16(desc+1) == channel) was a print-era
           assumption with no silicon behind it; it made Loop-B
           unreachable (no boot descriptor both word-matches and
           carries desc[2] >= 0xf0) while byte-match routes img2's
           0x0a-0x0e rows into the 0x90 HIGH path. Proven by the
           drain-e0 differential (desc[1] = ch, desc[2] = 0). */
        if ((descriptor[0] & 0x80u) == 0u ||
            descriptor[1u] != (uint8_t)(local.command & 0x0fu)) {
            continue;
        }
        ++local.descriptors_matched;
        /* Switch mapping (v0825 S2b): the ROM switch reads filed
           $150e and jumps per class ($90->0x858, $80->0x10d0,
           $e0->0x12b4, $b0->0x13ca, $c0->0x1e5a, $a0->0x1e74,
           else rts at 0x856 -- the C's own switch-routing legs pin
           every one). The old default->90 mapping is retired. */
        if (local.command_class == 0x80u) {
            vf2_status status = sound_board_dispatch_80(
                board, descriptor, local.command, local.payload, &local
            );
            if (status != VF2_OK) {
                *report = local;
                return status;
            }
        } else if (local.command_class == 0x90u) {
            vf2_status status = sound_board_dispatch_90(
                board, descriptor, local.command, local.payload,
                ram[VF2_SOUND_COMMAND_RING_BASE +
                    ((cursor + 2u) & UINT16_C(0x0fffu))], &local
            );
            if (status != VF2_OK) {
                *report = local;
                return status;
            }
        } else if (local.command_class != 0x80u &&
                   local.command_class != 0xa0u &&
                   local.command_class != 0xb0u &&
                   local.command_class != 0xc0u &&
                   local.command_class != 0xe0u) {
            /* ROM switch rts at 0x856 (v0825 S2b): unlisted classes
               run no body -- the old default->90 mapping contradicted
               the C's own switch-00 leg (filed 0x00 reaches 0x856 in
               14 steps). The drain still consumes the packet and a
               later descriptor may still match a listed class, so
               record the ignore and continue the scan. */
            local.handler_address = UINT32_C(0x00000856);
            local.channel = (uint8_t)(local.command & 0x0fu);
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
            /* Primer-correct byte 2 (S2a): the drain leaves a3 at
               ring+cursor+1, so the head's d0 comes from ring byte 2. */
            vf2_status status = sound_board_dispatch_e0(
                board, descriptor, local.command, local.payload,
                ram[VF2_SOUND_COMMAND_RING_BASE +
                    ((cursor + 2u) & UINT16_C(0x0fffu))],
                &local
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
