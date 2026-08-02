#include "vf2/hybrid.h"

#include "vf2/analysis/orchestrator_gates.h"
#include "vf2/analysis/orchestrator_limits.h"
#include "vf2/analysis/orchestrator_scan.h"

#include <limits.h>
#include <string.h>

#define VF2_TEXTURE_BYTE_RUN_ENTRY UINT32_C(0x0004c868)
#define VF2_TEXTURE_BYTE_DECODE_ENTRY UINT32_C(0x0004c6e0)
#define VF2_TEXTURE_BYTE_RUN_EXIT UINT32_C(0x0004c878)
#define VF2_TEXTURE_WORD_RUN_ENTRY UINT32_C(0x0004cce8)
#define VF2_TEXTURE_WORD_DECODE_ENTRY UINT32_C(0x0004cc28)
#define VF2_TEXTURE_WORD_PREPARE_ENTRY UINT32_C(0x0004cb64)
#define VF2_TEXTURE_WORD_PREPARE_EXIT UINT32_C(0x0004cc28)
#define VF2_TEXTURE_WORD_PREPARE_TIMER_RETURN UINT32_C(0x0004cbb0)
#define VF2_TEXTURE_WORD_RUN_EXIT UINT32_C(0x0004ccf8)
#define VF2_TEXTURE_SYMBOL_TABLE_ENTRY UINT32_C(0x0004c3f0)
#define VF2_TEXTURE_PAIR_TABLE_ENTRY UINT32_C(0x0004c4d4)
#define VF2_TEXTURE_TREE_DISPATCH_ENTRY UINT32_C(0x0004c544)
#define VF2_TEXTURE_TREE_DISPATCH_EXIT UINT32_C(0x0004c6e0)
#define VF2_TEXTURE_TREE_RETURN UINT32_C(0x0004c5dc)
#define VF2_TEXTURE_TREE_ENTRY UINT32_C(0x0004c928)
#define VF2_TEXTURE_CONVERT_ENTRY UINT32_C(0x0004ce88)
#define VF2_TEXTURE_ADDRESS_TABLE_ENTRY UINT32_C(0x0004d16c)
#define VF2_DIAGNOSTIC_TEXT_COPY_ENTRY UINT32_C(0x00007fc0)
#define VF2_TILE_GLYPH_EXPAND_ENTRY UINT32_C(0x0004f944)
#define VF2_VIDEO_STATUS_LATCH_ENTRY UINT32_C(0x00002ec4)
#define VF2_GEOMETRY_FRAME_COMMIT_ENTRY UINT32_C(0x00002edc)
#define VF2_GEOMETRY_COMMAND_SETUP_ENTRY UINT32_C(0x00002f5c)
#define VF2_FRAME_SCRATCH_CLEAR_ENTRY UINT32_C(0x0000a154)
#define VF2_PALETTE_PAGE_UPLOAD_ENTRY UINT32_C(0x00002de4)
#define VF2_TEXTURE_COLOR_PREPARE_ENTRY UINT32_C(0x0004cd18)
#define VF2_TEXTURE_COLOR_PREPARE_EXIT UINT32_C(0x0004cdb0)
#define VF2_TEXTURE_COLOR_PREPARE_TIMER_RETURN UINT32_C(0x0004cd64)
#define VF2_TEXTURE_CONVERT_LOOP_ENTRY UINT32_C(0x0004cdb0)
#define VF2_TEXTURE_CONVERT_POST_ENTRY UINT32_C(0x0004cdd4)
#define VF2_TIMER_WAIT_UPDATE_ENTRY UINT32_C(0x00000b6c)
#define VF2_INLINE_TEXT_THUNK_ENTRY UINT32_C(0x00009444)
#define VF2_TEXTURE_STATUS_LINE_ENTRY UINT32_C(0x0004d2c0)
#define VF2_GAME_STATE_CLASSIFY_ENTRY UINT32_C(0x0000281c)
#define VF2_GAME_COLOR_LOOKUP_ENTRY UINT32_C(0x000026ec)
#define VF2_TEXTURE_ORCHESTRATOR_SAVE_ENTRY UINT32_C(0x0004bb18)
#define VF2_TEXTURE_ORCHESTRATOR_BODY_ENTRY UINT32_C(0x0004bcd4)
#define VF2_TEXTURE_ORCHESTRATOR_BODY_RETURN UINT32_C(0x0004bb94)
#define VF2_TEXTURE_FRAME_GATE_ENTRY UINT32_C(0x0004bcd4)
#define VF2_TEXTURE_FRAME_GATE_LATCH UINT32_C(0x0050006d)
#define VF2_TEXTURE_DEFAULT_LIMITS_ENTRY UINT32_C(0x0004bfe0)
#define VF2_TEXTURE_DEFAULT_LIMITS_RETURN UINT32_C(0x0004bd00)
#define VF2_TEXTURE_STATUS_DISPATCH_ENTRY UINT32_C(0x0004bd24)
#define VF2_TEXTURE_STATUS_DISPATCH_TARGET UINT32_C(0x0004d2c0)
#define VF2_TEXTURE_STATUS_DISPATCH_RETURN UINT32_C(0x0004bd5c)
#define VF2_TEXTURE_ACTIVE_PREPARE_ENTRY UINT32_C(0x0004bde0)
#define VF2_TEXTURE_ACTIVE_PREPARE_TARGET UINT32_C(0x0004d16c)
#define VF2_TEXTURE_ACTIVE_PREPARE_RETURN UINT32_C(0x0004be6c)
#define VF2_TEXTURE_ACTIVE_FLAGS UINT32_C(0x0055c2f4)
#define VF2_TEXTURE_COORD_TABLE UINT32_C(0x0004c120)
#define VF2_TEXTURE_HEADER_DECODE_ENTRY UINT32_C(0x0004c180)
#define VF2_TEXTURE_HEADER_DECODE_EXIT UINT32_C(0x0004c3f0)
#define VF2_TEXTURE_HEADER_STATE UINT32_C(0x00550080)
#define VF2_TEXTURE_HEADER_OUTPUT UINT32_C(0x0055c320)
#define VF2_TEXTURE_RECORD_START UINT32_C(0x00550168)
#define VF2_TEXTURE_RECORD_END UINT32_C(0x005502a8)
#define VF2_TEXTURE_RUNTIME_FLAGS UINT32_C(0x00508000)
#define VF2_TEXTURE_RECORD_ADVANCE_ENTRY UINT32_C(0x0004bf60)
#define VF2_TEXTURE_RECORD_ADVANCE_EXIT UINT32_C(0x0004bd24)
#define VF2_TEXTURE_FINAL_STATUS_ENTRY UINT32_C(0x0004bf90)
#define VF2_TEXTURE_STATUS_WORD UINT32_C(0x0055c2f0)
#define VF2_TEXTURE_COUNTER0 UINT32_C(0x005502c0)
#define VF2_TEXTURE_COUNTER1 UINT32_C(0x005502d0)
#define VF2_TEXTURE_COUNTER2 UINT32_C(0x005502e0)
#define VF2_TEXTURE_FINAL_STATUS_TARGET UINT32_C(0x0004d25c)
#define VF2_TEXTURE_FINAL_STATUS_RETURN UINT32_C(0x0004bfdc)
#define VF2_TEXTURE_BODY_RETURN_ENTRY UINT32_C(0x0004bfdc)
#define VF2_TEXTURE_POST_BODY_CALL_ENTRY UINT32_C(0x0004bb94)
#define VF2_TEXTURE_POST_BODY_CALL_TARGET UINT32_C(0x0004b8d8)
#define VF2_TEXTURE_POST_BODY_CALL_RETURN UINT32_C(0x0004bb98)
#define VF2_TEXTURE_COUNTER_UPDATE_ENTRY UINT32_C(0x0004bb98)
#define VF2_TEXTURE_COUNTER_UPDATE_EXIT UINT32_C(0x0004bc58)
#define VF2_TEXTURE_ORCHESTRATOR_EPILOGUE_ENTRY UINT32_C(0x0004bc58)
#define VF2_TEXTURE_TREE_TABLE UINT32_C(0x0004ad78)
#define VF2_TEXTURE_CONVERT_STATE UINT32_C(0x005500f4)
#define VF2_TEXTURE_CONVERT_SOURCE UINT32_C(0x0055c2ec)
#define VF2_TEXTURE_CONVERT_ODD UINT32_C(0x0055c2ef)
#define VF2_TEXTURE_CONVERT_EVEN UINT32_C(0x0055c2ee)
#define VF2_FRAME_STATE UINT32_C(0x00500000)
#define VF2_FRAME_WAIT UINT32_C(0x0050008c)
#define VF2_TEXTURE_MAX_LOOP UINT32_C(0x00100000)


typedef struct texture_tree_stats {
    uint64_t instructions;
    uint64_t nested_calls;
    uint64_t writes;
    size_t bytes_written;
    uint32_t max_nested_depth;
} texture_tree_stats;

static vf2_status write_u16(
    vf2_model2a *machine,
    uint32_t address,
    uint16_t value
)
{
    const uint8_t bytes[2] = {
        (uint8_t)(value & UINT16_C(0x00ff)),
        (uint8_t)(value >> 8u)
    };
    return vf2_model2a_write(machine, address, bytes, sizeof(bytes));
}

