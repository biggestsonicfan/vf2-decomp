#include "texture_bridge_internal.h"

vf2_status copy_diagnostic_text(
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

vf2_status execute_video_input_sync(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t flags = 0u;
    uint32_t control = 0u;
    uint32_t previous_control = 0u;
    uint32_t enable_mask = 0u;
    uint32_t disable_mask = 0u;
    uint16_t input_a = 0u;
    uint16_t input_b = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00508000), &flags);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500710), &control);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500704), &disable_mask);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500700), &enable_mask);
    }
    if (status == VF2_OK) {
        status = read_u16(machine, UINT32_C(0x0100a008), &input_a);
    }
    if (status == VF2_OK) {
        status = read_u16(machine, UINT32_C(0x0100a00a), &input_b);
    }
    if (status != VF2_OK) {
        return status;
    }
    if ((control & (UINT32_C(1) << 6u)) != 0u) {
        input_a |= UINT16_C(0x8000);
        input_b |= UINT16_C(0x8000);
    } else {
        input_a &= UINT16_C(0x7fff);
        input_b &= UINT16_C(0x7fff);
    }
    status = write_u16(machine, UINT32_C(0x0100a008), input_a);
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x0100a00a), input_b);
    }
    flags &= ~(UINT32_C(1) << 9u);
    if ((control & UINT32_C(1)) == 0u) {
        flags |= UINT32_C(1) << 9u;
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500714), &previous_control
        );
        if (status != VF2_OK || (previous_control & UINT32_C(1)) != 0u) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
    }
    if ((control & (UINT32_C(1) << 1u)) != 0u) {
        flags |= UINT32_C(1) << 10u;
    } else {
        flags &= ~(UINT32_C(1) << 10u);
    }
    flags &= ~UINT32_C(0x000000d0);
    if ((flags & ((UINT32_C(1) << 12u) |
                  (UINT32_C(1) << 2u) |
                  (UINT32_C(1) << 3u))) != 0u ||
        (flags & (UINT32_C(1) << 9u)) == 0u ||
        (flags & (UINT32_C(1) << 10u)) != 0u ||
        (flags & (UINT32_C(1) << 1u)) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_write_u32(machine, UINT32_C(0x00508000), flags);
    if (status != VF2_OK) {
        return status;
    }
    cpu->compare_result = VF2_I960_COMPARE_NONE;
    cpu->arithmetic_control &= ~UINT32_C(7);
    status = finish_recovered_procedure(machine, cpu, UINT64_C(28));
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_VIDEO_INPUT_SYNC;
    report->entry_address = VF2_VIDEO_INPUT_SYNC_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(3);
    report->bytes_written = 8u;
    report->recovered_instruction_count = UINT64_C(28);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_input_sequence_gate(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t entry,
    uint32_t index_address,
    uint32_t countdown_address,
    vf2_hybrid_bridge_kind kind,
    vf2_hybrid_bridge_report *report
)
{
    uint8_t index = 0u;
    uint8_t countdown = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read(machine, index_address, &index, 1u);
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, countdown_address, &countdown, 1u);
    }
    if (status != VF2_OK || (index & UINT8_C(15)) != 0u || countdown != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = finish_recovered_procedure(machine, cpu, UINT64_C(7));
    if (status != VF2_OK) {
        return status;
    }
    report->kind = kind;
    report->entry_address = entry;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->recovered_instruction_count = UINT64_C(7);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_input_ring_poll(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint8_t producer = 0u;
    uint8_t consumer = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read(machine, UINT32_C(0x005000fc), &producer, 1u);
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x005000fd), &consumer, 1u);
    }
    if (status != VF2_OK || producer != consumer) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[VF2_I960_G0_REGISTER] = UINT32_MAX;
    status = finish_recovered_procedure(machine, cpu, UINT64_C(5));
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_HYBRID_BRIDGE_INPUT_RING_POLL;
    report->entry_address = VF2_INPUT_RING_POLL_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(1);
    report->recovered_instruction_count = UINT64_C(5);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_tile_runtime_gate(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t flags = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00508000), &flags);
    if (status != VF2_OK || (flags & (UINT32_C(1) << 4u)) != 0u ||
        (flags & (UINT32_C(1) << 2u)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = finish_recovered_procedure(machine, cpu, UINT64_C(4));
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_HYBRID_BRIDGE_TILE_RUNTIME_GATE;
    report->entry_address = VF2_TILE_RUNTIME_GATE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->recovered_instruction_count = UINT64_C(4);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_game_event_queue_write(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint8_t count = 0u;
    uint8_t index = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    cpu->registers[6] = VF2_GAME_EVENT_QUEUE_MMIO;
    cpu->registers[3] = UINT32_C(33);
    status = vf2_model2a_write_u32(
        machine, cpu->registers[6], cpu->registers[3]
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, cpu->registers[6], cpu->registers[3]
        );
    }
    cpu->registers[3] = UINT32_C(16);
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x00504001), &count, sizeof(count)
        );
    }
    if (status != VF2_OK || count >= UINT8_C(16)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    ++count;
    cpu->registers[5] = count;
    status = vf2_model2a_write(
        machine, UINT32_C(0x00504001), &count, sizeof(count)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x00504003), &index, sizeof(index)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine,
            UINT32_C(0x00504020) + (uint32_t)index * UINT32_C(4),
            cpu->registers[VF2_I960_G0_REGISTER]
        );
    }
    cpu->registers[4] = UINT32_C(15);
    index = (uint8_t)(((uint32_t)index + UINT32_C(1)) & UINT32_C(15));
    cpu->registers[3] = index;
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x00504003), &index, sizeof(index)
        );
    }
    cpu->registers[3] = UINT32_C(0x00000421);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, cpu->registers[6], cpu->registers[3]
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, cpu->registers[6], cpu->registers[3]
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    status = finish_recovered_procedure(machine, cpu, UINT64_C(19));
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_GAME_EVENT_QUEUE_WRITE;
    report->entry_address = VF2_GAME_EVENT_QUEUE_WRITE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(4);
    report->bytes_written = 22u;
    report->recovered_instruction_count = UINT64_C(19);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_game_input_update(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report bit0_report;
    vf2_hybrid_bridge_report bit1_report;
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint32_t runtime_flags = 0u;
    uint32_t selector_mask = 0u;
    uint32_t saved_g9 = cpu->registers[VF2_I960_G0_REGISTER + 9u];
    uint32_t base = 0u;
    uint32_t input_flags = 0u;
    uint64_t own_instruction_count = UINT64_C(9);
    size_t bytes_written = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(&bit0_report, 0, sizeof(bit0_report));
    memset(&bit1_report, 0, sizeof(bit1_report));

    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00500068), &runtime_flags
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050002c), &selector_mask
        );
    }
    if (status != VF2_OK ||
        (runtime_flags & (UINT32_C(1) << 31u)) == 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[15] = selector_mask & UINT32_C(0x00030000);
    cpu->registers[14] = UINT32_C(0x00030000);

    /* A non-zero selector mask takes the cmpobne at 0x00001adc and skips the
     * long meter/input body, continuing directly with both sequence gates. */
    if ((selector_mask & UINT32_C(0x00030000)) == 0u) {
        own_instruction_count = UINT64_C(18);
        bytes_written = sizeof(uint32_t);

        cpu->registers[1] += UINT32_C(4);
        status = vf2_model2a_write_u32(
            machine, cpu->registers[1] - UINT32_C(4), saved_g9
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x0050016c), &base
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x00500708), &input_flags
            );
        }
        if (status != VF2_OK || (input_flags & UINT32_C(3)) != 0u) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        cpu->registers[VF2_I960_G0_REGISTER + 9u] = base;
        cpu->registers[3] = UINT32_C(0xff);
        cpu->registers[8] = input_flags;

        status = vf2_model2a_read_u32(
            machine, cpu->registers[1] - UINT32_C(4),
            &cpu->registers[VF2_I960_G0_REGISTER + 9u]
        );
        cpu->registers[1] -= UINT32_C(4);
        if (status != VF2_OK ||
            cpu->registers[VF2_I960_G0_REGISTER + 9u] != saved_g9) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_INPUT_BIT0_SEQUENCE_GATE_ENTRY, UINT32_C(0x00001e48)
    );
    if (status == VF2_OK) {
        status = execute_input_sequence_gate(
            machine, cpu, VF2_INPUT_BIT0_SEQUENCE_GATE_ENTRY,
            UINT32_C(0x00500148), UINT32_C(0x0050014a),
            VF2_HYBRID_BRIDGE_INPUT_BIT0_SEQUENCE_GATE, &bit0_report
        );
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00001e48)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_INPUT_BIT1_SEQUENCE_GATE_ENTRY, UINT32_C(0x00001e4c)
    );
    if (status == VF2_OK) {
        status = execute_input_sequence_gate(
            machine, cpu, VF2_INPUT_BIT1_SEQUENCE_GATE_ENTRY,
            UINT32_C(0x00500149), UINT32_C(0x0050014b),
            VF2_HYBRID_BRIDGE_INPUT_BIT1_SEQUENCE_GATE, &bit1_report
        );
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00001e4c)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = finish_recovered_procedure(
        machine, cpu, own_instruction_count
    );
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_HYBRID_BRIDGE_GAME_INPUT_UPDATE;
    report->entry_address = VF2_GAME_INPUT_UPDATE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(2);
    report->bytes_written = bytes_written;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_diagnostic_text_copy(
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

vf2_status translate_glyph_word(
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

vf2_status execute_tile_glyph_expand(
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