static vf2_status read_u16(
    const vf2_model2a *machine,
    uint32_t address,
    uint16_t *value
)
{
    uint8_t bytes[2] = {0u, 0u};
    vf2_status status = VF2_OK;

    if (value == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read(machine, address, bytes, sizeof(bytes));
    if (status == VF2_OK) {
        *value = (uint16_t)((uint16_t)bytes[0] |
                            ((uint16_t)bytes[1] << 8u));
    }
    return status;
}

typedef struct texture_bit_reader {
    uint32_t next_address;
    uint32_t accumulator;
    uint32_t available_bits;
    uint32_t next_word;
    uint32_t last_shifted_word;
} texture_bit_reader;

static vf2_status texture_bit_reader_initialize(
    const vf2_model2a *machine,
    uint32_t address,
    texture_bit_reader *reader
)
{
    uint16_t word = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || reader == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(reader, 0, sizeof(*reader));
    status = read_u16(machine, address, &word);
    if (status != VF2_OK) {
        return status;
    }
    reader->next_address = address + UINT32_C(2);
    reader->next_word = word;
    return VF2_OK;
}

static vf2_status texture_bit_reader_refill(
    const vf2_model2a *machine,
    texture_bit_reader *reader
)
{
    uint16_t word = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || reader == NULL || reader->available_bits > 31u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    reader->last_shifted_word =
        reader->next_word << (reader->available_bits & UINT32_C(31));
    status = read_u16(machine, reader->next_address, &word);
    if (status != VF2_OK) {
        return status;
    }
    reader->next_address += UINT32_C(2);
    reader->accumulator |= reader->last_shifted_word;
    reader->available_bits += UINT32_C(16);
    reader->next_word = word;
    return VF2_OK;
}

static vf2_status texture_bit_reader_take(
    const vf2_model2a *machine,
    texture_bit_reader *reader,
    uint32_t width,
    uint32_t *value
)
{
    const uint32_t mask = width == UINT32_C(32)
        ? UINT32_MAX
        : (UINT32_C(1) << width) - UINT32_C(1);
    vf2_status status = VF2_OK;

    if (machine == NULL || reader == NULL || value == NULL ||
        width == 0u || width > UINT32_C(32) ||
        reader->available_bits < width) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *value = reader->accumulator & mask;
    reader->available_bits -= width;
    reader->accumulator >>= (width & UINT32_C(31));
    if (reader->available_bits <= UINT32_C(16)) {
        status = texture_bit_reader_refill(machine, reader);
    }
    return status;
}

static void set_equal_condition(vf2_i960_cpu *cpu)
{
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;
}


static void set_signed_condition(
    vf2_i960_cpu *cpu,
    int32_t left,
    int32_t right
)
{
    uint32_t condition = UINT32_C(2);
    vf2_i960_compare_result result = VF2_I960_COMPARE_EQUAL;

    if (left < right) {
        condition = UINT32_C(4);
        result = VF2_I960_COMPARE_LESS;
    } else if (left > right) {
        condition = UINT32_C(1);
        result = VF2_I960_COMPARE_GREATER;
    }
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | condition;
    cpu->compare_result = result;
}

static vf2_status finish_recovered_procedure(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint64_t instructions
)
{
    vf2_status status = VF2_OK;

    cpu->executed_instructions += instructions;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    return status;
}


static vf2_status copy_diagnostic_text(
    vf2_model2a *machine,
    uint32_t source,
    uint32_t destination,
    uint64_t *characters_written
)
{
    uint64_t characters = 0u;
    vf2_status status = VF2_OK;

    while (characters < UINT64_C(4096)) {
        uint8_t raw = 0u;
        status = vf2_model2a_read(machine, source, &raw, sizeof(raw));
        if (status != VF2_OK) {
            return status;
        }
        ++source;
        if (raw == 0u) {
            break;
        }
        status = write_u16(
            machine,
            destination,
            (uint16_t)(UINT16_C(0x8000) | (uint16_t)(int16_t)(int8_t)raw)
        );
        if (status != VF2_OK) {
            return status;
        }
        destination += UINT32_C(2);
        ++characters;
    }
    if (characters == UINT64_C(4096)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (characters_written != NULL) {
        *characters_written = characters;
    }
    return VF2_OK;
}

static void account_nested_procedure(
    vf2_i960_cpu *cpu,
    uint64_t calls,
    uint64_t returns
)
{
    const uint32_t nested_depth = cpu->local_frame_depth + (calls != 0u ? 1u : 0u);

    cpu->procedure_calls += calls;
    cpu->procedure_returns += returns;
    if (nested_depth > cpu->maximum_local_frame_depth) {
        cpu->maximum_local_frame_depth = nested_depth;
    }
}

static vf2_status classify_observed_game_state(
    const vf2_model2a *machine,
    uint32_t *classification,
    uint64_t *instructions
)
{
    uint32_t base = 0u;
    uint32_t flags = 0u;
    uint8_t mode = 0u;
    uint8_t mapped = 0u;
    vf2_status status = VF2_OK;

    if (classification == NULL || instructions == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x0050016c), &base);
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3324), &mode, sizeof(mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, base + UINT32_C(0x3320), &flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            UINT32_C(0x000028b8) + (uint32_t)mode,
            &mapped,
            sizeof(mapped)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    /* v0.0.22 accepts only the path observed on all eleven live calls. */
    if (mode == UINT8_C(25) ||
        (flags & UINT32_C(3)) != 0u ||
        mapped != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    *classification = 0u;
    *instructions = UINT64_C(16);
    return VF2_OK;
}

static vf2_status execute_game_state_classify(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t result = 0u;
    uint64_t instructions = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = classify_observed_game_state(machine, &result, &instructions);
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[16] = result;
    set_equal_condition(cpu);
    status = finish_recovered_procedure(machine, cpu, instructions);
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_GAME_STATE_CLASSIFY;
    report->entry_address = VF2_GAME_STATE_CLASSIFY_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_game_color_lookup(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint32_t selector = cpu->registers[16];
    const uint32_t stack_address = cpu->registers[1];
    const uint32_t saved_g9 = cpu->registers[25];
    uint32_t base = 0u;
    uint32_t flags = 0u;
    uint32_t classification = 0u;
    uint32_t color = 0u;
    uint64_t classify_instructions = 0u;
    uint64_t own_instructions = UINT64_C(19);
    uint8_t mode = 0u;
    uint8_t color_index = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u || selector > UINT32_C(1)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_write_u32(machine, stack_address, saved_g9);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050016c), &base
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, base + UINT32_C(0x3320), &flags
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if ((flags & (UINT32_C(1) << 1u)) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = classify_observed_game_state(
        machine, &classification, &classify_instructions
    );
    if (status != VF2_OK || classification != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_model2a_read(
        machine, base + UINT32_C(0x3324), &mode, sizeof(mode)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            UINT32_C(0x000027ac) + (uint32_t)mode * UINT32_C(2) + selector,
            &color_index,
            sizeof(color_index)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            UINT32_C(0x000027e0) + (uint32_t)color_index * UINT32_C(4),
            &color
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    color += UINT32_C(0x00010101);
    cpu->registers[16] = color;
    if (selector != 0u) {
        ++own_instructions;
    }
    set_equal_condition(cpu);
    account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
    status = finish_recovered_procedure(
        machine, cpu, own_instructions + classify_instructions
    );
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_GAME_COLOR_LOOKUP;
    report->entry_address = VF2_GAME_COLOR_LOOKUP_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(1);
    report->bytes_written = sizeof(uint32_t);
    report->recovered_instruction_count = own_instructions + classify_instructions;
    report->recovered_procedure_calls = UINT64_C(1);
    report->recovered_procedure_returns = UINT64_C(2);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_inline_text_thunk(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint32_t source = cpu->registers[14];
    const uint32_t destination = cpu->registers[25];
    uint32_t cursor = source;
    uint32_t word = 0u;
    uint64_t characters = 0u;
    uint64_t words = 0u;
    uint64_t instructions = 0u;
    vf2_status status = VF2_OK;

    status = copy_diagnostic_text(
        machine, source, destination, &characters
    );
    if (status != VF2_OK) {
        return status;
    }
    do {
        status = vf2_model2a_read_u32(machine, cursor, &word);
        if (status != VF2_OK) {
            return status;
        }
        cursor += UINT32_C(4);
        ++words;
        if (words > UINT64_C(1024)) {
            return VF2_ERROR_UNSUPPORTED;
        }
    } while (word > UINT32_C(0x00ffffff));

    cpu->registers[16] = word;
    cpu->registers[25] = destination + UINT32_C(128);
    cpu->registers[2] = UINT32_C(0x00009450);
    cpu->registers[14] = cursor;
    cpu->registers[15] = UINT32_C(0x00ffffff);
    cpu->ip = cursor;
    set_equal_condition(cpu);
    account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
    instructions = characters * UINT64_C(8) + UINT64_C(17) +
                   words * UINT64_C(3);
    cpu->executed_instructions += instructions;

    report->kind = VF2_HYBRID_BRIDGE_INLINE_TEXT_THUNK;
    report->entry_address = VF2_INLINE_TEXT_THUNK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = words;
    report->rows = characters;
    report->bytes_written = (size_t)characters * 2u;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_calls = UINT64_C(1);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_texture_status_line(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint32_t stack_address = cpu->registers[1];
    const uint32_t texture_index = cpu->registers[16];
    uint16_t label_kind = 0u;
    uint8_t display_mode = 0u;
    uint32_t inline_source = UINT32_C(0x0004d2e8);
    uint32_t inline_destination = UINT32_C(0x010000e2);
    uint32_t name_destination = UINT32_C(0x010000ea);
    uint32_t name_source = 0u;
    uint32_t inline_cursor = 0u;
    uint32_t inline_word = 0u;
    uint64_t inline_characters = 0u;
    uint64_t inline_words = 0u;
    uint64_t name_characters = 0u;
    uint64_t instructions = UINT64_C(4);
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_write_u32(machine, stack_address, texture_index);
    if (status == VF2_OK) {
        status = read_u16(machine, UINT32_C(0x0055c2f2), &label_kind);
    }
    if (status != VF2_OK) {
        return status;
    }
    if (label_kind == UINT16_C(1)) {
        inline_source = UINT32_C(0x0004d30c);
        instructions += UINT64_C(2);
    } else {
        instructions += UINT64_C(3);
    }
    status = copy_diagnostic_text(
        machine, inline_source, inline_destination, &inline_characters
    );
    if (status != VF2_OK) {
        return status;
    }
    inline_cursor = inline_source;
    do {
        status = vf2_model2a_read_u32(machine, inline_cursor, &inline_word);
        if (status != VF2_OK) {
            return status;
        }
        inline_cursor += UINT32_C(4);
        ++inline_words;
        if (inline_words > UINT64_C(1024)) {
            return VF2_ERROR_UNSUPPORTED;
        }
    } while (inline_word > UINT32_C(0x00ffffff));
    instructions += inline_characters * UINT64_C(8) + UINT64_C(17) +
                    inline_words * UINT64_C(3);

    status = vf2_model2a_read(
        machine, UINT32_C(0x0050002b), &display_mode, sizeof(display_mode)
    );
    if (status != VF2_OK) {
        return status;
    }
    if (display_mode == UINT8_C(12)) {
        name_destination = UINT32_C(0x010040ea);
        instructions += UINT64_C(11);
    } else if (display_mode == UINT8_C(13)) {
        name_destination = UINT32_C(0x010040ea);
        instructions += UINT64_C(13);
    } else {
        instructions += UINT64_C(12);
    }
    name_source = UINT32_C(0x0004d377) + texture_index * UINT32_C(32);
    status = copy_diagnostic_text(
        machine, name_source, name_destination, &name_characters
    );
    if (status != VF2_OK) {
        return status;
    }
    instructions += name_characters * UINT64_C(8) + UINT64_C(8);

    cpu->registers[16] = name_source;
    cpu->registers[25] = name_destination;
    set_equal_condition(cpu);
    account_nested_procedure(cpu, UINT64_C(2), UINT64_C(2));
    status = finish_recovered_procedure(machine, cpu, instructions);
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_STATUS_LINE;
    report->entry_address = VF2_TEXTURE_STATUS_LINE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = inline_words;
    report->rows = inline_characters + name_characters;
    report->bytes_written =
        (size_t)(inline_characters + name_characters) * 2u + sizeof(uint32_t);
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_calls = UINT64_C(2);
    report->recovered_procedure_returns = UINT64_C(3);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_texture_address_table(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t g6 = cpu->registers[22];
    uint32_t g7 = cpu->registers[23];
    const uint32_t g8 = cpu->registers[24];
    uint32_t r9 = 0u;
    uint32_t r10 = 0u;
    uint32_t r11 = UINT32_C(0x0055c2f8);
    uint32_t r6 = 0u;
    uint32_t r7 = 0u;
    uint32_t r8 = UINT32_C(9);
    uint64_t instructions = UINT64_C(1);
    size_t pointers_written = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    if ((g8 & UINT32_C(1)) == 0u) {
        r9 = UINT32_C(0x12600000);
        r10 = UINT32_C(0x12200000);
        instructions += UINT64_C(3);
    } else {
        r10 = UINT32_C(0x12600000);
        r9 = UINT32_C(0x12200000);
        instructions += UINT64_C(2);
    }
    instructions += UINT64_C(4);

    if ((g7 & (UINT32_C(1) << 10u)) != 0u) {
        instructions += UINT64_C(1);
        if ((g6 & (UINT32_C(1) << 9u)) != 0u) {
            g7 &= ~(UINT32_C(1) << 10u);
            g6 &= ~(UINT32_C(1) << 9u);
            g7 <<= 1u;
            g6 <<= 1u;
            instructions += UINT64_C(5);
        } else {
            const uint32_t address_index =
                ((g6 + UINT32_C(0x400)) << 9u) +
                (g7 & ~(UINT32_C(1) << 10u));
            status = vf2_model2a_write_u32(
                machine, r11, r9 + address_index * UINT32_C(2)
            );
            if (status != VF2_OK) {
                return status;
            }
            r11 += UINT32_C(4);
            {
                const uint32_t swap = r9;
                r9 = r10;
                r10 = swap;
            }
            ++pointers_written;
            instructions += UINT64_C(11);
        }
    } else {
        const uint32_t address_index = (g6 << 9u) + g7;
        status = vf2_model2a_write_u32(
            machine, r11, r9 + address_index * UINT32_C(2)
        );
        if (status != VF2_OK) {
            return status;
        }
        r11 += UINT32_C(4);
        {
            const uint32_t swap = r9;
            r9 = r10;
            r10 = swap;
        }
        ++pointers_written;
        instructions += UINT64_C(8);
    }

    r9 += UINT32_C(0x00180000);
    r10 += UINT32_C(0x00180000);
    instructions += UINT64_C(3);
    while (r8 != 0u) {
        uint32_t r3 = 0u;
        uint32_t r4 = 0u;
        uint32_t r15 = 0u;
        uint32_t shift = 0u;

        g6 >>= 1u;
        g7 >>= 1u;
        g6 &= ~UINT32_C(1);
        g7 &= ~UINT32_C(1);
        r3 = r6 + g6;
        r4 = r7 + g7;
        r3 = (r3 << 9u) + r4;
        r15 = r9 + r3 * UINT32_C(2);
        status = vf2_model2a_write_u32(machine, r11, r15);
        if (status != VF2_OK) {
            return status;
        }
        r11 += UINT32_C(4);
        {
            const uint32_t swap = r9;
            r9 = r10;
            r10 = swap;
        }
        shift = UINT32_C(1) << (r8 & UINT32_C(31));
        r7 += shift;
        r6 += shift >> 1u;
        --r8;
        ++pointers_written;
        instructions += UINT64_C(20);
    }

    cpu->registers[22] = g6;
    cpu->registers[23] = g7;
    set_equal_condition(cpu);
    status = finish_recovered_procedure(machine, cpu, instructions + UINT64_C(1));
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_ADDRESS_TABLE;
    report->entry_address = VF2_TEXTURE_ADDRESS_TABLE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = pointers_written;
    report->bytes_written = pointers_written * sizeof(uint32_t);
    report->recovered_instruction_count = instructions + UINT64_C(1);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_diagnostic_text_copy(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint32_t source = cpu->registers[16];
    const uint32_t destination = cpu->registers[25];
    uint64_t characters = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = copy_diagnostic_text(
        machine, source, destination, &characters
    );
    if (status != VF2_OK) {
        return status;
    }

    set_equal_condition(cpu);
    status = finish_recovered_procedure(
        machine, cpu, characters * UINT64_C(8) + UINT64_C(8)
    );
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_HYBRID_BRIDGE_DIAGNOSTIC_TEXT_COPY;
    report->entry_address = VF2_DIAGNOSTIC_TEXT_COPY_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = characters;
    report->bytes_written = (size_t)characters * 2u;
    report->recovered_instruction_count = characters * UINT64_C(8) + UINT64_C(8);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status translate_glyph_word(
    const vf2_model2a *machine,
    uint32_t source,
    uint32_t *translated
)
{
    uint8_t mapped[4] = {0u, 0u, 0u, 0u};
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (translated == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0u; index < 4u; ++index) {
        const uint32_t table_address =
            UINT32_C(0x00050628) + ((source >> (index * 8u)) & UINT32_C(0xff));
        status = vf2_model2a_read(
            machine, table_address, &mapped[index], sizeof(mapped[index])
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    *translated = ((uint32_t)mapped[0] << 8u) |
                  (uint32_t)mapped[1] |
                  ((uint32_t)mapped[2] << 24u) |
                  ((uint32_t)mapped[3] << 16u);
    return VF2_OK;
}

static vf2_status execute_tile_glyph_expand(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t offset = 0u;
    size_t writes = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    for (offset = 0u; offset < UINT32_C(384); offset += UINT32_C(8)) {
        uint32_t first = 0u;
        uint32_t second = 0u;
        uint32_t first_mapped = 0u;
        uint32_t second_mapped = 0u;
        uint32_t copy = 0u;
        const uint32_t destination =
            UINT32_C(0x0100c000) + offset * UINT32_C(8);

        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0059f280) + offset, &first
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x0059f280) + offset + UINT32_C(4), &second
            );
        }
        if (status == VF2_OK) {
            status = translate_glyph_word(machine, first, &first_mapped);
        }
        if (status == VF2_OK) {
            status = translate_glyph_word(machine, second, &second_mapped);
        }
        if (status != VF2_OK) {
            return status;
        }
        for (copy = 0u; copy < UINT32_C(8); ++copy) {
            status = vf2_model2a_write_u32(
                machine,
                destination + copy * UINT32_C(8),
                first_mapped
            );
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine,
                    destination + copy * UINT32_C(8) + UINT32_C(4),
                    second_mapped
                );
            }
            if (status != VF2_OK) {
                return status;
            }
            writes += 2u;
        }
    }

    set_equal_condition(cpu);
    status = finish_recovered_procedure(machine, cpu, UINT64_C(2598));
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_HYBRID_BRIDGE_TILE_GLYPH_EXPAND;
    report->entry_address = VF2_TILE_GLYPH_EXPAND_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(48);
    report->rows = UINT64_C(8);
    report->changed_values = UINT64_C(96);
    report->bytes_written = writes * sizeof(uint32_t);
    report->recovered_instruction_count = UINT64_C(2598);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_video_status_latch(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t value = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x0098000c), &value);
    if (status == VF2_OK) {
        const uint8_t low = (uint8_t)value;
        status = vf2_model2a_write(
            machine, UINT32_C(0x00501000), &low, sizeof(low)
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    status = finish_recovered_procedure(machine, cpu, UINT64_C(4));
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_HYBRID_BRIDGE_VIDEO_STATUS_LATCH;
    report->entry_address = VF2_VIDEO_STATUS_LATCH_ENTRY;
    report->exit_address = cpu->ip;
    report->bytes_written = 1u;
    report->recovered_instruction_count = UINT64_C(4);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_geometry_frame_commit(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint32_t geometry_base = cpu->registers[26];
    const uint32_t mask = UINT32_C(0x0007fffc);
    uint32_t previous_command = 0u;
    uint32_t read_pointer = 0u;
    uint32_t max_distance = 0u;
    int32_t distance = 0;
    uint8_t ring_index = 0u;
    uint32_t next_command = 0u;
    uint64_t instructions = UINT64_C(20);
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u || geometry_base != VF2_GEOMETRY_BASE) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00501004), &previous_command
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, geometry_base + UINT32_C(0x000000f0), 0u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, geometry_base + UINT32_C(0x00003008), previous_command
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, geometry_base + UINT32_C(0x00002008), &read_pointer
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00501008), &max_distance
        );
    }
    distance = (int32_t)((read_pointer & mask) - (previous_command & mask));
    set_signed_condition(cpu, distance, (int32_t)max_distance);
    if (status == VF2_OK && distance > (int32_t)max_distance) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00501008), (uint32_t)distance
        );
        ++instructions;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x0050100c), &ring_index, sizeof(ring_index)
        );
    }
    ring_index = (uint8_t)((ring_index + 1u) % 4u);
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050100c), &ring_index, sizeof(ring_index)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00007a00) + (uint32_t)ring_index * UINT32_C(4),
            &next_command
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00501004), next_command
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, geometry_base + UINT32_C(0x00001008), next_command
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    status = finish_recovered_procedure(machine, cpu, instructions);
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_HYBRID_BRIDGE_GEOMETRY_FRAME_COMMIT;
    report->entry_address = VF2_GEOMETRY_FRAME_COMMIT_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(2);
    report->bytes_written = 17u;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_geometry_command_setup(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint32_t geometry_base = cpu->registers[26];
    const uint32_t destination_offset = cpu->registers[28];
    uint16_t signed_source = 0u;
    uint8_t command_class = 0u;
    uint32_t command = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u || geometry_base != VF2_GEOMETRY_BASE) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_write_u32(
        machine, geometry_base + UINT32_C(0x80), 0u
    );
    if (status == VF2_OK) {
        status = read_u16(machine, UINT32_C(0x005010de), &signed_source);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x005010dc), &command_class,
            sizeof(command_class)
        );
    }
    command = ((uint32_t)(int32_t)(int16_t)signed_source << 8u) &
              UINT32_C(0x807fffff);
    command |= (uint32_t)command_class << 23u;
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x005010e0), command
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, geometry_base + destination_offset, command
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    status = finish_recovered_procedure(machine, cpu, UINT64_C(12));
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_HYBRID_BRIDGE_GEOMETRY_COMMAND_SETUP;
    report->entry_address = VF2_GEOMETRY_COMMAND_SETUP_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->bytes_written = 12u;
    report->recovered_instruction_count = UINT64_C(12);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_frame_scratch_clear(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t index = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    for (index = 0u; index < UINT32_C(43); ++index) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00501800) + index * UINT32_C(4), 0u
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    set_equal_condition(cpu);
    status = finish_recovered_procedure(machine, cpu, UINT64_C(176));
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_HYBRID_BRIDGE_FRAME_SCRATCH_CLEAR;
    report->entry_address = VF2_FRAME_SCRATCH_CLEAR_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(43);
    report->bytes_written = 43u * sizeof(uint32_t);
    report->recovered_instruction_count = UINT64_C(176);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}


static vf2_status execute_palette_page_upload(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t active = 0u;
    uint32_t page = 0u;
    uint64_t instructions = UINT64_C(2);
    uint64_t pages_uploaded = 0u;
    size_t bytes_written = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00546000), &active);
    if (status != VF2_OK) {
        return status;
    }
    if (active == 0u) {
        set_equal_condition(cpu);
        status = finish_recovered_procedure(machine, cpu, UINT64_C(3));
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_PALETTE_PAGE_UPLOAD;
        report->entry_address = VF2_PALETTE_PAGE_UPLOAD_ENTRY;
        report->exit_address = cpu->ip;
        report->recovered_instruction_count = UINT64_C(3);
        report->recovered_procedure_returns = UINT64_C(1);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

    status = vf2_model2a_read_u32(machine, UINT32_C(0x00546004), &page);
    if (status != VF2_OK || page > UINT32_C(27)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    instructions += UINT64_C(13);
    for (;;) {
        uint32_t source = UINT32_C(0x00546008) + page * UINT32_C(288);
        uint32_t destination = UINT32_C(0x01800000) + page * UINT32_C(512);
        uint32_t inner = 0u;
        uint32_t old_page = page;
        uint32_t timer3 = 0u;
        uint32_t elapsed = 0u;

        instructions += UINT64_C(1);
        for (inner = 0u; inner < UINT32_C(48); ++inner) {
            uint16_t first = 0u;
            uint16_t second = 0u;
            uint16_t third = 0u;
            status = read_u16(machine, source, &first);
            if (status == VF2_OK) {
                status = read_u16(machine, source + UINT32_C(2), &second);
            }
            if (status == VF2_OK) {
                status = read_u16(machine, source + UINT32_C(4), &third);
            }
            if (status == VF2_OK) {
                status = write_u16(
                    machine, destination + UINT32_C(0x10000), first
                );
            }
            if (status == VF2_OK) {
                status = write_u16(
                    machine, destination + UINT32_C(0x14000), second
                );
            }
            if (status == VF2_OK) {
                status = write_u16(
                    machine, destination + UINT32_C(0x18000), third
                );
            }
            if (status != VF2_OK) {
                return status;
            }
            source += UINT32_C(6);
            destination += UINT32_C(2);
            bytes_written += 6u;
            instructions += UINT64_C(12);
        }
        destination += UINT32_C(416);
        (void)destination;
        page = old_page + UINT32_C(1);
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00546004), page
        );
        if (status != VF2_OK) {
            return status;
        }
        ++pages_uploaded;
        instructions += UINT64_C(5);
        if (old_page == UINT32_C(27)) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00546000), 0u
            );
            if (status != VF2_OK) {
                return status;
            }
            set_equal_condition(cpu);
            instructions += UINT64_C(3);
            break;
        }

        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00f0000c), &timer3
        );
        if (status != VF2_OK) {
            return status;
        }
        elapsed = (UINT32_C(0x000fffff) -
                   (timer3 & UINT32_C(0x000fffff)) - UINT32_C(18)) /
                  UINT32_C(25);
        instructions += UINT64_C(8);
        if (elapsed > UINT32_C(0x62c)) {
            cpu->arithmetic_control =
                (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
            cpu->compare_result = VF2_I960_COMPARE_GREATER;
            ++instructions;
            break;
        }
        ++instructions;
        if (pages_uploaded >= UINT64_C(28)) {
            return VF2_ERROR_UNSUPPORTED;
        }
    }

    status = finish_recovered_procedure(machine, cpu, instructions);
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_HYBRID_BRIDGE_PALETTE_PAGE_UPLOAD;
    report->entry_address = VF2_PALETTE_PAGE_UPLOAD_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = pages_uploaded * UINT64_C(48);
    report->rows = pages_uploaded;
    report->bytes_written = bytes_written;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_texture_convert_loop(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t r12 = cpu->registers[12] >> 1u;
    uint32_t r13 = cpu->registers[13] >> 1u;
    uint64_t instructions = UINT64_C(3);
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    cpu->registers[12] = r12;
    cpu->registers[13] = r13;
    if (r12 == 0u) {
        set_equal_condition(cpu);
        status = finish_recovered_procedure(machine, cpu, UINT64_C(4));
        instructions = UINT64_C(4);
        if (status != VF2_OK) {
            return status;
        }
        report->recovered_procedure_returns = UINT64_C(1);
    } else if (r13 == 0u) {
        set_equal_condition(cpu);
        status = finish_recovered_procedure(machine, cpu, UINT64_C(5));
        instructions = UINT64_C(5);
        if (status != VF2_OK) {
            return status;
        }
        report->recovered_procedure_returns = UINT64_C(1);
    } else {
        uint32_t source = 0u;
        set_equal_condition(cpu);
        cpu->registers[11] += UINT32_C(4);
        status = vf2_model2a_read_u32(machine, cpu->registers[11], &source);
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[27] = source;
        cpu->registers[28] = r12;
        cpu->registers[29] = r13;
        cpu->executed_instructions += UINT64_C(9);
        status = vf2_i960_cpu_enter_procedure(
            cpu, VF2_TEXTURE_CONVERT_ENTRY, VF2_TEXTURE_CONVERT_POST_ENTRY
        );
        instructions = UINT64_C(9);
        if (status != VF2_OK) {
            return status;
        }
        report->recovered_procedure_calls = UINT64_C(1);
    }

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_CONVERT_LOOP;
    report->entry_address = VF2_TEXTURE_CONVERT_LOOP_ENTRY;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = instructions;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_texture_convert_post(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t state = 0u;
    vf2_status status = VF2_OK;

    status = vf2_model2a_read_u32(
        machine, VF2_TEXTURE_CONVERT_STATE, &state
    );
    if (status != VF2_OK || state != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[16] = 0u;
    set_equal_condition(cpu);
    cpu->ip = VF2_TEXTURE_CONVERT_LOOP_ENTRY;
    cpu->executed_instructions += UINT64_C(3);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_CONVERT_POST;
    report->entry_address = VF2_TEXTURE_CONVERT_POST_ENTRY;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = UINT64_C(3);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_timer_wait_update(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint32_t mask = UINT32_C(0x000fffff);
    const uint32_t input = cpu->registers[16];
    uint32_t timer3 = 0u;
    int32_t delta = 0;
    uint8_t wait_value = 0u;
    uint64_t instructions = UINT64_C(11);
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_write_u32(
        machine, UINT32_C(0x00f0000c), mask
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00f0000c), &timer3
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    delta = (int32_t)((timer3 & mask) - (mask - UINT32_C(25) * input));
    set_signed_condition(cpu, 0, delta);
    if (delta > 0) {
        wait_value = 0u;
        cpu->registers[16] = 0u;
        instructions = UINT64_C(12);
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050008c), &wait_value, sizeof(wait_value)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00f0000c), (uint32_t)delta
            );
        }
    } else {
        wait_value = 1u;
        cpu->registers[16] = 1u;
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050008c), &wait_value, sizeof(wait_value)
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    /* The observed caller-facing post-state leaves CC equal on return. */
    set_equal_condition(cpu);
    status = finish_recovered_procedure(machine, cpu, instructions);
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_HYBRID_BRIDGE_TIMER_WAIT_UPDATE;
    report->entry_address = VF2_TIMER_WAIT_UPDATE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->bytes_written = delta > 0 ? 5u : 1u;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_texture_word_prepare(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report timer_report;
    const uint32_t mask = UINT32_C(0x000fffff);
    uint32_t tick = 0u;
    uint32_t timer2 = 0u;
    uint32_t timer3 = 0u;
    uint32_t input = 0u;
    uint32_t child_state = 0u;
    uint32_t flags = 0u;
    uint64_t wrapper_instructions = UINT64_C(22);
    uint8_t frame_state = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(&timer_report, 0, sizeof(timer_report));
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00550004), &tick);
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_FRAME_STATE, &frame_state, sizeof(frame_state)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00f00008), &timer2
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00f0000c), &timer3
        );
    }
    if (status != VF2_OK || frame_state != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[13] = mask - UINT32_C(25) * tick;
    cpu->registers[14] = mask;
    cpu->registers[15] = timer3 & mask;
    if (cpu->registers[15] != mask) {
        ++wrapper_instructions;
        if (cpu->registers[15] > cpu->registers[13]) {
            return VF2_ERROR_UNSUPPORTED;
        }
    }
    (void)timer2;
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00550008), &input);
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[16] = input;
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_TIMER_WAIT_UPDATE_ENTRY, VF2_TEXTURE_WORD_PREPARE_TIMER_RETURN
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions += wrapper_instructions - UINT64_C(10);
    status = execute_timer_wait_update(machine, cpu, &timer_report);
    if (status != VF2_OK || cpu->ip != VF2_TEXTURE_WORD_PREPARE_TIMER_RETURN) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = vf2_model2a_read_u32(
        machine, VF2_TEXTURE_HEADER_STATE, &child_state
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_TEXTURE_ACTIVE_FLAGS, &flags
        );
    }
    if (status != VF2_OK || child_state != 0u ||
        (flags & (UINT32_C(1) << 1u)) != 0u ||
        (flags & (UINT32_C(1) << 2u)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[16] = flags;
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0055c340), &cpu->registers[8]
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0055c2f8), &cpu->registers[11]
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[9] = UINT32_C(0x005502f0);
    cpu->ip = VF2_TEXTURE_WORD_PREPARE_EXIT;
    cpu->executed_instructions += UINT64_C(10);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_WORD_PREPARE;
    report->entry_address = VF2_TEXTURE_WORD_PREPARE_ENTRY;
    report->exit_address = VF2_TEXTURE_WORD_PREPARE_EXIT;
    report->iterations = UINT64_C(1);
    report->bytes_written = timer_report.bytes_written;
    report->recovered_instruction_count =
        wrapper_instructions + timer_report.recovered_instruction_count;
    report->recovered_procedure_calls = UINT64_C(1);
    report->recovered_procedure_returns =
        timer_report.recovered_procedure_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}


static vf2_status execute_texture_color_prepare(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report timer_report;
    const uint32_t mask = UINT32_C(0x000fffff);
    uint32_t tick = 0u;
    uint32_t timer2 = 0u;
    uint32_t timer3 = 0u;
    uint32_t input = 0u;
    uint32_t child_state = 0u;
    uint32_t flags = 0u;
    uint16_t width = 0u;
    uint16_t height = 0u;
    uint8_t frame_state = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(&timer_report, 0, sizeof(timer_report));
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00550004), &tick);
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_FRAME_STATE, &frame_state, sizeof(frame_state)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00f00008), &timer2);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00f0000c), &timer3);
    }
    if (status != VF2_OK || frame_state != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[13] = mask - UINT32_C(25) * tick;
    cpu->registers[14] = mask;
    cpu->registers[15] = timer3 & mask;
    if (cpu->registers[15] == mask ||
        cpu->registers[15] > cpu->registers[13]) {
        return VF2_ERROR_UNSUPPORTED;
    }
    (void)timer2;
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00550008), &input);
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[16] = input;
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_TIMER_WAIT_UPDATE_ENTRY, VF2_TEXTURE_COLOR_PREPARE_TIMER_RETURN
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions += UINT64_C(13);
    status = execute_timer_wait_update(machine, cpu, &timer_report);
    if (status != VF2_OK || cpu->ip != VF2_TEXTURE_COLOR_PREPARE_TIMER_RETURN) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_model2a_read_u32(machine, VF2_TEXTURE_HEADER_STATE, &child_state);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, VF2_TEXTURE_ACTIVE_FLAGS, &flags);
    }
    if (status != VF2_OK || child_state != 0u ||
        (flags & (UINT32_C(1) << 1u)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[16] = flags;
    status = read_u16(machine, UINT32_C(0x0055c320), &width);
    if (status == VF2_OK) {
        status = read_u16(machine, UINT32_C(0x0055c322), &height);
    }
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[11] = UINT32_C(0x0055c2f8);
    cpu->registers[12] = width;
    cpu->registers[13] = height;
    cpu->registers[26] = UINT32_C(1) << 11u;
    cpu->ip = VF2_TEXTURE_COLOR_PREPARE_EXIT;
    cpu->executed_instructions += UINT64_C(8);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_COLOR_PREPARE;
    report->entry_address = VF2_TEXTURE_COLOR_PREPARE_ENTRY;
    report->exit_address = VF2_TEXTURE_COLOR_PREPARE_EXIT;
    report->iterations = UINT64_C(1);
    report->bytes_written = timer_report.bytes_written;
    report->recovered_instruction_count =
        UINT64_C(21) + timer_report.recovered_instruction_count;
    report->recovered_procedure_calls = UINT64_C(1);
    report->recovered_procedure_returns = timer_report.recovered_procedure_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}


static vf2_status texture_tree_write_leaf(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t level,
    uint32_t packed,
    uint32_t start_index,
    texture_tree_stats *stats
)
{
    uint32_t index = start_index;
    uint32_t stride = 0u;
    uint32_t table_value = 0u;
    vf2_status status = VF2_OK;

    cpu->registers[20] = packed | ((level & UINT32_C(0xff)) << 24u);
    cpu->registers[16] = cpu->registers[20] >> 28u;
    status = vf2_model2a_read_u32(
        machine,
        VF2_TEXTURE_TREE_TABLE + cpu->registers[16] * UINT32_C(4),
        &table_value
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[21] = table_value;
    stride = UINT32_C(1) << (level & UINT32_C(31));
    if (stride == 0u || cpu->registers[25] == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    stats->instructions += UINT64_C(5);
    do {
        const uint32_t address =
            cpu->registers[26] + index * UINT32_C(8);
        status = vf2_model2a_write_u32(
            machine, address, cpu->registers[20]
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, address + UINT32_C(4), cpu->registers[21]
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        stats->instructions += UINT64_C(3);
        ++stats->writes;
        stats->bytes_written += 8u;
        index += stride;
    } while (index < cpu->registers[25]);
    stats->instructions += UINT64_C(1);
    return VF2_OK;
}

static vf2_status texture_tree_expand_recursive(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t level,
    uint32_t node_address,
    uint32_t output_index,
    uint32_t nested_depth,
    texture_tree_stats *stats
)
{
    const uint32_t next_level = level + UINT32_C(1);
    uint32_t child_address = 0u;
    uint32_t packed = 0u;
    uint32_t second_index = 0u;
    vf2_status status = VF2_OK;

    stats->instructions += UINT64_C(1);
    if (level == UINT32_C(8)) {
        status = vf2_model2a_write_u32(
            machine,
            cpu->registers[26] + output_index * UINT32_C(8),
            node_address
        );
        if (status != VF2_OK) {
            return status;
        }
        stats->instructions += UINT64_C(2);
        ++stats->writes;
        stats->bytes_written += 4u;
        return VF2_OK;
    }
    if (level > UINT32_C(8) || nested_depth > UINT32_C(16)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    child_address = node_address + UINT32_C(4);
    stats->instructions += UINT64_C(8);
    status = vf2_model2a_read_u32(machine, child_address, &packed);
    if (status != VF2_OK) {
        return status;
    }
    if ((int32_t)packed >= 0) {
        cpu->registers[30] = next_level;
        cpu->registers[29] = child_address;
        cpu->registers[28] = output_index;
        stats->instructions += UINT64_C(4);
        ++stats->nested_calls;
        if (nested_depth + UINT32_C(1) > stats->max_nested_depth) {
            stats->max_nested_depth = nested_depth + UINT32_C(1);
        }
        status = texture_tree_expand_recursive(
            machine, cpu, next_level, child_address, output_index,
            nested_depth + UINT32_C(1), stats
        );
    } else {
        status = texture_tree_write_leaf(
            machine, cpu, next_level, packed, output_index, stats
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    stats->instructions += UINT64_C(5);
    status = vf2_model2a_read_u32(machine, node_address, &child_address);
    if (status != VF2_OK) {
        return status;
    }
    second_index = output_index +
        (UINT32_C(1) << (level & UINT32_C(31)));
    status = vf2_model2a_read_u32(machine, child_address, &packed);
    if (status != VF2_OK) {
        return status;
    }
    if ((int32_t)packed >= 0) {
        cpu->registers[30] = next_level;
        cpu->registers[29] = child_address;
        cpu->registers[28] = second_index;
        stats->instructions += UINT64_C(4);
        ++stats->nested_calls;
        if (nested_depth + UINT32_C(1) > stats->max_nested_depth) {
            stats->max_nested_depth = nested_depth + UINT32_C(1);
        }
        status = texture_tree_expand_recursive(
            machine, cpu, next_level, child_address, second_index,
            nested_depth + UINT32_C(1), stats
        );
    } else {
        status = texture_tree_write_leaf(
            machine, cpu, next_level, packed, second_index, stats
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    stats->instructions += UINT64_C(1);
    return VF2_OK;
}

static vf2_status execute_texture_byte_run(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint32_t count = cpu->registers[17];
    const uint8_t value = (uint8_t)cpu->registers[8];
    uint32_t address = cpu->registers[10];
    uint32_t index = 0u;
    vf2_status status = VF2_OK;

    if (count == 0u || count > VF2_TEXTURE_MAX_LOOP) {
        return VF2_ERROR_UNSUPPORTED;
    }
    for (index = 0u; index < count; ++index) {
        status = vf2_model2a_write(machine, address, &value, sizeof(value));
        if (status != VF2_OK) {
            return status;
        }
        address -= UINT32_C(2);
    }
    cpu->registers[10] = address;
    cpu->registers[17] = 0u;
    cpu->ip = VF2_TEXTURE_BYTE_RUN_EXIT;
    cpu->executed_instructions += (uint64_t)count * UINT64_C(4);
    set_equal_condition(cpu);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_BYTE_RUN;
    report->entry_address = VF2_TEXTURE_BYTE_RUN_ENTRY;
    report->exit_address = VF2_TEXTURE_BYTE_RUN_EXIT;
    report->iterations = count;
    report->bytes_written = count;
    report->recovered_instruction_count =
        (uint64_t)count * UINT64_C(4);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_texture_byte_decode(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t r4 = cpu->registers[4];
    uint32_t r5 = cpu->registers[5];
    uint32_t r6 = cpu->registers[6];
    uint32_t r7 = cpu->registers[7];
    uint32_t r8 = cpu->registers[8];
    uint32_t r9 = cpu->registers[9];
    uint32_t r10 = cpu->registers[10];
    uint32_t r11 = cpu->registers[11];
    uint32_t r12 = cpu->registers[12];
    uint32_t r13 = cpu->registers[13];
    uint32_t r14 = cpu->registers[14];
    uint32_t r15 = cpu->registers[15];
    uint32_t g0 = cpu->registers[16];
    uint32_t g1 = cpu->registers[17];
    uint32_t g2 = cpu->registers[18];
    uint32_t g3 = cpu->registers[19];
    uint32_t g4 = cpu->registers[20];
    uint32_t g5 = cpu->registers[21];
    uint32_t g11 = cpu->registers[27];
    uint32_t g12 = cpu->registers[28];
    uint32_t g13 = cpu->registers[29];
    uint32_t g14 = cpu->registers[30];
    uint64_t instructions = 0u;
    uint64_t outputs = 0u;
    uint64_t encoded_runs = 0u;
    uint32_t outer_iterations = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u || g14 == 0u || g14 > UINT32_C(4096)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    while (g14 != 0u) {
        uint8_t frame_state = 0u;
        uint16_t row_count = 0u;

        status = vf2_model2a_read(
            machine, VF2_FRAME_STATE, &frame_state, sizeof(frame_state)
        );
        if (status != VF2_OK) {
            return status;
        }
        instructions += UINT64_C(2);
        if (frame_state != 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = read_u16(machine, UINT32_C(0x0055c322), &row_count);
        if (status != VF2_OK || row_count == 0u) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        g13 = row_count;
        instructions += UINT64_C(2);

        while (g13 != 0u) {
            uint32_t handler = 0u;

            instructions += UINT64_C(3);
            g2 = g4 >> 24u;
            if ((int32_t)g4 >= 0) {
                g3 = g4;
                r4 = UINT32_C(8);
                instructions += UINT64_C(2);
                for (;;) {
                    const bool bit_set =
                        (r13 & (UINT32_C(1) << (r4 & UINT32_C(31)))) != 0u;
                    status = vf2_model2a_read_u32(machine, g3, &g4);
                    if (status != VF2_OK) {
                        return status;
                    }
                    ++r4;
                    instructions += UINT64_C(4);
                    if (bit_set) {
                        instructions += UINT64_C(3);
                        g3 = g4;
                        if ((int32_t)g4 < 0) {
                            break;
                        }
                    } else {
                        instructions += UINT64_C(3);
                        g3 += UINT32_C(4);
                        if ((int32_t)g4 < 0) {
                            ++instructions;
                            break;
                        }
                    }
                    if (r4 > UINT32_C(31)) {
                        return VF2_ERROR_UNSUPPORTED;
                    }
                }
                g0 = g4 >> 28u;
                --r4;
                status = vf2_model2a_read_u32(
                    machine,
                    VF2_TEXTURE_TREE_TABLE + g0 * UINT32_C(4),
                    &handler
                );
                if (status != VF2_OK) {
                    return status;
                }
                g3 = handler;
                r14 -= r4;
                r13 >>= (r4 & UINT32_C(31));
                instructions += UINT64_C(7);
                if (r14 <= UINT32_C(16)) {
                    uint16_t next_bits = 0u;
                    g0 = g11 << (r14 & UINT32_C(31));
                    status = read_u16(machine, r15, &next_bits);
                    if (status != VF2_OK) {
                        return status;
                    }
                    g11 = next_bits;
                    r15 += UINT32_C(2);
                    r13 |= g0;
                    r14 += UINT32_C(16);
                    instructions += UINT64_C(5);
                }
                ++instructions;
            } else {
                g2 &= UINT32_C(15);
                r14 -= g2;
                r13 >>= (g2 & UINT32_C(31));
                instructions += UINT64_C(5);
                if (r14 <= UINT32_C(16)) {
                    uint16_t next_bits = 0u;
                    g0 = g11 << (r14 & UINT32_C(31));
                    status = read_u16(machine, r15, &next_bits);
                    if (status != VF2_OK) {
                        return status;
                    }
                    g11 = next_bits;
                    r15 += UINT32_C(2);
                    r13 |= g0;
                    r14 += UINT32_C(16);
                    instructions += UINT64_C(5);
                }
                handler = g5;
                ++instructions;
            }

            if (handler == UINT32_C(0x0004c798)) {
                uint32_t table_value = 0u;
                g4 = r13 & r12;
                r14 -= r6;
                r13 >>= (r6 & UINT32_C(31));
                instructions += UINT64_C(5);
                if (r14 <= UINT32_C(16)) {
                    uint16_t next_bits = 0u;
                    g0 = g11 << (r14 & UINT32_C(31));
                    status = read_u16(machine, r15, &next_bits);
                    if (status != VF2_OK) {
                        return status;
                    }
                    g11 = next_bits;
                    r15 += UINT32_C(2);
                    r13 |= g0;
                    r14 += UINT32_C(16);
                    instructions += UINT64_C(5);
                }
                status = vf2_model2a_read_u32(
                    machine,
                    UINT32_C(0x0055cd50) + g4 * UINT32_C(4),
                    &table_value
                );
                if (status != VF2_OK) {
                    return status;
                }
                g4 = table_value;
                ++instructions;
                handler = UINT32_C(0x0004c7c8);
            }

            if (handler == UINT32_C(0x0004c7c8)) {
                uint16_t palette = 0u;
                r8 = (r8 + g4) & UINT32_C(15);
                status = read_u16(
                    machine,
                    UINT32_C(0x0004adb0) + r8 * UINT32_C(2),
                    &palette
                );
                if (status != VF2_OK) {
                    return status;
                }
                r7 = palette;
                g4 >>= 8u;
                instructions += UINT64_C(4);
                handler = UINT32_C(0x0004c7dc);
            }

            if (handler == UINT32_C(0x0004c7dc)) {
                uint8_t byte_value = (uint8_t)r8;
                status = vf2_model2a_write(
                    machine, r10, &byte_value, sizeof(byte_value)
                );
                if (status == VF2_OK) {
                    status = write_u16(machine, r11, (uint16_t)(g4 + r7));
                }
                if (status != VF2_OK) {
                    return status;
                }
                r10 -= UINT32_C(2);
                g4 += r7;
                r11 += UINT32_C(2);
                g0 = (r13 << 24u) >> 24u;
                status = vf2_model2a_read_u32(
                    machine, r5 + g0 * UINT32_C(8), &g4
                );
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(
                        machine, r5 + g0 * UINT32_C(8) + UINT32_C(4), &g5
                    );
                }
                if (status != VF2_OK) {
                    return status;
                }
                --g13;
                ++outputs;
                instructions += UINT64_C(10);
                continue;
            }

            if (handler == UINT32_C(0x0004c854)) {
                uint8_t run_value = (uint8_t)r8;
                uint32_t run_length = (g4 << 24u) >> 24u;
                uint32_t run_index = 0u;
                status = write_u16(machine, r11, (uint16_t)cpu->registers[26]);
                if (status != VF2_OK || run_length == 0u || run_length > g13) {
                    return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
                }
                r11 += UINT32_C(2);
                g1 = run_length;
                g13 -= run_length;
                instructions += UINT64_C(5);
                for (run_index = 0u; run_index < run_length; ++run_index) {
                    status = vf2_model2a_write(
                        machine, r10, &run_value, sizeof(run_value)
                    );
                    if (status != VF2_OK) {
                        return status;
                    }
                    r10 -= UINT32_C(2);
                    instructions += UINT64_C(4);
                }
                g1 = 0u;
                g2 = (r7 << 8u) | g4;
                status = write_u16(machine, r11, (uint16_t)g2);
                if (status != VF2_OK) {
                    return status;
                }
                r11 += UINT32_C(2);
                g0 = (r13 << 24u) >> 24u;
                status = vf2_model2a_read_u32(
                    machine, r5 + g0 * UINT32_C(8), &g4
                );
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(
                        machine, r5 + g0 * UINT32_C(8) + UINT32_C(4), &g5
                    );
                }
                if (status != VF2_OK) {
                    return status;
                }
                instructions += UINT64_C(10);
                outputs += run_length;
                ++encoded_runs;
                continue;
            }

            if (handler == UINT32_C(0x0004c89c)) {
                uint16_t next_bits = 0u;
                uint16_t palette = 0u;
                g2 = (r13 << 16u) >> 16u;
                r14 -= UINT32_C(16);
                r13 >>= 16u;
                instructions += UINT64_C(6);
                if (r14 <= UINT32_C(16)) {
                    g0 = g11 << (r14 & UINT32_C(31));
                    status = read_u16(machine, r15, &next_bits);
                    if (status != VF2_OK) {
                        return status;
                    }
                    g11 = next_bits;
                    r15 += UINT32_C(2);
                    r13 |= g0;
                    r14 += UINT32_C(16);
                    instructions += UINT64_C(5);
                }
                g1 = r13 & UINT32_C(15);
                r14 -= UINT32_C(4);
                r13 >>= 4u;
                instructions += UINT64_C(5);
                if (r14 <= UINT32_C(16)) {
                    g0 = g11 << (r14 & UINT32_C(31));
                    status = read_u16(machine, r15, &next_bits);
                    if (status != VF2_OK) {
                        return status;
                    }
                    g11 = next_bits;
                    r15 += UINT32_C(2);
                    r13 |= g0;
                    r14 += UINT32_C(16);
                    instructions += UINT64_C(5);
                }
                g0 = (r13 << 24u) >> 24u;
                status = vf2_model2a_read_u32(
                    machine, r5 + g0 * UINT32_C(8), &g4
                );
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(
                        machine, r5 + g0 * UINT32_C(8) + UINT32_C(4), &g5
                    );
                }
                r8 = (r8 + g1) & UINT32_C(15);
                if (status == VF2_OK) {
                    status = read_u16(
                        machine,
                        UINT32_C(0x0004adb0) + r8 * UINT32_C(2),
                        &palette
                    );
                }
                if (status != VF2_OK) {
                    return status;
                }
                r7 = palette;
                --g13;
                {
                    const uint8_t byte_value = (uint8_t)r8;
                    status = vf2_model2a_write(
                        machine, r10, &byte_value, sizeof(byte_value)
                    );
                }
                g2 += r7;
                if (status == VF2_OK) {
                    status = write_u16(machine, r11, (uint16_t)g2);
                }
                if (status != VF2_OK) {
                    return status;
                }
                r10 -= UINT32_C(2);
                r11 += UINT32_C(2);
                ++outputs;
                instructions += UINT64_C(14);
                continue;
            }

            return VF2_ERROR_UNSUPPORTED;
        }

        g0 = r9;
        r9 = r10;
        r10 = g0;
        --g14;
        ++outer_iterations;
        instructions += UINT64_C(5);
    }
    ++instructions;

    cpu->registers[16] = g0;
    cpu->registers[17] = g1;
    cpu->registers[18] = g2;
    cpu->registers[19] = g3;
    cpu->registers[20] = g4;
    cpu->registers[21] = g5;
    cpu->registers[27] = g11;
    cpu->registers[28] = g12;
    cpu->registers[29] = g13;
    cpu->registers[30] = g14;
    set_equal_condition(cpu);
    cpu->executed_instructions += instructions;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_BYTE_DECODE;
    report->entry_address = VF2_TEXTURE_BYTE_DECODE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = outputs;
    report->rows = outer_iterations;
    report->changed_values = encoded_runs;
    report->bytes_written = (size_t)outputs * 3u;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}


static vf2_status execute_texture_symbol_table_build(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t r3 = cpu->registers[3];
    const uint32_t r4 = cpu->registers[4];
    const uint32_t r5 = cpu->registers[5];
    uint32_t r7 = cpu->registers[7];
    const uint32_t r12 = cpu->registers[12];
    uint32_t r13 = cpu->registers[13];
    uint32_t r14 = cpu->registers[14];
    uint32_t r15 = cpu->registers[15];
    uint32_t g0 = cpu->registers[16];
    uint32_t g2 = cpu->registers[18];
    const uint32_t g3 = cpu->registers[19];
    const uint32_t g4 = cpu->registers[20];
    const uint32_t g5 = cpu->registers[21];
    const uint32_t g6 = cpu->registers[22];
    uint32_t g11 = cpu->registers[27];
    const uint32_t input_count = r7;
    uint64_t instructions = 0u;
    vf2_status status = VF2_OK;

    if (r5 == 0u || r5 > UINT32_C(31) || r7 == 0u ||
        r7 > VF2_TEXTURE_MAX_LOOP || r12 == 0u ||
        g3 != UINT32_C(0x00000100) ||
        g4 != UINT32_C(0x00000102) ||
        g5 != UINT32_C(0x00000122) ||
        g6 != UINT32_C(0x00000142)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    do {
        g2 = r13 & r12;
        r14 -= r5;
        r13 >>= (r5 & UINT32_C(31));
        instructions += UINT64_C(5);
        if (r14 <= UINT32_C(16)) {
            uint16_t next_bits = 0u;
            g0 = g11 << (r14 & UINT32_C(31));
            status = read_u16(machine, r15, &next_bits);
            if (status != VF2_OK) {
                return status;
            }
            g11 = next_bits;
            r15 += UINT32_C(2);
            r13 |= g0;
            r14 += UINT32_C(16);
            instructions += UINT64_C(5);
        }

        ++instructions;
        if (g2 >= g6) {
            g2 = r4 + ((g2 - g6) << 2u);
            instructions += UINT64_C(4);
        } else {
            ++instructions;
            if (g2 < g4) {
                ++instructions;
                if (g2 < g3) {
                    uint32_t table_value = 0u;
                    status = vf2_model2a_read_u32(
                        machine,
                        UINT32_C(0x02300010) + g2 * UINT32_C(4),
                        &table_value
                    );
                    if (status != VF2_OK) {
                        return status;
                    }
                    g2 = table_value;
                    instructions += UINT64_C(3);
                    if ((g2 & UINT32_C(15)) != 0u) {
                        g0 = UINT32_C(1) << 31u;
                        g2 |= g0;
                        instructions += UINT64_C(3);
                    } else {
                        g2 >>= 8u;
                        g0 = UINT32_C(3) << 30u;
                        g2 |= g0;
                        instructions += UINT64_C(4);
                    }
                } else {
                    ++instructions;
                    if (g2 == g3) {
                        g2 = UINT32_C(9) << 28u;
                    } else {
                        g2 = UINT32_C(5) << 29u;
                    }
                    instructions += UINT64_C(2);
                }
            } else {
                ++instructions;
                if (g2 < g5) {
                    g2 = g2 - g4 + UINT32_C(1);
                    instructions += UINT64_C(3);
                    if (g2 >= UINT32_C(17)) {
                        g2 = (g2 - UINT32_C(16)) << 4u;
                        instructions += UINT64_C(2);
                    }
                    g0 = UINT32_C(11) << 28u;
                    g2 |= g0;
                    instructions += UINT64_C(3);
                } else {
                    g2 = g2 - g5 + UINT32_C(1);
                    instructions += UINT64_C(3);
                    if (g2 >= UINT32_C(17)) {
                        g2 = (g2 - UINT32_C(16)) << 4u;
                        instructions += UINT64_C(2);
                    }
                    g0 = UINT32_C(13) << 28u;
                    g2 |= g0;
                    instructions += UINT64_C(3);
                }
            }
        }

        status = vf2_model2a_write_u32(machine, r3, g2);
        if (status != VF2_OK) {
            return status;
        }
        r3 += UINT32_C(4);
        --r7;
        instructions += UINT64_C(4);
    } while (r7 != 0u);

    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0055c338), &r7
    );
    if (status != VF2_OK) {
        return status;
    }
    r3 = UINT32_C(0x0055cd50);
    instructions += UINT64_C(2);

    cpu->registers[3] = r3;
    cpu->registers[7] = r7;
    cpu->registers[13] = r13;
    cpu->registers[14] = r14;
    cpu->registers[15] = r15;
    cpu->registers[16] = g0;
    cpu->registers[18] = g2;
    cpu->registers[27] = g11;
    cpu->ip = UINT32_C(0x0004c4d4);
    cpu->executed_instructions += instructions;
    set_equal_condition(cpu);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_SYMBOL_TABLE_BUILD;
    report->entry_address = VF2_TEXTURE_SYMBOL_TABLE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = input_count;
    report->bytes_written = (size_t)input_count * 4u;
    report->recovered_instruction_count = instructions;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_texture_pair_table_build(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t r3 = cpu->registers[3];
    uint32_t r7 = cpu->registers[7];
    uint32_t r13 = cpu->registers[13];
    uint32_t r14 = cpu->registers[14];
    uint32_t r15 = cpu->registers[15];
    uint32_t g0 = cpu->registers[16];
    uint32_t g2 = cpu->registers[18];
    uint32_t g3 = cpu->registers[19];
    uint32_t g11 = cpu->registers[27];
    const uint32_t input_count = r7;
    uint64_t instructions = 0u;
    vf2_status status = VF2_OK;

    ++instructions;
    if (r7 != 0u) {
        do {
            g2 = (r13 << 16u) >> 16u;
            r14 -= UINT32_C(16);
            r13 >>= 16u;
            instructions += UINT64_C(6);
            if (r14 <= UINT32_C(16)) {
                uint16_t next_bits = 0u;
                g0 = g11 << (r14 & UINT32_C(31));
                status = read_u16(machine, r15, &next_bits);
                if (status != VF2_OK) {
                    return status;
                }
                g11 = next_bits;
                r15 += UINT32_C(2);
                r13 |= g0;
                r14 += UINT32_C(16);
                instructions += UINT64_C(5);
            }

            g3 = r13 & UINT32_C(15);
            r14 -= UINT32_C(4);
            r13 >>= 4u;
            instructions += UINT64_C(5);
            if (r14 <= UINT32_C(16)) {
                uint16_t next_bits = 0u;
                g0 = g11 << (r14 & UINT32_C(31));
                status = read_u16(machine, r15, &next_bits);
                if (status != VF2_OK) {
                    return status;
                }
                g11 = next_bits;
                r15 += UINT32_C(2);
                r13 |= g0;
                r14 += UINT32_C(16);
                instructions += UINT64_C(5);
            }

            g2 = (g2 << 8u) | g3;
            status = vf2_model2a_write_u32(machine, r3, g2);
            if (status != VF2_OK) {
                return status;
            }
            r3 += UINT32_C(4);
            --r7;
            instructions += UINT64_C(6);
        } while (r7 != 0u);
    }

    cpu->registers[3] = r3;
    cpu->registers[7] = r7;
    cpu->registers[13] = r13;
    cpu->registers[14] = r14;
    cpu->registers[15] = r15;
    cpu->registers[16] = g0;
    cpu->registers[18] = g2;
    cpu->registers[19] = g3;
    cpu->registers[27] = g11;
    cpu->ip = UINT32_C(0x0004c544);
    cpu->executed_instructions += instructions;
    set_equal_condition(cpu);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_PAIR_TABLE_BUILD;
    report->entry_address = VF2_TEXTURE_PAIR_TABLE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = input_count;
    report->bytes_written = (size_t)input_count * 4u;
    report->recovered_instruction_count = instructions;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_texture_word_run(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint32_t count = cpu->registers[19];
    const uint16_t value = (uint16_t)cpu->registers[18];
    uint32_t address = cpu->registers[10];
    uint32_t index = 0u;
    vf2_status status = VF2_OK;

    if (count == 0u || count > VF2_TEXTURE_MAX_LOOP) {
        return VF2_ERROR_UNSUPPORTED;
    }
    for (index = 0u; index < count; ++index) {
        status = write_u16(machine, address, value);
        if (status != VF2_OK) {
            return status;
        }
        address += UINT32_C(4);
    }
    cpu->registers[10] = address;
    cpu->registers[19] = 0u;
    cpu->ip = VF2_TEXTURE_WORD_RUN_EXIT;
    cpu->executed_instructions += (uint64_t)count * UINT64_C(4);
    set_equal_condition(cpu);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_WORD_RUN;
    report->entry_address = VF2_TEXTURE_WORD_RUN_ENTRY;
    report->exit_address = VF2_TEXTURE_WORD_RUN_EXIT;
    report->iterations = count;
    report->bytes_written = (size_t)count * 2u;
    report->recovered_instruction_count =
        (uint64_t)count * UINT64_C(4);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_texture_word_decode(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint8_t frame_state = 0u;
    uint8_t frame_wait = 0u;
    uint16_t width_value = 0u;
    uint16_t height_value = 0u;
    uint16_t current_value = 0u;
    uint32_t source = cpu->registers[9];
    uint32_t row_destination = cpu->registers[11];
    uint32_t rows = 0u;
    uint64_t instructions = 0u;
    uint64_t output_values = 0u;
    uint64_t encoded_runs = 0u;
    uint16_t last_encoded = (uint16_t)cpu->registers[17];
    uint16_t last_high_byte = (uint16_t)cpu->registers[20];
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read(
        machine, VF2_FRAME_STATE, &frame_state, sizeof(frame_state)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_FRAME_WAIT, &frame_wait, sizeof(frame_wait)
        );
    }
    if (status == VF2_OK) {
        status = read_u16(machine, UINT32_C(0x0055c320), &width_value);
    }
    if (status == VF2_OK) {
        status = read_u16(machine, UINT32_C(0x0055c322), &height_value);
    }
    if (status != VF2_OK) {
        return status;
    }
    if (frame_state != 0u || frame_wait != 0u || width_value == 0u ||
        height_value == 0u || width_value > UINT16_C(2048) ||
        height_value > UINT16_C(2048)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = read_u16(machine, source, &current_value);
    if (status != VF2_OK) {
        return status;
    }
    source += UINT32_C(2);
    instructions += UINT64_C(3);
    rows = width_value;

    while (rows != 0u) {
        uint32_t destination = row_destination;
        int32_t remaining = (int32_t)height_value;

        instructions += UINT64_C(7);
        row_destination += UINT32_C(0x800);
        while (remaining > 0) {
            instructions += UINT64_C(3);
            --remaining;
            if (current_value == (uint16_t)cpu->registers[8]) {
                uint16_t encoded = 0u;
                uint32_t run_length = 0u;
                uint16_t repeated = 0u;

                status = read_u16(machine, source, &encoded);
                if (status != VF2_OK) {
                    return status;
                }
                source += UINT32_C(2);
                ++remaining;
                repeated = (uint16_t)(((uint16_t)(encoded >> 8u)) |
                                      ((uint16_t)(encoded >> 8u) << 8u));
                last_encoded = encoded;
                last_high_byte = (uint16_t)(encoded & UINT16_C(0xff00));
                run_length = (uint32_t)((uint16_t)(encoded ^
                    ((uint16_t)(encoded >> 8u) << 8u)));
                if (run_length == 0u || run_length > (uint32_t)remaining) {
                    return VF2_ERROR_UNSUPPORTED;
                }
                remaining -= (int32_t)run_length;
                instructions += UINT64_C(8);
                while (run_length != 0u) {
                    status = write_u16(machine, destination, repeated);
                    if (status != VF2_OK) {
                        return status;
                    }
                    destination += UINT32_C(4);
                    --run_length;
                    ++output_values;
                    instructions += UINT64_C(4);
                }
                ++encoded_runs;
                status = read_u16(machine, source, &current_value);
                if (status != VF2_OK) {
                    return status;
                }
                source += UINT32_C(2);
                instructions += UINT64_C(4);
            } else {
                status = write_u16(machine, destination, current_value);
                if (status != VF2_OK) {
                    return status;
                }
                destination += UINT32_C(4);
                status = read_u16(machine, source, &current_value);
                if (status != VF2_OK) {
                    return status;
                }
                source += UINT32_C(2);
                ++output_values;
                instructions += UINT64_C(6);
            }
        }
        --rows;
        instructions += UINT64_C(2);
    }
    instructions += UINT64_C(1);

    cpu->registers[16] = 0u;
    cpu->registers[17] = last_encoded;
    cpu->registers[18] = current_value;
    cpu->registers[19] = 0u;
    cpu->registers[20] = last_high_byte;
    set_equal_condition(cpu);
    cpu->executed_instructions += instructions;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_WORD_DECODE;
    report->entry_address = VF2_TEXTURE_WORD_DECODE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = output_values;
    report->rows = width_value;
    report->changed_values = encoded_runs;
    report->bytes_written = (size_t)output_values * 2u;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_texture_tree(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    texture_tree_stats stats;
    const uint32_t entry_depth = cpu->local_frame_depth;
    vf2_status status = VF2_OK;

    if (entry_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(&stats, 0, sizeof(stats));
    status = texture_tree_expand_recursive(
        machine,
        cpu,
        cpu->registers[30],
        cpu->registers[29],
        cpu->registers[28],
        0u,
        &stats
    );
    if (status != VF2_OK) {
        return status;
    }
    if (entry_depth + stats.max_nested_depth >
        VF2_I960_MAX_LOCAL_FRAMES) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    if (cpu->maximum_local_frame_depth <
        entry_depth + stats.max_nested_depth) {
        cpu->maximum_local_frame_depth =
            entry_depth + stats.max_nested_depth;
    }
    cpu->executed_instructions += stats.instructions;
    cpu->procedure_calls += stats.nested_calls;
    cpu->procedure_returns += stats.nested_calls;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_TREE_EXPAND;
    report->entry_address = VF2_TEXTURE_TREE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = stats.writes;
    report->bytes_written = stats.bytes_written;
    report->max_recursion_depth = stats.max_nested_depth;
    report->recovered_instruction_count = stats.instructions;
    report->recovered_procedure_calls = stats.nested_calls;
    report->recovered_procedure_returns = stats.nested_calls + UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_texture_tree_dispatch(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report tree_report;
    const uint32_t stack_start = cpu->registers[1];
    const uint32_t saved_registers[] = {
        3u,
        4u, 5u, 6u, 7u,
        8u, 9u, 10u, 11u,
        12u, 13u, 14u, 15u,
        16u, 17u, 18u, 19u,
        20u, 21u, 22u, 23u,
        24u, 25u, 26u, 27u,
        28u, 29u, 30u
    };
    uint32_t index = 0u;
    uint32_t flags = 0u;
    uint32_t table_index = 0u;
    uint32_t value = 0u;
    uint16_t width = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u ||
        stack_start > UINT32_MAX - UINT32_C(112)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(&tree_report, 0, sizeof(tree_report));
    for (index = 0u;
         index < sizeof(saved_registers) / sizeof(saved_registers[0]);
         ++index) {
        status = vf2_model2a_write_u32(
            machine,
            stack_start + index * UINT32_C(4),
            cpu->registers[saved_registers[index]]
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    cpu->registers[1] = stack_start + UINT32_C(112);
    cpu->registers[26] = UINT32_C(0x00545000);
    cpu->registers[25] = UINT32_C(256);
    cpu->registers[30] = 0u;
    cpu->registers[29] = UINT32_C(0x0055c344);
    cpu->registers[28] = 0u;

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_TEXTURE_TREE_ENTRY, VF2_TEXTURE_TREE_RETURN
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions += UINT64_C(26);
    status = execute_texture_tree(machine, cpu, &tree_report);
    if (status != VF2_OK) {
        return status;
    }
    if (cpu->ip != VF2_TEXTURE_TREE_RETURN ||
        cpu->registers[1] != stack_start + UINT32_C(112)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    for (index = 0u;
         index < sizeof(saved_registers) / sizeof(saved_registers[0]);
         ++index) {
        status = vf2_model2a_read_u32(
            machine,
            stack_start + index * UINT32_C(4),
            &value
        );
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[saved_registers[index]] = value;
    }
    cpu->registers[1] = stack_start;
    status = vf2_model2a_read_u32(
        machine, VF2_TEXTURE_ACTIVE_FLAGS, &flags
    );
    if (status != VF2_OK || (flags & (UINT32_C(1) << 1u)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[16] = flags;
    cpu->registers[11] = UINT32_C(0x005502f0);
    cpu->registers[8] = 0u;
    cpu->registers[7] = 0u;
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0055c33c), &cpu->registers[6]
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0055c340), &cpu->registers[26]
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[5] = UINT32_C(0x00545000);
    cpu->registers[10] = UINT32_C(0x0055c2ef);
    cpu->registers[9] = UINT32_C(0x0055c2ee);
    cpu->registers[12] = cpu->registers[6] >= UINT32_C(32)
        ? UINT32_MAX
        : (UINT32_C(1) << cpu->registers[6]) - UINT32_C(1);
    table_index = cpu->registers[13] & UINT32_C(0xff);
    cpu->registers[16] = table_index;
    status = vf2_model2a_read_u32(
        machine,
        cpu->registers[5] + table_index * UINT32_C(8),
        &cpu->registers[20]
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            cpu->registers[5] + table_index * UINT32_C(8) + UINT32_C(4),
            &cpu->registers[21]
        );
    }
    if (status == VF2_OK) {
        status = read_u16(machine, UINT32_C(0x0055c320), &width);
    }
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[30] = width;
    cpu->ip = VF2_TEXTURE_TREE_DISPATCH_EXIT;
    cpu->executed_instructions += UINT64_C(37);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_TREE_DISPATCH;
    report->entry_address = VF2_TEXTURE_TREE_DISPATCH_ENTRY;
    report->exit_address = VF2_TEXTURE_TREE_DISPATCH_EXIT;
    report->iterations = tree_report.iterations;
    report->changed_values = tree_report.changed_values;
    report->bytes_written = 112u + tree_report.bytes_written;
    report->max_recursion_depth = tree_report.max_recursion_depth;
    report->recovered_instruction_count =
        UINT64_C(63) + tree_report.recovered_instruction_count;
    report->recovered_procedure_calls =
        UINT64_C(1) + tree_report.recovered_procedure_calls;
    report->recovered_procedure_returns =
        tree_report.recovered_procedure_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}


static vf2_status execute_texture_convert(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t saved_state = 0u;
    uint8_t frame_state = 0u;
    uint8_t frame_wait = 0u;
    uint32_t source = VF2_TEXTURE_CONVERT_SOURCE;
    uint32_t odd_address = VF2_TEXTURE_CONVERT_ODD;
    uint32_t even_address = VF2_TEXTURE_CONVERT_EVEN;
    uint32_t current = 0u;
    uint32_t previous = UINT32_MAX;
    uint32_t rows = cpu->registers[28];
    const uint32_t input_rows = rows;
    const uint32_t columns = cpu->registers[29];
    uint32_t repeat_value = 0u;
    uint32_t nibble_value = 0u;
    uint64_t instructions = UINT64_C(8);
    uint64_t changed_pixels = 0u;
    uint64_t pixels = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u || rows == 0u || columns == 0u ||
        rows > UINT32_C(1024) || columns > UINT32_C(1024)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, VF2_TEXTURE_CONVERT_STATE, &saved_state);
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_FRAME_STATE, &frame_state, sizeof(frame_state)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_FRAME_WAIT, &frame_wait, sizeof(frame_wait)
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if (saved_state != 0u || frame_state != 0u || frame_wait != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    cpu->registers[24] = UINT32_MAX;
    status = vf2_model2a_read_u32(machine, source, &current);
    if (status != VF2_OK) {
        return status;
    }

    while (rows != 0u) {
        uint32_t destination = cpu->registers[27];
        uint32_t remaining = columns;
        instructions += UINT64_C(12);
        cpu->registers[16] = 0u;
        cpu->registers[27] += cpu->registers[26];

        while (remaining != 0u) {
            source -= UINT32_C(4);
            instructions += UINT64_C(3);
            if (current == previous) {
                status = write_u16(
                    machine, destination, (uint16_t)repeat_value
                );
                if (status == VF2_OK) {
                    const uint8_t nibble = (uint8_t)nibble_value;
                    status = vf2_model2a_write(
                        machine, odd_address, &nibble, sizeof(nibble)
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(machine, source, &current);
                }
                instructions += UINT64_C(7);
            } else {
                uint32_t mixed = 0u;
                uint8_t nibble = 0u;
                repeat_value = current | (current >> 12u);
                status = write_u16(
                    machine, destination, (uint16_t)repeat_value
                );
                previous = current;
                mixed = current + (current >> 16u);
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(machine, source, &current);
                }
                cpu->registers[18] = mixed + (mixed >> 8u);
                cpu->registers[18] >>= 2u;
                nibble_value = cpu->registers[18] & UINT32_C(15);
                cpu->registers[19] = mixed >> 8u;
                nibble = (uint8_t)nibble_value;
                if (status == VF2_OK) {
                    status = vf2_model2a_write(
                        machine, odd_address, &nibble, sizeof(nibble)
                    );
                }
                instructions += UINT64_C(16);
                ++changed_pixels;
            }
            if (status != VF2_OK) {
                return status;
            }
            destination += UINT32_C(4);
            odd_address -= UINT32_C(2);
            --remaining;
            ++pixels;
        }
        {
            const uint32_t swap = odd_address;
            cpu->registers[16] = swap;
            odd_address = even_address;
            even_address = swap;
        }
        --rows;
    }

    instructions += UINT64_C(1);
    cpu->registers[24] = previous;
    cpu->executed_instructions += instructions;
    set_equal_condition(cpu);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_COLOR_CONVERT;
    report->entry_address = VF2_TEXTURE_CONVERT_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = pixels;
    report->rows = input_rows;
    report->changed_values = changed_pixels;
    report->bytes_written = (size_t)pixels * 3u;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_texture_header_decode(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    texture_bit_reader reader;
    uint32_t raw_width = 0u;
    uint32_t raw_height = 0u;
    uint32_t width = 0u;
    uint32_t height = 0u;
    uint32_t area = 0u;
    uint32_t code_bits = 0u;
    uint32_t symbol_count = 0u;
    uint32_t table_a = 0u;
    uint32_t table_b = 0u;
    uint32_t nibble = 0u;
    uint32_t table_c = 0u;
    uint32_t child_state = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u || cpu->registers[19] == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(
        machine, VF2_TEXTURE_HEADER_STATE, &child_state
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[VF2_I960_G0_REGISTER] = child_state;
    if (child_state != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = texture_bit_reader_initialize(
        machine, cpu->registers[19], &reader
    );
    if (status == VF2_OK) {
        status = texture_bit_reader_refill(machine, &reader);
    }
    if (status == VF2_OK) {
        status = texture_bit_reader_refill(machine, &reader);
    }
    if (status == VF2_OK) {
        status = texture_bit_reader_take(machine, &reader, 8u, &raw_width);
    }
    if (status == VF2_OK) {
        status = texture_bit_reader_take(machine, &reader, 8u, &raw_height);
    }
    if (status == VF2_OK) {
        status = texture_bit_reader_take(machine, &reader, 8u, &code_bits);
    }
    if (status == VF2_OK) {
        status = texture_bit_reader_take(machine, &reader, 16u, &symbol_count);
    }
    if (status == VF2_OK) {
        status = texture_bit_reader_take(machine, &reader, 16u, &table_a);
    }
    if (status == VF2_OK) {
        status = texture_bit_reader_take(machine, &reader, 16u, &table_b);
    }
    if (status == VF2_OK) {
        status = texture_bit_reader_take(machine, &reader, 4u, &nibble);
    }
    if (status == VF2_OK) {
        status = texture_bit_reader_take(machine, &reader, 16u, &table_c);
    }
    if (status != VF2_OK) {
        return status;
    }

    width = (raw_width + UINT32_C(1)) >> 1u;
    height = (raw_height + UINT32_C(1)) >> 1u;
    area = width * height;
    if (code_bits == 0u || code_bits > UINT32_C(31) ||
        symbol_count == 0u || symbol_count > VF2_TEXTURE_MAX_LOOP) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = write_u16(
        machine, VF2_TEXTURE_HEADER_OUTPUT, (uint16_t)width
    );
    if (status == VF2_OK) {
        status = write_u16(
            machine, VF2_TEXTURE_HEADER_OUTPUT + UINT32_C(2),
            (uint16_t)height
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_TEXTURE_HEADER_OUTPUT + UINT32_C(4), area
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_TEXTURE_HEADER_OUTPUT + UINT32_C(8),
            (symbol_count << 1u) - UINT32_C(1)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_TEXTURE_HEADER_OUTPUT + UINT32_C(12), symbol_count
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_TEXTURE_HEADER_OUTPUT + UINT32_C(16), code_bits
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_TEXTURE_HEADER_OUTPUT + UINT32_C(20), table_a
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_TEXTURE_HEADER_OUTPUT + UINT32_C(24), table_b
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_TEXTURE_HEADER_OUTPUT + UINT32_C(28), nibble
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_TEXTURE_HEADER_OUTPUT + UINT32_C(32), table_c
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[3] = UINT32_C(0x0055c344);
    cpu->registers[4] = UINT32_C(0x0055c344);
    cpu->registers[5] = code_bits;
    cpu->registers[7] = (symbol_count << 1u) - UINT32_C(1);
    cpu->registers[12] =
        (UINT32_C(1) << code_bits) - UINT32_C(1);
    cpu->registers[13] = reader.accumulator;
    cpu->registers[14] = reader.available_bits;
    cpu->registers[15] = reader.next_address;
    cpu->registers[16] = reader.last_shifted_word;
    cpu->registers[18] = table_c;
    cpu->registers[19] = UINT32_C(0x00000100);
    cpu->registers[20] = UINT32_C(0x00000102);
    cpu->registers[21] = UINT32_C(0x00000122);
    cpu->registers[22] = UINT32_C(0x00000142);
    cpu->registers[27] = reader.next_word;
    set_signed_condition(cpu, 0, 1);
    cpu->ip = VF2_TEXTURE_HEADER_DECODE_EXIT;
    cpu->executed_instructions += UINT64_C(120);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_HEADER_DECODE;
    report->entry_address = VF2_TEXTURE_HEADER_DECODE_ENTRY;
    report->exit_address = VF2_TEXTURE_HEADER_DECODE_EXIT;
    report->iterations = UINT64_C(8);
    report->changed_values = UINT64_C(10);
    report->bytes_written = 36u;
    report->recovered_instruction_count = UINT64_C(120);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}


static vf2_status execute_texture_active_prepare_call(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint16_t raw_count = 0u;
    uint16_t raw_x = 0u;
    uint16_t raw_y = 0u;
    uint32_t flags = 0u;
    uint32_t stream_word = 0u;
    uint32_t table_index = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(
        machine, cpu->registers[5] + UINT32_C(0x10), &flags
    );
    if (status == VF2_OK) {
        status = read_u16(
            machine, cpu->registers[5] + UINT32_C(2), &raw_count
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, cpu->registers[5] + UINT32_C(0x14), &cpu->registers[9]
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, cpu->registers[5] + UINT32_C(0x18), &cpu->registers[10]
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[7] = flags;
    cpu->registers[8] = (uint32_t)(int32_t)(int16_t)raw_count;
    if ((flags & (UINT32_C(1) << 3u)) != 0u ||
        (flags & (UINT32_C(1) << 4u)) != 0u ||
        (int32_t)cpu->registers[8] <= 0) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_write_u32(machine, VF2_TEXTURE_ACTIVE_FLAGS, flags);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, cpu->registers[9], &stream_word);
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[6] = 1u;
    cpu->registers[18] = stream_word;
    cpu->registers[24] = (stream_word ^ flags) & UINT32_C(1);
    table_index = (stream_word & UINT32_C(0xffff)) >> 1u;
    cpu->registers[16] = table_index;
    status = read_u16(
        machine,
        VF2_TEXTURE_COORD_TABLE + table_index * UINT32_C(4),
        &raw_x
    );
    if (status == VF2_OK) {
        status = read_u16(
            machine,
            VF2_TEXTURE_COORD_TABLE + table_index * UINT32_C(4) + UINT32_C(2),
            &raw_y
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[22] = (uint32_t)(int32_t)(int16_t)raw_x;
    cpu->registers[23] = (uint32_t)(int32_t)(int16_t)raw_y;
    cpu->registers[16] = stream_word >> 24u;
    cpu->registers[17] = (stream_word >> 16u) & UINT32_C(0xff);
    cpu->registers[22] += cpu->registers[16];
    cpu->registers[23] += cpu->registers[17];

    status = vf2_i960_cpu_enter_procedure(
        cpu,
        VF2_TEXTURE_ACTIVE_PREPARE_TARGET,
        VF2_TEXTURE_ACTIVE_PREPARE_RETURN
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions += UINT64_C(22);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_ACTIVE_PREPARE_CALL;
    report->entry_address = VF2_TEXTURE_ACTIVE_PREPARE_ENTRY;
    report->exit_address = VF2_TEXTURE_ACTIVE_PREPARE_TARGET;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(1);
    report->bytes_written = 4u;
    report->recovered_instruction_count = UINT64_C(22);
    report->recovered_procedure_calls = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}


static vf2_status execute_texture_status_dispatch_call(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint16_t active_count = 0u;
    uint16_t status_value = 0u;
    uint32_t runtime_flags = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    cpu->registers[5] = VF2_TEXTURE_RECORD_START;
    cpu->registers[6] = VF2_TEXTURE_RECORD_END;
    status = read_u16(
        machine,
        VF2_TEXTURE_RECORD_START + UINT32_C(2),
        &active_count
    );
    if (status == VF2_OK) {
        cpu->registers[3] =
            (uint32_t)(int32_t)(int16_t)active_count;
        if (cpu->registers[3] == 0u) {
            vf2_orchestrator_scan_report scan_report;
            memset(&scan_report, 0, sizeof(scan_report));
            status = vf2_orchestrator_scan_inactive_records(
                machine, cpu, &scan_report
            );
            if (status != VF2_OK) {
                return status;
            }
            report->kind =
                VF2_HYBRID_BRIDGE_TEXTURE_STATUS_SCAN_END;
            report->entry_address = scan_report.entry_address;
            report->exit_address = scan_report.exit_address;
            report->iterations =
                (uint64_t)scan_report.records_scanned;
            report->recovered_instruction_count =
                scan_report.recovered_instruction_count;
            report->cpu_poststate_applied =
                scan_report.cpu_poststate_applied;
            return VF2_OK;
        }
        status = read_u16(
            machine, VF2_TEXTURE_RECORD_START, &status_value
        );
    }
    if (status == VF2_OK) {
        cpu->registers[16] = status_value;
        status = vf2_model2a_read_u32(
            machine, VF2_TEXTURE_RUNTIME_FLAGS, &runtime_flags
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[15] = runtime_flags;
    if ((runtime_flags & (UINT32_C(1) << 9u)) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu,
        VF2_TEXTURE_STATUS_DISPATCH_TARGET,
        VF2_TEXTURE_STATUS_DISPATCH_RETURN
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions += UINT64_C(8);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_STATUS_DISPATCH_CALL;
    report->entry_address = VF2_TEXTURE_STATUS_DISPATCH_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->recovered_instruction_count = UINT64_C(8);
    report->recovered_procedure_calls = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_texture_record_advance(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint16_t raw_count = 0u;
    uint32_t count = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u || cpu->registers[6] != 1u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = read_u16(machine, cpu->registers[5] + UINT32_C(2), &raw_count);
    if (status != VF2_OK) {
        return status;
    }
    count = (uint32_t)(int32_t)(int16_t)raw_count;
    cpu->registers[VF2_I960_G0_REGISTER] = count;
    if ((int32_t)count <= 0 || cpu->registers[8] != count) {
        return VF2_ERROR_UNSUPPORTED;
    }

    cpu->registers[6] = 0u;
    cpu->registers[8] -= UINT32_C(1);
    cpu->registers[9] += UINT32_C(4);
    cpu->registers[10] += UINT32_C(4);

    status = write_u16(
        machine,
        cpu->registers[5] + UINT32_C(2),
        (uint16_t)cpu->registers[8]
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine,
            cpu->registers[5] + UINT32_C(0x14),
            cpu->registers[9]
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine,
            cpu->registers[5] + UINT32_C(0x18),
            cpu->registers[10]
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    set_equal_condition(cpu);
    cpu->ip = VF2_TEXTURE_RECORD_ADVANCE_EXIT;
    cpu->executed_instructions += UINT64_C(12);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_RECORD_ADVANCE;
    report->entry_address = VF2_TEXTURE_RECORD_ADVANCE_ENTRY;
    report->exit_address = VF2_TEXTURE_RECORD_ADVANCE_EXIT;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(3);
    report->bytes_written = 10u;
    report->recovered_instruction_count = UINT64_C(12);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}


static vf2_status execute_texture_final_status_call(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t counter0 = 0u;
    uint32_t counter1 = 0u;
    uint32_t counter2 = 0u;
    uint32_t runtime_flags = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    cpu->registers[3] = 0u;
    status = write_u16(machine, VF2_TEXTURE_STATUS_WORD, 0u);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_TEXTURE_COUNTER0, &counter0
        );
    }
    if (status == VF2_OK) {
        cpu->registers[14] = counter0;
        if (counter0 != 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = vf2_model2a_read_u32(
            machine, VF2_TEXTURE_COUNTER1, &counter1
        );
    }
    if (status == VF2_OK) {
        cpu->registers[14] = counter1;
        if (counter1 != 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = vf2_model2a_read_u32(
            machine, VF2_TEXTURE_COUNTER2, &counter2
        );
    }
    if (status == VF2_OK) {
        cpu->registers[14] = counter2;
        if (counter2 == 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = vf2_model2a_read_u32(
            machine, VF2_TEXTURE_RUNTIME_FLAGS, &runtime_flags
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[15] = runtime_flags;
    if ((runtime_flags & (UINT32_C(1) << 9u)) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu,
        VF2_TEXTURE_FINAL_STATUS_TARGET,
        VF2_TEXTURE_FINAL_STATUS_RETURN
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions += UINT64_C(11);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_FINAL_STATUS_CALL;
    report->entry_address = VF2_TEXTURE_FINAL_STATUS_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(1);
    report->bytes_written = 2u;
    report->recovered_instruction_count = UINT64_C(11);
    report->recovered_procedure_calls = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_texture_body_return(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    cpu->executed_instructions += UINT64_C(1);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_BODY_RETURN;
    report->entry_address = VF2_TEXTURE_BODY_RETURN_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->recovered_instruction_count = UINT64_C(1);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_texture_post_body_call(
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_i960_cpu_enter_procedure(
        cpu,
        VF2_TEXTURE_POST_BODY_CALL_TARGET,
        VF2_TEXTURE_POST_BODY_CALL_RETURN
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions += UINT64_C(1);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_POST_BODY_CALL;
    report->entry_address = VF2_TEXTURE_POST_BODY_CALL_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->recovered_instruction_count = UINT64_C(1);
    report->recovered_procedure_calls = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_texture_counter_update(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t counter0 = 0u;
    uint32_t counter1 = 0u;
    uint32_t counter2 = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    cpu->registers[3] = VF2_TEXTURE_COUNTER0;
    status = vf2_model2a_read_u32(machine, VF2_TEXTURE_COUNTER0, &counter0);
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[4] = counter0 - UINT32_C(1);
    if (counter0 != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    cpu->registers[3] = VF2_TEXTURE_COUNTER1;
    status = vf2_model2a_read_u32(machine, VF2_TEXTURE_COUNTER1, &counter1);
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[4] = counter1 - UINT32_C(1);
    if (counter1 != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    cpu->registers[3] = VF2_TEXTURE_COUNTER2;
    status = vf2_model2a_read_u32(machine, VF2_TEXTURE_COUNTER2, &counter2);
    if (status != VF2_OK) {
        return status;
    }
    if (counter2 <= UINT32_C(1)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    cpu->registers[4] = counter2 - UINT32_C(1);
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(4);
    cpu->compare_result = VF2_I960_COMPARE_LESS;
    status = vf2_model2a_write_u32(
        machine, VF2_TEXTURE_COUNTER2, cpu->registers[4]
    );
    if (status != VF2_OK) {
        return status;
    }

    cpu->ip = VF2_TEXTURE_COUNTER_UPDATE_EXIT;
    cpu->executed_instructions += UINT64_C(14);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_COUNTER_UPDATE;
    report->entry_address = VF2_TEXTURE_COUNTER_UPDATE_ENTRY;
    report->exit_address = VF2_TEXTURE_COUNTER_UPDATE_EXIT;
    report->iterations = UINT64_C(3);
    report->changed_values = UINT64_C(1);
    report->bytes_written = 4u;
    report->recovered_instruction_count = UINT64_C(14);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}


static vf2_status execute_texture_orchestrator_epilogue(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint32_t stack_end = cpu->registers[1];
    uint32_t register_index = 0u;
    uint32_t value = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u || stack_end < UINT32_C(112)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    for (register_index = 30u; register_index >= 3u;
         --register_index) {
        status = vf2_model2a_read_u32(
            machine,
            stack_end - (UINT32_C(31) - register_index) * UINT32_C(4),
            &value
        );
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[register_index] = value;
        if (register_index == 3u) {
            break;
        }
    }
    cpu->registers[1] = stack_end - UINT32_C(112);
    cpu->executed_instructions += UINT64_C(21);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }

    report->kind =
        VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_EPILOGUE;
    report->entry_address = VF2_TEXTURE_ORCHESTRATOR_EPILOGUE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(28);
    report->changed_values = UINT64_C(28);
    report->recovered_instruction_count = UINT64_C(21);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_texture_orchestrator_save_call(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint32_t stack_start = cpu->registers[1];
    uint32_t register_index = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    for (register_index = 3u; register_index <= 30u;
         ++register_index) {
        status = vf2_model2a_write_u32(
            machine,
            stack_start + (register_index - 3u) * UINT32_C(4),
            cpu->registers[register_index]
        );
        if (status != VF2_OK) {
            return status;
        }
    }

    cpu->registers[1] = stack_start + UINT32_C(112);
    status = vf2_i960_cpu_enter_procedure(
        cpu,
        VF2_TEXTURE_ORCHESTRATOR_BODY_ENTRY,
        VF2_TEXTURE_ORCHESTRATOR_BODY_RETURN
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions += UINT64_C(21);

    report->kind =
        VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_SAVE_CALL;
    report->entry_address = VF2_TEXTURE_ORCHESTRATOR_SAVE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(28);
    report->bytes_written = 112u;
    report->recovered_instruction_count = UINT64_C(21);
    report->recovered_procedure_calls = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_texture_frame_gate_call(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint8_t frame_state = 0u;
    const uint8_t latch_value = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read(
        machine,
        VF2_FRAME_STATE,
        &frame_state,
        sizeof(frame_state)
    );
    if (status != VF2_OK) {
        return status;
    }
    if (frame_state != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    cpu->registers[16] = 0u;
    status = vf2_model2a_write(
        machine,
        VF2_TEXTURE_FRAME_GATE_LATCH,
        &latch_value,
        sizeof(latch_value)
    );
    if (status != VF2_OK) {
        return status;
    }
    status = vf2_i960_cpu_enter_procedure(
        cpu,
        VF2_TEXTURE_DEFAULT_LIMITS_ENTRY,
        VF2_TEXTURE_DEFAULT_LIMITS_RETURN
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions += UINT64_C(5);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_FRAME_GATE_CALL;
    report->entry_address = VF2_TEXTURE_FRAME_GATE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(1);
    report->bytes_written = 1u;
    report->recovered_instruction_count = UINT64_C(5);
    report->recovered_procedure_calls = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_texture_default_limits(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_orchestrator_limits_report limits_report;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(&limits_report, 0, sizeof(limits_report));
    status = vf2_orchestrator_apply_default_limits(
        machine, &limits_report
    );
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions +=
        limits_report.interpreted_instruction_equivalent;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_DEFAULT_LIMITS;
    report->entry_address = VF2_TEXTURE_DEFAULT_LIMITS_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(2);
    report->bytes_written = limits_report.bytes_written;
    report->recovered_instruction_count =
        limits_report.interpreted_instruction_equivalent;
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_texture_orchestrator_gate(
    const vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_orchestrator_gate_report gate_report;
    vf2_status status = VF2_OK;

    memset(&gate_report, 0, sizeof(gate_report));
    if (cpu->ip == VF2_ORCHESTRATOR_LOOP_GATE_ENTRY) {
        status = vf2_orchestrator_apply_zero_loop_gate(
            machine, cpu, &gate_report
        );
    } else {
        status = vf2_orchestrator_enter_zero_child_gate(
            machine, cpu, &gate_report
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    switch (gate_report.kind) {
    case VF2_ORCHESTRATOR_GATE_CHILD_A:
        report->kind = VF2_HYBRID_BRIDGE_TEXTURE_CHILD_GATE_A;
        break;
    case VF2_ORCHESTRATOR_GATE_CHILD_B:
        report->kind = VF2_HYBRID_BRIDGE_TEXTURE_CHILD_GATE_B;
        break;
    case VF2_ORCHESTRATOR_GATE_LOOP_TAIL:
        report->kind = VF2_HYBRID_BRIDGE_TEXTURE_LOOP_GATE;
        break;
    case VF2_ORCHESTRATOR_GATE_NONE:
    default:
        return VF2_ERROR_UNSUPPORTED;
    }

    report->entry_address = gate_report.entry_address;
    report->exit_address = gate_report.exit_address;
    report->iterations = UINT64_C(1);
    report->recovered_instruction_count =
        gate_report.recovered_instruction_count;
    report->recovered_procedure_calls =
        gate_report.recovered_procedure_calls;
    report->cpu_poststate_applied = gate_report.cpu_poststate_applied;
    return VF2_OK;
}

vf2_status vf2_hybrid_post_frame_bridge_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report local_report;
    vf2_status status = VF2_ERROR_UNSUPPORTED;

    if (machine == NULL || cpu == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&local_report, 0, sizeof(local_report));
    switch (cpu->ip) {
    case VF2_TEXTURE_BYTE_RUN_ENTRY:
        status = execute_texture_byte_run(machine, cpu, &local_report);
        break;
    case VF2_TEXTURE_BYTE_DECODE_ENTRY:
        status = execute_texture_byte_decode(machine, cpu, &local_report);
        break;
    case VF2_TEXTURE_WORD_RUN_ENTRY:
        status = execute_texture_word_run(machine, cpu, &local_report);
        break;
    case VF2_TEXTURE_WORD_PREPARE_ENTRY:
        status = execute_texture_word_prepare(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_WORD_DECODE_ENTRY:
        status = execute_texture_word_decode(machine, cpu, &local_report);
        break;
    case VF2_TEXTURE_SYMBOL_TABLE_ENTRY:
        status = execute_texture_symbol_table_build(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_PAIR_TABLE_ENTRY:
        status = execute_texture_pair_table_build(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_TREE_DISPATCH_ENTRY:
        status = execute_texture_tree_dispatch(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_TREE_ENTRY:
        status = execute_texture_tree(machine, cpu, &local_report);
        break;
    case VF2_TEXTURE_COLOR_PREPARE_ENTRY:
        status = execute_texture_color_prepare(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_CONVERT_ENTRY:
        status = execute_texture_convert(machine, cpu, &local_report);
        break;
    case VF2_ORCHESTRATOR_CHILD_GATE_A_ENTRY:
    case VF2_ORCHESTRATOR_CHILD_GATE_B_ENTRY:
    case VF2_ORCHESTRATOR_LOOP_GATE_ENTRY:
        status = execute_texture_orchestrator_gate(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_HEADER_DECODE_ENTRY:
        status = execute_texture_header_decode(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_ACTIVE_PREPARE_ENTRY:
        status = execute_texture_active_prepare_call(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_STATUS_DISPATCH_ENTRY:
        status = execute_texture_status_dispatch_call(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_RECORD_ADVANCE_ENTRY:
        status = execute_texture_record_advance(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_FINAL_STATUS_ENTRY:
        status = execute_texture_final_status_call(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_BODY_RETURN_ENTRY:
        status = execute_texture_body_return(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_POST_BODY_CALL_ENTRY:
        status = execute_texture_post_body_call(
            cpu, &local_report
        );
        break;
    case VF2_TEXTURE_COUNTER_UPDATE_ENTRY:
        status = execute_texture_counter_update(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_ORCHESTRATOR_EPILOGUE_ENTRY:
        status = execute_texture_orchestrator_epilogue(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_ORCHESTRATOR_SAVE_ENTRY:
        status = execute_texture_orchestrator_save_call(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_FRAME_GATE_ENTRY:
        status = execute_texture_frame_gate_call(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_DEFAULT_LIMITS_ENTRY:
        status = execute_texture_default_limits(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_ADDRESS_TABLE_ENTRY:
        status = execute_texture_address_table(machine, cpu, &local_report);
        break;
    case VF2_DIAGNOSTIC_TEXT_COPY_ENTRY:
        status = execute_diagnostic_text_copy(machine, cpu, &local_report);
        break;
    case VF2_TILE_GLYPH_EXPAND_ENTRY:
        status = execute_tile_glyph_expand(machine, cpu, &local_report);
        break;
    case VF2_VIDEO_STATUS_LATCH_ENTRY:
        status = execute_video_status_latch(machine, cpu, &local_report);
        break;
    case VF2_GEOMETRY_FRAME_COMMIT_ENTRY:
        status = execute_geometry_frame_commit(machine, cpu, &local_report);
        break;
    case VF2_GEOMETRY_COMMAND_SETUP_ENTRY:
        status = execute_geometry_command_setup(machine, cpu, &local_report);
        break;
    case VF2_FRAME_SCRATCH_CLEAR_ENTRY:
        status = execute_frame_scratch_clear(machine, cpu, &local_report);
        break;
    case VF2_PALETTE_PAGE_UPLOAD_ENTRY:
        status = execute_palette_page_upload(machine, cpu, &local_report);
        break;
    case VF2_TEXTURE_CONVERT_LOOP_ENTRY:
        status = execute_texture_convert_loop(machine, cpu, &local_report);
        break;
    case VF2_TEXTURE_CONVERT_POST_ENTRY:
        status = execute_texture_convert_post(machine, cpu, &local_report);
        break;
    case VF2_TIMER_WAIT_UPDATE_ENTRY:
        status = execute_timer_wait_update(machine, cpu, &local_report);
        break;
    case VF2_INLINE_TEXT_THUNK_ENTRY:
        status = execute_inline_text_thunk(machine, cpu, &local_report);
        break;
    case VF2_TEXTURE_STATUS_LINE_ENTRY:
        status = execute_texture_status_line(machine, cpu, &local_report);
        break;
    case VF2_GAME_STATE_CLASSIFY_ENTRY:
        status = execute_game_state_classify(machine, cpu, &local_report);
        break;
    case VF2_GAME_COLOR_LOOKUP_ENTRY:
        status = execute_game_color_lookup(machine, cpu, &local_report);
        break;
    default:
        status = VF2_ERROR_UNSUPPORTED;
        break;
    }
    if (status == VF2_OK && report != NULL) {
        *report = local_report;
    }
    return status;
}

const char *vf2_hybrid_bridge_kind_name(vf2_hybrid_bridge_kind kind)
{
    switch (kind) {
    case VF2_HYBRID_BRIDGE_TEXTURE_BYTE_RUN:
        return "texture-byte-run";
    case VF2_HYBRID_BRIDGE_TEXTURE_BYTE_DECODE:
        return "texture-byte-decode";
    case VF2_HYBRID_BRIDGE_TEXTURE_WORD_RUN:
        return "texture-word-run";
    case VF2_HYBRID_BRIDGE_TEXTURE_WORD_DECODE:
        return "texture-word-decode";
    case VF2_HYBRID_BRIDGE_TEXTURE_SYMBOL_TABLE_BUILD:
        return "texture-symbol-table-build";
    case VF2_HYBRID_BRIDGE_TEXTURE_PAIR_TABLE_BUILD:
        return "texture-pair-table-build";
    case VF2_HYBRID_BRIDGE_TEXTURE_TREE_DISPATCH:
        return "texture-tree-dispatch";
    case VF2_HYBRID_BRIDGE_TEXTURE_TREE_EXPAND:
        return "texture-tree-expand";
    case VF2_HYBRID_BRIDGE_TEXTURE_WORD_PREPARE:
        return "texture-word-prepare";
    case VF2_HYBRID_BRIDGE_TEXTURE_COLOR_PREPARE:
        return "texture-color-prepare";
    case VF2_HYBRID_BRIDGE_TEXTURE_COLOR_CONVERT:
        return "texture-color-convert";
    case VF2_HYBRID_BRIDGE_TEXTURE_ADDRESS_TABLE:
        return "texture-address-table";
    case VF2_HYBRID_BRIDGE_DIAGNOSTIC_TEXT_COPY:
        return "diagnostic-text-copy";
    case VF2_HYBRID_BRIDGE_TILE_GLYPH_EXPAND:
        return "tile-glyph-expand";
    case VF2_HYBRID_BRIDGE_VIDEO_STATUS_LATCH:
        return "video-status-latch";
    case VF2_HYBRID_BRIDGE_GEOMETRY_FRAME_COMMIT:
        return "geometry-frame-commit";
    case VF2_HYBRID_BRIDGE_GEOMETRY_COMMAND_SETUP:
        return "geometry-command-setup";
    case VF2_HYBRID_BRIDGE_FRAME_SCRATCH_CLEAR:
        return "frame-scratch-clear";
    case VF2_HYBRID_BRIDGE_PALETTE_PAGE_UPLOAD:
        return "palette-page-upload";
    case VF2_HYBRID_BRIDGE_TEXTURE_CONVERT_LOOP:
        return "texture-convert-loop";
    case VF2_HYBRID_BRIDGE_TEXTURE_CONVERT_POST:
        return "texture-convert-post";
    case VF2_HYBRID_BRIDGE_TIMER_WAIT_UPDATE:
        return "timer-wait-update";
    case VF2_HYBRID_BRIDGE_INLINE_TEXT_THUNK:
        return "inline-text-thunk";
    case VF2_HYBRID_BRIDGE_TEXTURE_STATUS_LINE:
        return "texture-status-line";
    case VF2_HYBRID_BRIDGE_GAME_STATE_CLASSIFY:
        return "game-state-classify";
    case VF2_HYBRID_BRIDGE_GAME_COLOR_LOOKUP:
        return "game-color-lookup";
    case VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_SAVE_CALL:
        return "texture-orchestrator-save-call";
    case VF2_HYBRID_BRIDGE_TEXTURE_FRAME_GATE_CALL:
        return "texture-frame-gate-call";
    case VF2_HYBRID_BRIDGE_TEXTURE_DEFAULT_LIMITS:
        return "texture-default-limits";
    case VF2_HYBRID_BRIDGE_TEXTURE_STATUS_DISPATCH_CALL:
        return "texture-status-dispatch-call";
    case VF2_HYBRID_BRIDGE_TEXTURE_ACTIVE_PREPARE_CALL:
        return "texture-active-prepare-call";
    case VF2_HYBRID_BRIDGE_TEXTURE_HEADER_DECODE:
        return "texture-header-decode";
    case VF2_HYBRID_BRIDGE_TEXTURE_STATUS_SCAN_END:
        return "texture-status-scan-end";
    case VF2_HYBRID_BRIDGE_TEXTURE_CHILD_GATE_A:
        return "texture-child-gate-a";
    case VF2_HYBRID_BRIDGE_TEXTURE_CHILD_GATE_B:
        return "texture-child-gate-b";
    case VF2_HYBRID_BRIDGE_TEXTURE_LOOP_GATE:
        return "texture-loop-gate";
    case VF2_HYBRID_BRIDGE_TEXTURE_RECORD_ADVANCE:
        return "texture-record-advance";
    case VF2_HYBRID_BRIDGE_TEXTURE_FINAL_STATUS_CALL:
        return "texture-final-status-call";
    case VF2_HYBRID_BRIDGE_TEXTURE_BODY_RETURN:
        return "texture-body-return";
    case VF2_HYBRID_BRIDGE_TEXTURE_POST_BODY_CALL:
        return "texture-post-body-call";
    case VF2_HYBRID_BRIDGE_TEXTURE_COUNTER_UPDATE:
        return "texture-counter-update";
    case VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_EPILOGUE:
        return "texture-orchestrator-epilogue";
    case VF2_HYBRID_BRIDGE_SECOND_SCHEDULER_ENTRY:
        return "second-scheduler-entry";
    case VF2_HYBRID_BRIDGE_NONE:
    default:
        return "none";
    }
}
