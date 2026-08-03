#include "texture_bridge_internal.h"


vf2_status finish_recovered_procedure(
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

vf2_status classify_observed_game_state(
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

vf2_status execute_game_state_classify(
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

vf2_status execute_game_color_lookup(
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

vf2_status execute_game_meter_component(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t selector,
    uint32_t structure,
    uint32_t return_address,
    uint32_t *meter_value,
    uint32_t *secondary_value,
    size_t *bytes_written
)
{
    vf2_hybrid_bridge_report first_color_report;
    vf2_hybrid_bridge_report second_color_report;
    uint32_t first_color = 0u;
    uint32_t second_color = 0u;
    uint32_t restored_selector = 0u;
    uint32_t restored_structure = 0u;
    uint32_t low_nibble = 0u;
    uint32_t middle_nibble = 0u;
    uint32_t high_nibble = 0u;
    uint32_t offset_result = 0u;
    uint32_t component = 0u;
    uint32_t threshold = UINT32_C(9);
    uint32_t base = 0u;
    uint8_t variant = 0u;
    uint8_t byte0 = 0u;
    uint8_t byte4 = 0u;
    uint8_t byte5 = 0u;
    vf2_status status = VF2_OK;

    memset(&first_color_report, 0, sizeof(first_color_report));
    memset(&second_color_report, 0, sizeof(second_color_report));

    cpu->registers[VF2_I960_G0_REGISTER] = selector;
    cpu->registers[VF2_I960_G0_REGISTER + 1u] = structure;
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_METER_COMPONENT_ENTRY, return_address
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[3] = selector;

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_METER_VALUE_ENTRY, UINT32_C(0x00002550)
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[3] = selector;
    cpu->registers[4] = structure;
    cpu->registers[1] += UINT32_C(4);
    status = vf2_model2a_write_u32(
        machine, cpu->registers[1] - UINT32_C(4), selector
    );
    cpu->registers[1] += UINT32_C(4);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, cpu->registers[1] - UINT32_C(4), structure
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_COLOR_LOOKUP_ENTRY, UINT32_C(0x00002664)
    );
    if (status == VF2_OK) {
        status = execute_game_color_lookup(
            machine, cpu, &first_color_report
        );
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00002664)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    first_color = cpu->registers[VF2_I960_G0_REGISTER];
    low_nibble = first_color & UINT32_C(15);
    middle_nibble = (first_color >> 8u) & UINT32_C(15);
    high_nibble = (first_color >> 16u) & UINT32_C(15);

    status = vf2_model2a_read_u32(
        machine, cpu->registers[1] - UINT32_C(4), &restored_structure
    );
    if (status == VF2_OK) {
        cpu->registers[VF2_I960_G0_REGISTER + 1u] = restored_structure;
        cpu->registers[1] -= UINT32_C(4);
        status = vf2_model2a_read_u32(
            machine, cpu->registers[1] - UINT32_C(4), &restored_selector
        );
    }
    if (status != VF2_OK || restored_structure != structure ||
        restored_selector != selector) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[VF2_I960_G0_REGISTER] = restored_selector;
    cpu->registers[1] -= UINT32_C(4);

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_METER_OFFSET_ENTRY, UINT32_C(0x00002694)
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[3] = structure;
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_COLOR_LOOKUP_ENTRY, UINT32_C(0x000026b8)
    );
    if (status == VF2_OK) {
        status = execute_game_color_lookup(
            machine, cpu, &second_color_report
        );
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x000026b8)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    second_color = cpu->registers[VF2_I960_G0_REGISTER];
    status = vf2_model2a_read(
        machine, structure + UINT32_C(4), &byte4, sizeof(byte4)
    );
    if (status != VF2_OK) {
        return status;
    }
    {
        const uint32_t second_low = second_color & UINT32_C(15);
        const uint32_t second_middle =
            (second_color >> 8u) & UINT32_C(15);
        const uint32_t scaled = second_low * (uint32_t)byte4;
        const uint32_t scaled_next =
            second_low * ((uint32_t)byte4 + UINT32_C(1));

        if (second_middle == UINT32_C(1)) {
            offset_result = 0u;
        } else if (second_middle != 0u) {
            offset_result =
                scaled_next / second_middle - scaled / second_middle;
        } else {
            return VF2_ERROR_UNSUPPORTED;
        }
    }
    cpu->registers[VF2_I960_G0_REGISTER] = offset_result;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00002694)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[VF2_I960_G0_REGISTER + 1u] =
        offset_result + low_nibble;
    status = vf2_model2a_read(
        machine, structure, &byte0, sizeof(byte0)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, structure + UINT32_C(5), &byte5, sizeof(byte5)
        );
    }
    if (status != VF2_OK || high_nibble == 0u || middle_nibble == 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    component = (uint32_t)byte0 / high_nibble + (uint32_t)byte5;
    cpu->registers[VF2_I960_G0_REGISTER] = component;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00002550)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0050016c), &base
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3350), &variant, sizeof(variant)
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if (variant == UINT8_C(1)) {
        threshold = UINT32_C(24);
    }
    if (component >= threshold) {
        component |= UINT32_C(1) << 16u;
    }
    cpu->registers[VF2_I960_G0_REGISTER] = component;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != return_address) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    if (meter_value != NULL) {
        *meter_value = component;
    }
    if (secondary_value != NULL) {
        *secondary_value = cpu->registers[VF2_I960_G0_REGISTER + 1u];
    }
    if (bytes_written != NULL) {
        *bytes_written += sizeof(uint32_t) * 2u +
            first_color_report.bytes_written +
            second_color_report.bytes_written;
    }
    return VF2_OK;
}

vf2_status execute_game_meter_update(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report threshold_report;
    uint32_t base = 0u;
    uint32_t flags = 0u;
    uint32_t first_meter = 0u;
    uint32_t second_meter = 0u;
    uint32_t secondary = 0u;
    uint32_t threshold = UINT32_C(9);
    uint8_t variant = 0u;
    size_t bytes_written = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(&threshold_report, 0, sizeof(threshold_report));
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0050016c), &base
    );
    if (status != VF2_OK || cpu->registers[VF2_I960_G0_REGISTER + 9u] != base) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_THRESHOLD_EVALUATE_ENTRY, UINT32_C(0x000020f4)
    );
    if (status == VF2_OK) {
        status = execute_game_threshold_evaluate(
            machine, cpu, &threshold_report
        );
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x000020f4) ||
        cpu->registers[VF2_I960_G0_REGISTER] != 0u ||
        cpu->registers[VF2_I960_G0_REGISTER + 1u] != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    bytes_written += threshold_report.bytes_written;

    status = vf2_model2a_read_u32(
        machine, base + UINT32_C(0x3320), &flags
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3350), &variant, sizeof(variant)
        );
    }
    if (status != VF2_OK || (flags & UINT32_C(3)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    if (variant == UINT8_C(1)) {
        threshold = UINT32_C(24);
    }

    status = execute_game_meter_component(
        machine, cpu, 0u, base + UINT32_C(0x3380),
        UINT32_C(0x000022d4), &first_meter, &secondary,
        &bytes_written
    );
    if (status != VF2_OK || cpu->ip != UINT32_C(0x000022d4)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = execute_game_meter_component(
        machine, cpu, 1u, base + UINT32_C(0x3388),
        UINT32_C(0x000022ec), &second_meter, &secondary,
        &bytes_written
    );
    if (status != VF2_OK || cpu->ip != UINT32_C(0x000022ec) ||
        (first_meter & (UINT32_C(1) << 16u)) != 0u ||
        (second_meter & (UINT32_C(1) << 16u)) != 0u ||
        (first_meter & UINT32_C(0xffff)) +
            (second_meter & UINT32_C(0xffff)) >= threshold) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = finish_recovered_procedure(machine, cpu, UINT64_C(122));
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_GAME_METER_UPDATE;
    report->entry_address = VF2_GAME_METER_UPDATE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(2);
    report->changed_values = UINT64_C(2);
    report->bytes_written = bytes_written;
    report->recovered_instruction_count = UINT64_C(389);
    report->recovered_procedure_calls = UINT64_C(20);
    report->recovered_procedure_returns = UINT64_C(21);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_system_memory_diagnostic(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t low = 0u;
    uint32_t high = 0u;
    uint32_t address = 0u;
    uint8_t enabled = 0u;
    uint8_t frame_mode = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read(
        machine, UINT32_C(0x00500171), &enabled, sizeof(enabled)
    );
    if (status != VF2_OK || (enabled & UINT8_C(1)) == 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    for (address = VF2_MEMORY_PROBE_ADDRESS;
         address < VF2_MEMORY_PROBE_ADDRESS + UINT32_C(4);
         ++address) {
        uint8_t original = 0u;
        uint8_t observed = 0u;
        const uint8_t patterns[2] = {UINT8_C(0x55), UINT8_C(0xaa)};
        uint32_t pattern_index = 0u;

        status = vf2_model2a_read(machine, address, &original, sizeof(original));
        if (status != VF2_OK) {
            return status;
        }
        for (pattern_index = 0u; pattern_index < UINT32_C(2); ++pattern_index) {
            status = vf2_model2a_write(
                machine, address, &patterns[pattern_index], sizeof(uint8_t)
            );
            if (status == VF2_OK) {
                status = vf2_model2a_read(
                    machine, address, &observed, sizeof(observed)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, address, &original, sizeof(original)
                );
            }
            if (status != VF2_OK || observed != patterns[pattern_index]) {
                return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
            }
        }
    }
    cpu->registers[VF2_I960_G0_REGISTER] = 0u;
    status = vf2_model2a_read(
        machine, UINT32_C(0x0050002b), &frame_mode, sizeof(frame_mode)
    );
    if (status != VF2_OK || frame_mode == UINT8_C(16) || frame_mode == UINT8_C(17)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x0059c318), &low);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x0059c31c), &high);
    }
    if (status != VF2_OK) {
        return status;
    }
    ++low;
    if (low == 0u) {
        ++high;
    }
    status = vf2_model2a_write_u32(machine, UINT32_C(0x01d03318), low);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0059c318), low);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x01d0331c), high);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0059c31c), high);
    }
    if (status != VF2_OK) {
        return status;
    }
    cpu->compare_result = VF2_I960_COMPARE_GREATER;
    cpu->arithmetic_control &= ~UINT32_C(7);
    account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
    status = finish_recovered_procedure(machine, cpu, UINT64_C(75));
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_SYSTEM_MEMORY_DIAGNOSTIC;
    report->entry_address = VF2_SYSTEM_MEMORY_DIAGNOSTIC_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(4);
    report->changed_values = UINT64_C(4);
    report->bytes_written = 32u;
    report->recovered_instruction_count = UINT64_C(75);
    report->recovered_procedure_calls = UINT64_C(1);
    report->recovered_procedure_returns = UINT64_C(2);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_frame_counter_advance(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t mode = 0u;
    uint32_t counter = 0u;
    uint32_t mask = 0u;
    uint8_t shift = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x0050002c), &mode);
    if (status != VF2_OK || (mode & UINT32_C(0x00030000)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x0050d000), &counter);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x0050d000), counter + UINT32_C(1)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x0050d006), &shift, sizeof(shift)
        );
    }
    if (status != VF2_OK || shift >= UINT8_C(32)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    mask = (UINT32_C(1) << shift) - UINT32_C(1);
    if ((counter & mask) == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x0050002c), &mode);
    if (status != VF2_OK || (mode & UINT32_C(0x000cffc0)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = finish_recovered_procedure(machine, cpu, UINT64_C(21));
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_FRAME_COUNTER_ADVANCE;
    report->entry_address = VF2_FRAME_COUNTER_ADVANCE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(1);
    report->bytes_written = 4u;
    report->recovered_instruction_count = UINT64_C(21);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_frame_phase_advance(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t base = 0u;
    uint8_t phase = 0u;
    uint8_t next = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x0050016c), &base);
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3001), &phase, sizeof(phase)
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    next = (uint8_t)((phase & UINT8_C(0xf0)) + UINT8_C(0x10) +
                     (uint8_t)(((uint32_t)phase + UINT32_C(1)) & UINT32_C(0x0f)));
    status = vf2_model2a_write(
        machine, UINT32_C(0x01d03001), &next, sizeof(next)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x0059c001), &next, sizeof(next)
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    status = finish_recovered_procedure(machine, cpu, UINT64_C(10));
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_FRAME_PHASE_ADVANCE;
    report->entry_address = VF2_FRAME_PHASE_ADVANCE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(2);
    report->bytes_written = 2u;
    report->recovered_instruction_count = UINT64_C(10);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_frame_shadow_verify(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    static const uint32_t live_addresses[9] = {
        UINT32_C(0x00500234), UINT32_C(0x00500235),
        UINT32_C(0x00500236), UINT32_C(0x00500237),
        UINT32_C(0x00500238), UINT32_C(0x00500239),
        UINT32_C(0x005000e0), UINT32_C(0x005000e1),
        UINT32_C(0x005000e2)
    };
    uint32_t index = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    for (index = 0u; index < UINT32_C(9); ++index) {
        uint8_t live = 0u;
        uint8_t shadow = 0u;
        status = vf2_model2a_read(
            machine, live_addresses[index], &live, sizeof(live)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, UINT32_C(0x00544600) + index,
                &shadow, sizeof(shadow)
            );
        }
        if (status != VF2_OK || live != shadow) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
    }
    status = finish_recovered_procedure(machine, cpu, UINT64_C(28));
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_FRAME_SHADOW_VERIFY;
    report->entry_address = VF2_FRAME_SHADOW_VERIFY_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(9);
    report->recovered_instruction_count = UINT64_C(28);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_frame_buffer_gate(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t flags = 0u;
    uint32_t first = 0u;
    uint32_t second = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00508000), &flags);
    if (status != VF2_OK || (flags & (UINT32_C(1) << 1u)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00500804), &cpu->registers[23]
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500808), &cpu->registers[24]
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, cpu->registers[23], &first);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, cpu->registers[24], &second);
    }
    if (status != VF2_OK ||
        (first & (UINT32_C(1) << 5u)) != 0u ||
        (second & (UINT32_C(1) << 5u)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = finish_recovered_procedure(machine, cpu, UINT64_C(9));
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_FRAME_BUFFER_GATE;
    report->entry_address = VF2_FRAME_BUFFER_GATE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(2);
    report->recovered_instruction_count = UINT64_C(9);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status phase16_update_descriptor(
    vf2_model2a *machine,
    uint32_t pointer_address,
    uint32_t set_mask,
    uint32_t clear_mask,
    uint32_t entry_address,
    int write_entry
)
{
    uint32_t descriptor = 0u;
    uint32_t flags = 0u;
    vf2_status status = vf2_model2a_read_u32(
        machine, pointer_address, &descriptor
    );

    if (status == VF2_OK && descriptor == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, descriptor, &flags);
    }
    if (status == VF2_OK) {
        flags |= set_mask;
        flags &= ~clear_mask;
        status = vf2_model2a_write_u32(machine, descriptor, flags);
    }
    if (status == VF2_OK && write_entry) {
        status = vf2_model2a_write_u32(
            machine, descriptor + UINT32_C(0x0c), entry_address
        );
    }
    return status;
}

static vf2_status phase16_crc16(
    const vf2_model2a *machine,
    uint32_t source,
    uint32_t count,
    uint16_t *result
)
{
    uint32_t index = 0u;
    uint16_t crc = 0u;
    vf2_status status = VF2_OK;

    if (result == NULL || count == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0u; index < count; ++index) {
        uint8_t raw = 0u;
        uint16_t table_value = 0u;
        const uint16_t high = (uint16_t)((uint32_t)crc << 8u);

        crc = (uint16_t)(crc >> 8u);
        status = vf2_model2a_read(
            machine, source + index, &raw, sizeof(raw)
        );
        if (status != VF2_OK) {
            return status;
        }
        crc ^= (uint16_t)raw;
        status = read_u16(
            machine,
            UINT32_C(0x02000000) + (uint32_t)(crc & UINT16_C(0x00ff)) *
                UINT32_C(2),
            &table_value
        );
        if (status != VF2_OK) {
            return status;
        }
        crc = (uint16_t)(table_value ^ high);
    }
    *result = crc;
    return VF2_OK;
}

static vf2_status phase16_queue_sound_command(
    vf2_model2a *machine,
    uint32_t command
)
{
    uint8_t count = 0u;
    uint8_t index = 0u;
    vf2_status status = vf2_model2a_write_u32(
        machine, UINT32_C(0x00e80004), UINT32_C(33)
    );

    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00e80004), UINT32_C(33)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x00504001), &count, sizeof(count)
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if (count < UINT8_C(16)) {
        ++count;
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
                command
            );
        }
        index = (uint8_t)(((uint32_t)index + UINT32_C(1)) & UINT32_C(15));
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x00504003), &index, sizeof(index)
            );
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00e80004), UINT32_C(0x421)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00e80004), UINT32_C(0x421)
        );
    }
    return status;
}

static vf2_status phase16_clear_tile_plane(
    vf2_model2a *machine,
    uint32_t base
)
{
    uint32_t row = 0u;
    uint32_t column = 0u;

    for (row = 0u; row < UINT32_C(48); ++row) {
        for (column = 0u; column < UINT32_C(64); ++column) {
            const vf2_status status = write_u16(
                machine,
                base + row * UINT32_C(0x80) + column * UINT32_C(2),
                UINT16_C(32)
            );
            if (status != VF2_OK) {
                return status;
            }
        }
    }
    return VF2_OK;
}

static vf2_status phase16_initialize_tile_patterns(vf2_model2a *machine)
{
    static const uint16_t palette[] = {
        UINT16_C(0xfc00), UINT16_C(0x83e0), UINT16_C(0x801f),
        UINT16_C(0xffe0), UINT16_C(0xfc1f), UINT16_C(0x83ff),
        UINT16_C(0x7fff), UINT16_C(0xbdef), UINT16_C(0x3c00),
        UINT16_C(0x01e0), UINT16_C(0x000f), UINT16_C(0x3de0),
        UINT16_C(0x3c0f), UINT16_C(0x01ef), UINT16_C(0x0000)
    };
    uint8_t block[16];
    uint32_t offset = 0u;
    uint32_t page = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    for (offset = 0u; offset < UINT32_C(32); offset += UINT32_C(4)) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x01080fa0) + offset, UINT32_MAX
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    status = write_u16(machine, UINT32_C(0x0180001e), 0u);
    if (status != VF2_OK) {
        return status;
    }
    for (offset = 0u; offset < UINT32_C(0x4000); offset += UINT32_C(4)) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x01004000) + offset, UINT32_C(0x807d807d)
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    for (offset = 0u; offset < UINT32_C(0x1000); offset += UINT32_C(16)) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x01080000) + offset, block, sizeof(block)
        );
        if (status != VF2_OK) {
            return status;
        }
        for (page = 1u; page < UINT32_C(16); ++page) {
            status = vf2_model2a_write(
                machine,
                UINT32_C(0x01080000) + page * UINT32_C(0x1000) + offset,
                block,
                sizeof(block)
            );
            if (status != VF2_OK) {
                return status;
            }
        }
    }
    for (index = 0u; index < sizeof(palette) / sizeof(palette[0]); ++index) {
        status = write_u16(
            machine,
            UINT32_C(0x01800022) + (uint32_t)index * UINT32_C(0x20),
            palette[index]
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    return VF2_OK;
}

static vf2_status phase16_copy_text_record(
    vf2_model2a *machine,
    uint32_t record,
    uint32_t *last_source,
    uint32_t *last_destination,
    uint64_t *characters
)
{
    uint32_t destination = 0u;
    uint64_t copied = 0u;
    vf2_status status = VF2_OK;

    if (record == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, record, &destination);
    if (status == VF2_OK) {
        status = copy_diagnostic_text(
            machine, record + UINT32_C(4), destination, &copied
        );
    }
    if (status == VF2_OK && last_source != NULL) {
        *last_source = record + UINT32_C(4);
    }
    if (status == VF2_OK && last_destination != NULL) {
        *last_destination = destination;
    }
    if (status == VF2_OK && characters != NULL) {
        *characters += copied;
    }
    return status;
}

static vf2_status execute_frame_phase16(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    static const uint32_t set_bit3_descriptors[] = {
        UINT32_C(0x0050083c), UINT32_C(0x00500840)
    };
    static const uint32_t first_clear_descriptors[] = {
        UINT32_C(0x00500844), UINT32_C(0x00500848),
        UINT32_C(0x00500858), UINT32_C(0x00500854),
        UINT32_C(0x00500850)
    };
    static const uint32_t second_clear_descriptors[] = {
        UINT32_C(0x00500834), UINT32_C(0x00500838),
        UINT32_C(0x0050083c), UINT32_C(0x00500840),
        UINT32_C(0x00500864)
    };
    static const uint32_t transition_clear_descriptors[] = {
        UINT32_C(0x00500868), UINT32_C(0x0050086c),
        UINT32_C(0x00500804), UINT32_C(0x00500808),
        UINT32_C(0x0050081c), UINT32_C(0x00500834),
        UINT32_C(0x00500864)
    };
    static const uint32_t extra_text_records[] = {
        UINT32_C(0x0005ff08), UINT32_C(0x0005ff14),
        UINT32_C(0x0005ff18)
    };
    const uint32_t saved_g4 = cpu->registers[VF2_I960_G0_REGISTER + 4u];
    uint32_t base = 0u;
    uint32_t runtime_flags = 0u;
    uint32_t record = 0u;
    uint32_t last_source = 0u;
    uint32_t last_destination = 0u;
    uint32_t text_destination = 0u;
    uint16_t crc = 0u;
    uint16_t tile_control = 0u;
    uint8_t event_flags = 0u;
    uint8_t phase = 0u;
    uint8_t zero = 0u;
    uint8_t eleven = UINT8_C(11);
    uint8_t one = UINT8_C(1);
    uint8_t ff = UINT8_MAX;
    uint64_t characters = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    status = vf2_model2a_read_u32(machine, UINT32_C(0x0050016c), &base);
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x005000e9), &event_flags, sizeof(event_flags)
        );
    }
    if (status != VF2_OK || event_flags != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[VF2_I960_G0_REGISTER + 1u] = 0u;
    cpu->registers[VF2_I960_G0_REGISTER + 2u] = UINT32_C(0x644);
    status = phase16_crc16(
        machine,
        base + UINT32_C(0x33a8),
        UINT32_C(0x644),
        &crc
    );
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x01d03304), crc);
    }
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[VF2_I960_G0_REGISTER] = crc;

    for (index = 0u;
         index < sizeof(set_bit3_descriptors) / sizeof(set_bit3_descriptors[0]);
         ++index) {
        status = phase16_update_descriptor(
            machine, set_bit3_descriptors[index], UINT32_C(1) << 3u, 0u,
            0u, 0
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    for (index = 0u;
         index < sizeof(first_clear_descriptors) /
            sizeof(first_clear_descriptors[0]);
         ++index) {
        status = phase16_update_descriptor(
            machine, first_clear_descriptors[index], 0u,
            UINT32_C(1) << 31u, 0u, 0
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00508000), &runtime_flags
    );
    if (status != VF2_OK || (runtime_flags & (UINT32_C(1) << 9u)) == 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    for (index = 0u;
         index < sizeof(second_clear_descriptors) /
            sizeof(second_clear_descriptors[0]);
         ++index) {
        status = phase16_update_descriptor(
            machine, second_clear_descriptors[index], 0u,
            UINT32_C(1) << 31u, 0u, 0
        );
        if (status != VF2_OK) {
            return status;
        }
    }

    cpu->registers[VF2_I960_G0_REGISTER + 4u] = base;
    cpu->registers[VF2_I960_G0_REGISTER] = UINT32_C(0x00ae101f);
    status = phase16_queue_sound_command(
        machine, cpu->registers[VF2_I960_G0_REGISTER]
    );
    if (status != VF2_OK) {
        return status;
    }
    status = phase16_update_descriptor(
        machine, UINT32_C(0x0050081c), UINT32_C(3), 0u, 0u, 0
    );
    if (status != VF2_OK) {
        return status;
    }
    for (index = 0u;
         index < sizeof(transition_clear_descriptors) /
            sizeof(transition_clear_descriptors[0]);
         ++index) {
        status = phase16_update_descriptor(
            machine, transition_clear_descriptors[index], 0u,
            UINT32_C(1) << 31u, 0u, 0
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    status = phase16_update_descriptor(
        machine, UINT32_C(0x00500830), UINT32_C(1) << 31u, 0u,
        UINT32_C(0x00029748), 1
    );
    if (status != VF2_OK) {
        return status;
    }
    status = vf2_model2a_read(
        machine, UINT32_C(0x0050002a), &phase, sizeof(phase)
    );
    ++phase;
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050002a), &phase, sizeof(phase)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x005000a5), &zero, sizeof(zero)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x005000a4), &eleven, sizeof(eleven)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    status = write_u16(machine, UINT32_C(0x00500082), UINT16_C(0x8000));
    if (status == VF2_OK) {
        uint32_t ready_flags = 0u;
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500068), &ready_flags
        );
        ready_flags |= UINT32_C(1) << 14u;
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00500068), ready_flags
            );
        }
    }
    if (status == VF2_OK) {
        status = phase16_clear_tile_plane(machine, UINT32_C(0x01004000));
    }
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x0100a004), 0u);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x0100a00c), 0u);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x0100a006), 0u);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x0100a00e), 0u);
    }
    if (status == VF2_OK) {
        status = phase16_initialize_tile_patterns(machine);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050009c), &one, sizeof(one)
        );
    }
    if (status == VF2_OK) {
        status = phase16_clear_tile_plane(machine, UINT32_C(0x01000000));
    }
    if (status != VF2_OK) {
        return status;
    }

    status = read_u16(machine, UINT32_C(0x00500082), &tile_control);
    tile_control &= UINT16_C(0xfc7f);
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x00500082), tile_control);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            UINT32_C(0x0005feac) + (uint32_t)eleven * UINT32_C(8),
            &record
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, record, &text_destination);
    }
    if (status == VF2_OK) {
        status = write_u16(
            machine, text_destination - UINT32_C(4), UINT16_C(0x801c)
        );
    }
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x00500082), tile_control);
    }
    if (status != VF2_OK) {
        return status;
    }

    for (index = 0u; index < 12u; ++index) {
        status = vf2_model2a_read_u32(
            machine,
            UINT32_C(0x0005feac) + (uint32_t)index * UINT32_C(8),
            &record
        );
        if (status == VF2_OK) {
            status = phase16_copy_text_record(
                machine, record, &last_source, &last_destination, &characters
            );
        }
        if (status != VF2_OK) {
            return status;
        }
    }
    for (index = 0u;
         index < sizeof(extra_text_records) / sizeof(extra_text_records[0]);
         ++index) {
        status = vf2_model2a_read_u32(
            machine, extra_text_records[index], &record
        );
        if (status == VF2_OK) {
            status = phase16_copy_text_record(
                machine, record, &last_source, &last_destination, &characters
            );
        }
        if (status != VF2_OK) {
            return status;
        }
    }

    status = vf2_model2a_write_u32(
        machine, UINT32_C(0x005ff640), saved_g4
    );
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[VF2_I960_G0_REGISTER] = last_source;
    cpu->registers[VF2_I960_G0_REGISTER + 1u] = 0u;
    cpu->registers[VF2_I960_G0_REGISTER + 2u] = UINT32_C(0x644);
    cpu->registers[VF2_I960_G0_REGISTER + 4u] = saved_g4;
    cpu->registers[VF2_I960_G0_REGISTER + 9u] = last_destination;
    status = vf2_model2a_write(
        machine, UINT32_C(0x005000a6), &ff, sizeof(ff)
    );
    if (status != VF2_OK) {
        return status;
    }

    account_nested_procedure(cpu, UINT64_C(26), UINT64_C(26));
    if (cpu->maximum_local_frame_depth < cpu->local_frame_depth + 4u) {
        cpu->maximum_local_frame_depth = cpu->local_frame_depth + 4u;
    }
    status = finish_recovered_procedure(machine, cpu, UINT64_C(52571));
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = characters + UINT64_C(64);
    report->bytes_written = (size_t)(characters * UINT64_C(2));
    report->recovered_instruction_count = UINT64_C(52571);
    report->recovered_procedure_calls = UINT64_C(26);
    report->recovered_procedure_returns = UINT64_C(27);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_frame_dispatch_tick(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t flags = 0u;
    uint32_t target = 0u;
    uint32_t counter = 0u;
    uint32_t selector_word = 0u;
    uint8_t selector = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00508000), &flags);
    if (status != VF2_OK || (flags & (UINT32_C(1) << 5u)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_model2a_read(
        machine, UINT32_C(0x0050002a), &selector, sizeof(selector)
    );
    if (status != VF2_OK ||
        (selector != UINT8_C(1) && selector != UINT8_C(16))) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    selector_word = UINT32_C(1) << selector;
    status = vf2_model2a_write(
        machine, UINT32_C(0x0050002b), &selector, sizeof(selector)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x0050002c), selector_word
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0000a6f8) + (uint32_t)selector * UINT32_C(4),
            &target
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if (selector == UINT8_C(16)) {
        if (target != UINT32_C(0x00010a0c)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        return execute_frame_phase16(machine, cpu, report);
    }
    if (target != UINT32_C(0x0000a974)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500024), &counter);
    if (status != VF2_OK || counter == 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    --counter;
    status = vf2_model2a_write_u32(machine, UINT32_C(0x00500024), counter);
    if (status != VF2_OK || (int32_t)counter <= 0) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
    status = finish_recovered_procedure(machine, cpu, UINT64_C(14));
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(3);
    report->bytes_written = 9u;
    report->recovered_instruction_count = UINT64_C(14);
    report->recovered_procedure_calls = UINT64_C(1);
    report->recovered_procedure_returns = UINT64_C(2);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_player_update_gate(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t flags = 0u;
    uint8_t state = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &flags);
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x00503001), &state, 1u);
    }
    if (status != VF2_OK || (flags & (UINT32_C(1) << 14u)) != 0u || state != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = finish_recovered_procedure(machine, cpu, UINT64_C(5));
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_HYBRID_BRIDGE_PLAYER_UPDATE_GATE;
    report->entry_address = VF2_PLAYER_UPDATE_GATE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->recovered_instruction_count = UINT64_C(5);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_game_state_update(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report classify_report;
    vf2_hybrid_bridge_report threshold_report;
    vf2_hybrid_bridge_report event_report;
    vf2_hybrid_bridge_report meter_report;
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint32_t runtime_flags = 0u;
    uint32_t selector_mask = 0u;
    uint32_t saved_g9 = cpu->registers[VF2_I960_G0_REGISTER + 9u];
    uint32_t base = 0u;
    uint32_t input_flags = 0u;
    uint32_t classification = 0u;
    uint32_t first_result = 0u;
    uint32_t second_result = 0u;
    uint32_t counter32 = 0u;
    uint8_t counter8 = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(&classify_report, 0, sizeof(classify_report));
    memset(&threshold_report, 0, sizeof(threshold_report));
    memset(&event_report, 0, sizeof(event_report));
    memset(&meter_report, 0, sizeof(meter_report));

    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &runtime_flags);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x0050002c), &selector_mask);
    }
    if (status != VF2_OK ||
        (runtime_flags & (UINT32_C(1) << 31u)) == 0u ||
        (selector_mask & UINT32_C(0x00030000)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[1] += UINT32_C(4);
    status = vf2_model2a_write_u32(
        machine, cpu->registers[1] - UINT32_C(4), saved_g9
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x0050016c), &base);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500704), &input_flags);
    }
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[VF2_I960_G0_REGISTER + 9u] = base;
    cpu->registers[13] = input_flags;

    if ((input_flags & ((UINT32_C(1) << 3u) |
                        (UINT32_C(1) << 27u))) == 0u) {
        status = vf2_i960_cpu_enter_procedure(
            cpu, VF2_GAME_METER_UPDATE_ENTRY, UINT32_C(0x000020e0)
        );
        if (status == VF2_OK) {
            status = execute_game_meter_update(machine, cpu, &meter_report);
        }
        if (status != VF2_OK || cpu->ip != UINT32_C(0x000020e0)) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        status = vf2_model2a_read_u32(
            machine, cpu->registers[1] - UINT32_C(4),
            &cpu->registers[VF2_I960_G0_REGISTER + 9u]
        );
        cpu->registers[1] -= UINT32_C(4);
        if (status != VF2_OK ||
            cpu->registers[VF2_I960_G0_REGISTER + 9u] != saved_g9) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        status = finish_recovered_procedure(machine, cpu, UINT64_C(17));
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_GAME_STATE_UPDATE;
        report->entry_address = VF2_GAME_STATE_UPDATE_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->changed_values = meter_report.changed_values;
        report->bytes_written = sizeof(uint32_t) + meter_report.bytes_written;
        report->recovered_instruction_count =
            cpu->executed_instructions - start_instructions;
        report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
        report->recovered_procedure_returns =
            cpu->procedure_returns - start_returns;
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_STATE_CLASSIFY_ENTRY, UINT32_C(0x00001fac)
    );
    if (status == VF2_OK) {
        status = execute_game_state_classify(machine, cpu, &classify_report);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00001fac)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    classification = cpu->registers[VF2_I960_G0_REGISTER];
    cpu->registers[3] = classification;
    if (classification == UINT32_C(3)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &runtime_flags);
    if (status != VF2_OK) {
        return status;
    }
    runtime_flags |= UINT32_C(1) << 10u;
    status = vf2_model2a_write_u32(machine, UINT32_C(0x00500068), runtime_flags);
    if (status != VF2_OK) {
        return status;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_THRESHOLD_EVALUATE_ENTRY, UINT32_C(0x00001fcc)
    );
    if (status == VF2_OK) {
        status = execute_game_threshold_evaluate(machine, cpu, &threshold_report);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00001fcc)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    first_result = cpu->registers[VF2_I960_G0_REGISTER];
    second_result = cpu->registers[VF2_I960_G0_REGISTER + 1u];
    cpu->registers[4] = first_result;
    cpu->registers[5] = second_result;
    if (classification == UINT32_C(2) || first_result == UINT32_C(1)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_model2a_read(
        machine, base + UINT32_C(0x3385), &counter8, sizeof(counter8)
    );
    ++counter8;
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x01d03385), &counter8, sizeof(counter8)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, base + UINT32_C(0x3385), &counter8, sizeof(counter8)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, base + UINT32_C(0x339c), &counter32
        );
    }
    ++counter32;
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x01d0339c), counter32
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, base + UINT32_C(0x339c), counter32
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[VF2_I960_G0_REGISTER] = UINT32_C(0x009e0a7f);
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_EVENT_QUEUE_WRITE_ENTRY, UINT32_C(0x00002020)
    );
    if (status == VF2_OK) {
        status = execute_game_event_queue_write(machine, cpu, &event_report);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00002020)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_METER_UPDATE_ENTRY, UINT32_C(0x000020e0)
    );
    if (status == VF2_OK) {
        status = execute_game_meter_update(machine, cpu, &meter_report);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x000020e0)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = vf2_model2a_read_u32(
        machine, cpu->registers[1] - UINT32_C(4),
        &cpu->registers[VF2_I960_G0_REGISTER + 9u]
    );
    cpu->registers[1] -= UINT32_C(4);
    if (status != VF2_OK ||
        cpu->registers[VF2_I960_G0_REGISTER + 9u] != saved_g9) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = finish_recovered_procedure(machine, cpu, UINT64_C(37));
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_HYBRID_BRIDGE_GAME_STATE_UPDATE;
    report->entry_address = VF2_GAME_STATE_UPDATE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(4);
    report->bytes_written = sizeof(uint32_t) * 5u + sizeof(uint8_t) * 2u +
        classify_report.bytes_written + threshold_report.bytes_written +
        event_report.bytes_written + meter_report.bytes_written;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_game_threshold_evaluate(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report color0_report;
    vf2_hybrid_bridge_report color1_report;
    vf2_hybrid_bridge_report classify_report;
    uint32_t base = 0u;
    uint32_t flags = 0u;
    uint32_t color0 = 0u;
    uint32_t color1 = 0u;
    uint32_t quotient0 = 0u;
    uint32_t quotient1 = 0u;
    uint8_t mode = 0u;
    uint8_t variant = 0u;
    uint8_t numerator0 = 0u;
    uint8_t numerator1 = 0u;
    uint8_t offset0 = 0u;
    uint8_t offset1 = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(&color0_report, 0, sizeof(color0_report));
    memset(&color1_report, 0, sizeof(color1_report));
    memset(&classify_report, 0, sizeof(classify_report));

    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0050016c), &base
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3324), &mode, sizeof(mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3350), &variant, sizeof(variant)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3380), &numerator0,
            sizeof(numerator0)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3388), &numerator1,
            sizeof(numerator1)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3385), &offset0, sizeof(offset0)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x338d), &offset1, sizeof(offset1)
        );
    }
    if (status != VF2_OK || mode == UINT8_C(25) || variant == UINT8_C(1)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[11] = UINT32_C(9);
    cpu->registers[16] = 0u;
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_COLOR_LOOKUP_ENTRY, UINT32_C(0x00002938)
    );
    if (status == VF2_OK) {
        status = execute_game_color_lookup(
            machine, cpu, &color0_report
        );
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00002938)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    color0 = cpu->registers[16];

    cpu->registers[16] = 1u;
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_COLOR_LOOKUP_ENTRY, UINT32_C(0x00002944)
    );
    if (status == VF2_OK) {
        status = execute_game_color_lookup(
            machine, cpu, &color1_report
        );
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00002944)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    color1 = cpu->registers[16];
    color0 = (color0 << 8u) >> 24u;
    color1 = (color1 << 8u) >> 24u;
    if (color0 == 0u || color1 == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_STATE_CLASSIFY_ENTRY, UINT32_C(0x0000295c)
    );
    if (status == VF2_OK) {
        status = execute_game_state_classify(
            machine, cpu, &classify_report
        );
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000295c) ||
        cpu->registers[16] == UINT32_C(2) ||
        cpu->registers[16] == UINT32_C(3)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_model2a_read_u32(
        machine, base + UINT32_C(0x3320), &flags
    );
    if (status != VF2_OK || (flags & (UINT32_C(1) << 1u)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    quotient0 = (uint32_t)numerator0 / color0;
    quotient1 = (uint32_t)numerator1 / color1;
    if (quotient0 + (uint32_t)offset0 + quotient1 +
            (uint32_t)offset1 >= UINT32_C(9)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    cpu->registers[16] = 0u;
    cpu->registers[17] = 0u;
    set_equal_condition(cpu);
    status = finish_recovered_procedure(machine, cpu, UINT64_C(38));
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_GAME_THRESHOLD_EVALUATE;
    report->entry_address = VF2_GAME_THRESHOLD_EVALUATE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(2);
    report->bytes_written =
        color0_report.bytes_written + color1_report.bytes_written;
    report->recovered_instruction_count =
        UINT64_C(38) + color0_report.recovered_instruction_count +
        color1_report.recovered_instruction_count +
        classify_report.recovered_instruction_count;
    report->recovered_procedure_calls =
        UINT64_C(3) + color0_report.recovered_procedure_calls +
        color1_report.recovered_procedure_calls +
        classify_report.recovered_procedure_calls;
    report->recovered_procedure_returns =
        UINT64_C(1) + color0_report.recovered_procedure_returns +
        color1_report.recovered_procedure_returns +
        classify_report.recovered_procedure_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_inline_text_thunk(
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

vf2_status execute_texture_status_line(
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

vf2_status execute_texture_address_table(
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


vf2_status execute_frame_timer_prefix(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t timer_low = 0u;
    uint32_t timer_high = 0u;
    uint32_t frame_counter = 0u;
    uint32_t previous_minimum = 0u;
    uint8_t frame_byte = 0u;
    uint8_t mode = 0u;
    vf2_status status = VF2_OK;

    status = vf2_model2a_read_u32(machine, UINT32_C(0x00f00008), &timer_low);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00f0000c), &timer_high);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x00500000), &frame_byte, 1u);
    }
    if (status != VF2_OK || frame_byte != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[14] = UINT32_C(0x000fffff);
    cpu->registers[15] =
        UINT32_C(0x000fffff) - (timer_high & UINT32_C(0x000fffff));
    cpu->registers[15] -= UINT32_C(18);
    cpu->registers[3] = cpu->registers[15] / UINT32_C(25);
    cpu->registers[6] = frame_byte;
    cpu->registers[4] = UINT32_C(0x000043db);
    cpu->registers[5] = cpu->registers[4] - cpu->registers[3];
    status = vf2_model2a_write_u32(
        machine, UINT32_C(0x0050015c), cpu->registers[5]
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500020), &frame_counter
        );
    }
    if (status != VF2_OK || (frame_counter & UINT32_C(31)) == 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00500160), &previous_minimum
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500160), cpu->registers[5]
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x0050006d), &mode, 1u);
    }
    if (status != VF2_OK || mode == UINT8_C(1) || mode == UINT8_C(2)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[3] = previous_minimum;
    cpu->registers[VF2_I960_G0_REGISTER] = frame_byte;
    (void)timer_low;
    finish_recovered_control_block(cpu, UINT32_C(0x00010f90), UINT64_C(23));
    report->kind = VF2_HYBRID_BRIDGE_FRAME_TIMER_PREFIX;
    report->entry_address = VF2_FRAME_TIMER_PREFIX_ENTRY;
    report->exit_address = cpu->ip;
    report->changed_values = UINT64_C(2);
    report->bytes_written = sizeof(uint32_t) * 2u;
    report->recovered_instruction_count = UINT64_C(23);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_interrupt_save_prefix(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t saved_sp = cpu->registers[1];
    uint32_t runtime_flags = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    cpu->registers[3] = saved_sp;
    cpu->registers[1] += UINT32_C(64);
    for (index = 0u; index < 16u; ++index) {
        status = vf2_model2a_write_u32(
            machine, saved_sp + (uint32_t)index * UINT32_C(4),
            cpu->registers[VF2_I960_G0_REGISTER + index]
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    cpu->registers[VF2_I960_G0_REGISTER + 10u] = UINT32_C(1) << 23u;
    cpu->registers[VF2_I960_G0_REGISTER + 11u] = UINT32_C(17) << 19u;
    cpu->registers[VF2_I960_G0_REGISTER + 12u] = UINT32_C(1) << 14u;
    cpu->registers[15] = UINT32_C(0x000fffff);
    status = vf2_model2a_write_u32(
        machine, UINT32_C(0x00f00008), cpu->registers[15]
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500068), &runtime_flags
        );
    }
    if (status != VF2_OK || (runtime_flags & (UINT32_C(1) << 31u)) == 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[15] = runtime_flags;
    finish_recovered_control_block(cpu, UINT32_C(0x00000c00), UINT64_C(13));
    report->kind = VF2_HYBRID_BRIDGE_INTERRUPT_SAVE_PREFIX;
    report->entry_address = VF2_INTERRUPT_SAVE_PREFIX_ENTRY;
    report->exit_address = cpu->ip;
    report->changed_values = UINT64_C(19);
    report->bytes_written = 16u * sizeof(uint32_t) + sizeof(uint32_t);
    report->recovered_instruction_count = UINT64_C(13);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_interrupt_buffer_gate(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t pointer = 0u;
    uint32_t runtime_flags = 0u;
    uint8_t first = 0u;
    uint8_t second = 0u;
    vf2_status status = VF2_OK;

    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500804), &pointer);
    cpu->registers[3] = pointer;
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, pointer + UINT32_C(0x69c), &first, 1u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, pointer + UINT32_C(0x69d), &second, 1u);
    }
    cpu->registers[13] = first;
    cpu->registers[14] = second;
    if (status != VF2_OK || first != second) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500808), &pointer);
    cpu->registers[3] = pointer;
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, pointer + UINT32_C(0x69c), &first, 1u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, pointer + UINT32_C(0x69d), &second, 1u);
    }
    cpu->registers[13] = first;
    cpu->registers[14] = second;
    if (status != VF2_OK || first != second) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00508000), &runtime_flags);
    if (status != VF2_OK || (runtime_flags & (UINT32_C(1) << 13u)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[15] = runtime_flags;
    finish_recovered_control_block(cpu, UINT32_C(0x00000c78), UINT64_C(10));
    report->kind = VF2_HYBRID_BRIDGE_INTERRUPT_BUFFER_GATE;
    report->entry_address = VF2_INTERRUPT_BUFFER_GATE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(2);
    report->recovered_instruction_count = UINT64_C(10);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_interrupt_input_ring(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report ring_report;
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    const uint32_t saved_g0 = cpu->registers[VF2_I960_G0_REGISTER];
    vf2_status status = VF2_OK;

    memset(&ring_report, 0, sizeof(ring_report));
    cpu->registers[1] += UINT32_C(4);
    status = vf2_model2a_write_u32(
        machine, cpu->registers[1] - UINT32_C(4), saved_g0
    );
    if (status == VF2_OK) {
        status = vf2_i960_cpu_enter_procedure(
            cpu, VF2_INPUT_RING_POLL_ENTRY, UINT32_C(0x00000ca4)
        );
    }
    if (status == VF2_OK) {
        status = execute_input_ring_poll(machine, cpu, &ring_report);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00000ca4) ||
        cpu->registers[VF2_I960_G0_REGISTER] != UINT32_MAX) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[15] = UINT32_MAX;
    status = vf2_model2a_read_u32(
        machine, cpu->registers[1] - UINT32_C(4),
        &cpu->registers[VF2_I960_G0_REGISTER]
    );
    cpu->registers[1] -= UINT32_C(4);
    if (status != VF2_OK || cpu->registers[VF2_I960_G0_REGISTER] != saved_g0) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    finish_recovered_control_block(cpu, UINT32_C(0x00000cd4), UINT64_C(7));
    report->kind = VF2_HYBRID_BRIDGE_INTERRUPT_INPUT_RING;
    report->entry_address = VF2_INTERRUPT_INPUT_RING_ENTRY;
    report->exit_address = cpu->ip;
    report->bytes_written = sizeof(uint32_t) + ring_report.bytes_written;
    report->recovered_instruction_count = cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_interrupt_restore_prefix(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t saved_sp = cpu->registers[1] - UINT32_C(64);
    uint8_t frame_byte = 0u;
    size_t index = 0u;
    vf2_status status = vf2_model2a_read(
        machine, UINT32_C(0x00500000), &frame_byte, 1u
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[5] = (uint32_t)(int32_t)(int8_t)frame_byte;
    cpu->registers[5] += UINT32_C(1);
    frame_byte = (uint8_t)cpu->registers[5];
    status = vf2_model2a_write(machine, UINT32_C(0x00500000), &frame_byte, 1u);
    cpu->registers[3] = saved_sp;
    for (index = 0u; status == VF2_OK && index < 16u; ++index) {
        status = vf2_model2a_read_u32(
            machine, saved_sp + (uint32_t)index * UINT32_C(4),
            &cpu->registers[VF2_I960_G0_REGISTER + index]
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[1] = saved_sp;
    cpu->registers[4] = UINT32_C(0x00e80000);
    cpu->registers[5] = UINT32_C(0xfffffffe);
    status = vf2_model2a_write_u32(
        machine, cpu->registers[4], cpu->registers[5]
    );
    if (status != VF2_OK) {
        return status;
    }
    finish_recovered_control_block(cpu, UINT32_C(0x00000d20), UINT64_C(12));
    report->kind = VF2_HYBRID_BRIDGE_INTERRUPT_RESTORE_PREFIX;
    report->entry_address = VF2_INTERRUPT_RESTORE_PREFIX_ENTRY;
    report->exit_address = cpu->ip;
    report->changed_values = UINT64_C(18);
    report->bytes_written = sizeof(uint8_t) + sizeof(uint32_t);
    report->recovered_instruction_count = UINT64_C(12);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_frame_timer_suffix(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report latch_report;
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint32_t runtime_flags = 0u;
    uint32_t video_status = 0u;
    uint8_t frame_byte = 0u;
    vf2_status status = VF2_OK;

    memset(&latch_report, 0, sizeof(latch_report));
    status = vf2_model2a_read(machine, UINT32_C(0x00500000), &frame_byte, 1u);
    if (status != VF2_OK || frame_byte >= UINT8_C(2)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[4] = frame_byte;
    cpu->registers[3] = 0u;
    frame_byte = 0u;
    status = vf2_model2a_write(machine, UINT32_C(0x00500000), &frame_byte, 1u);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00980014), &video_status);
    }
    cpu->registers[3] = video_status & UINT32_C(15);
    if (status != VF2_OK) {
        return status;
    }
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_VIDEO_STATUS_LATCH_ENTRY, UINT32_C(0x00010fe4)
    );
    if (status == VF2_OK) {
        status = execute_video_status_latch(machine, cpu, &latch_report);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00010fe4)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00508000), &runtime_flags);
    if (status != VF2_OK || (runtime_flags & (UINT32_C(1) << 9u)) == 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[15] = runtime_flags;
    status = finish_recovered_procedure(machine, cpu, UINT64_C(11));
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_HYBRID_BRIDGE_FRAME_TIMER_SUFFIX;
    report->entry_address = VF2_FRAME_TIMER_SUFFIX_ENTRY;
    report->exit_address = cpu->ip;
    report->changed_values = UINT64_C(2);
    report->bytes_written = sizeof(uint8_t) + latch_report.bytes_written;
    report->recovered_instruction_count = cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_interrupt_player_layer(
    vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report)
{
    vf2_hybrid_bridge_report a={0}, b={0};
    uint64_t i=cpu->executed_instructions,c=cpu->procedure_calls,r=cpu->procedure_returns;
    vf2_status status=vf2_i960_cpu_enter_procedure(cpu,VF2_PLAYER_UPDATE_GATE_ENTRY,UINT32_C(0x00000c7c));
    if(status==VF2_OK) status=execute_player_update_gate(machine,cpu,&a);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x00000c7c)) return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    status=vf2_i960_cpu_enter_procedure(cpu,VF2_VIDEO_LAYER_COMMIT_ENTRY,UINT32_C(0x00000c80));
    if(status==VF2_OK) status=execute_video_layer_commit(machine,cpu,&b);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x00000c80)) return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    cpu->executed_instructions+=UINT64_C(2);
    report->kind=VF2_HYBRID_BRIDGE_INTERRUPT_PLAYER_LAYER; report->entry_address=VF2_INTERRUPT_PLAYER_LAYER_ENTRY; report->exit_address=cpu->ip;
    report->bytes_written=a.bytes_written+b.bytes_written; report->recovered_instruction_count=cpu->executed_instructions-i; report->recovered_procedure_calls=cpu->procedure_calls-c; report->recovered_procedure_returns=cpu->procedure_returns-r; report->cpu_poststate_applied=1; return VF2_OK;
}

vf2_status execute_interrupt_game_input(
    vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report)
{
    vf2_hybrid_bridge_report nested={0}; uint64_t i=cpu->executed_instructions,c=cpu->procedure_calls,r=cpu->procedure_returns; uint32_t flags=0u;
    vf2_status status=vf2_model2a_read_u32(machine,UINT32_C(0x00500068),&flags);
    if(status!=VF2_OK||(flags&(UINT32_C(1)<<31u))==0u) return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    cpu->registers[15]=flags;
    status=vf2_i960_cpu_enter_procedure(cpu,VF2_GAME_INPUT_UPDATE_ENTRY,UINT32_C(0x00000c90));
    if(status==VF2_OK) status=execute_game_input_update(machine,cpu,&nested);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x00000c90)) return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    cpu->executed_instructions+=UINT64_C(3);
    report->kind=VF2_HYBRID_BRIDGE_INTERRUPT_GAME_INPUT; report->entry_address=VF2_INTERRUPT_GAME_INPUT_ENTRY; report->exit_address=cpu->ip; report->bytes_written=nested.bytes_written; report->recovered_instruction_count=cpu->executed_instructions-i; report->recovered_procedure_calls=cpu->procedure_calls-c; report->recovered_procedure_returns=cpu->procedure_returns-r; report->cpu_poststate_applied=1; return VF2_OK;
}

vf2_status execute_interrupt_game_state(
    vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report)
{
    vf2_hybrid_bridge_report nested={0}; uint64_t i=cpu->executed_instructions,c=cpu->procedure_calls,r=cpu->procedure_returns;
    vf2_status status=vf2_i960_cpu_enter_procedure(cpu,VF2_GAME_STATE_UPDATE_ENTRY,UINT32_C(0x00000c94));
    if(status==VF2_OK) status=execute_game_state_update(machine,cpu,&nested);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x00000c94)) return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    cpu->executed_instructions+=UINT64_C(1);
    report->kind=VF2_HYBRID_BRIDGE_INTERRUPT_GAME_STATE; report->entry_address=VF2_INTERRUPT_GAME_STATE_ENTRY; report->exit_address=cpu->ip; report->bytes_written=nested.bytes_written; report->recovered_instruction_count=cpu->executed_instructions-i; report->recovered_procedure_calls=cpu->procedure_calls-c; report->recovered_procedure_returns=cpu->procedure_returns-r; report->cpu_poststate_applied=1; return VF2_OK;
}

vf2_status execute_interrupt_tile_sync(
    vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report)
{
    vf2_hybrid_bridge_report a={0},b={0},d={0}; uint64_t i=cpu->executed_instructions,c=cpu->procedure_calls,r=cpu->procedure_returns;
    vf2_status status=vf2_i960_cpu_enter_procedure(cpu,VF2_TILE_RUNTIME_GATE_ENTRY,UINT32_C(0x00000cd8));
    if(status==VF2_OK) status=execute_tile_runtime_gate(machine,cpu,&a);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x00000cd8)) return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    status=vf2_i960_cpu_enter_procedure(cpu,VF2_TILE_CONTROLLER_UPDATE_ENTRY,UINT32_C(0x00000cdc));
    if(status==VF2_OK) status=execute_tile_controller_update(machine,cpu,&b);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x00000cdc)) return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    status=vf2_i960_cpu_enter_procedure(cpu,VF2_VIDEO_INPUT_SYNC_ENTRY,UINT32_C(0x00000ce0));
    if(status==VF2_OK) status=execute_video_input_sync(machine,cpu,&d);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x00000ce0)) return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    cpu->executed_instructions+=UINT64_C(3);
    report->kind=VF2_HYBRID_BRIDGE_INTERRUPT_TILE_SYNC; report->entry_address=VF2_INTERRUPT_TILE_SYNC_ENTRY; report->exit_address=cpu->ip; report->bytes_written=a.bytes_written+b.bytes_written+d.bytes_written; report->recovered_instruction_count=cpu->executed_instructions-i; report->recovered_procedure_calls=cpu->procedure_calls-c; report->recovered_procedure_returns=cpu->procedure_returns-r; report->cpu_poststate_applied=1; return VF2_OK;
}

vf2_status execute_main_post_timer(
    vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report)
{
    vf2_hybrid_bridge_report a={0},b={0},d={0}; uint64_t i=cpu->executed_instructions,c=cpu->procedure_calls,r=cpu->procedure_returns;
    vf2_status status=vf2_i960_cpu_enter_procedure(cpu,VF2_SYSTEM_MEMORY_DIAGNOSTIC_ENTRY,UINT32_C(0x0000a03c));
    if(status==VF2_OK) status=execute_system_memory_diagnostic(machine,cpu,&a);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x0000a03c)) return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    status=vf2_i960_cpu_enter_procedure(cpu,VF2_FRAME_COUNTER_ADVANCE_ENTRY,UINT32_C(0x0000a040));
    if(status==VF2_OK) status=execute_frame_counter_advance(machine,cpu,&b);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x0000a040)) return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    status=vf2_i960_cpu_enter_procedure(cpu,VF2_FRAME_PHASE_ADVANCE_ENTRY,UINT32_C(0x0000a044));
    if(status==VF2_OK) status=execute_frame_phase_advance(machine,cpu,&d);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x0000a044)) return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    finish_recovered_control_block(cpu,UINT32_C(0x00009fb0),UINT64_C(4));
    report->kind=VF2_HYBRID_BRIDGE_MAIN_POST_TIMER; report->entry_address=VF2_MAIN_POST_TIMER_ENTRY; report->exit_address=cpu->ip; report->bytes_written=a.bytes_written+b.bytes_written+d.bytes_written; report->recovered_instruction_count=cpu->executed_instructions-i; report->recovered_procedure_calls=cpu->procedure_calls-c; report->recovered_procedure_returns=cpu->procedure_returns-r; report->cpu_poststate_applied=1; return VF2_OK;
}

vf2_status execute_main_clear_prefix(
    vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report)
{
    static const uint32_t addresses[5]={UINT32_C(0x0050101c),UINT32_C(0x005010d0),UINT32_C(0x00501010),UINT32_C(0x00501014),UINT32_C(0x00501998)};
    uint32_t flags=0u; size_t n=0u; vf2_status status=VF2_OK; cpu->registers[3]=0u;
    for(n=0;n<5u;++n){status=vf2_model2a_write_u32(machine,addresses[n],0u); if(status!=VF2_OK)return status;}
    cpu->registers[15]=UINT32_C(0x000fffff); status=vf2_model2a_write_u32(machine,UINT32_C(0x00f00000),cpu->registers[15]);
    if(status==VF2_OK) status=vf2_model2a_read_u32(machine,UINT32_C(0x00508000),&flags);
    if(status!=VF2_OK||(flags&(UINT32_C(1)<<13u))!=0u)return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    cpu->registers[15]=flags; finish_recovered_control_block(cpu,UINT32_C(0x00009ff8),UINT64_C(10));
    report->kind=VF2_HYBRID_BRIDGE_MAIN_CLEAR_PREFIX; report->entry_address=VF2_MAIN_CLEAR_PREFIX_ENTRY; report->exit_address=cpu->ip; report->changed_values=UINT64_C(6); report->bytes_written=sizeof(uint32_t)*6u; report->recovered_instruction_count=UINT64_C(10); report->cpu_poststate_applied=1; return VF2_OK;
}

vf2_status execute_main_final_cluster(
    vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report)
{
    vf2_hybrid_bridge_report a={0},b={0},d={0},e={0},f={0}; uint64_t i=cpu->executed_instructions,c=cpu->procedure_calls,r=cpu->procedure_returns;
    vf2_status status=vf2_i960_cpu_enter_procedure(cpu,VF2_FRAME_SHADOW_VERIFY_ENTRY,UINT32_C(0x00009ffc));
    if(status==VF2_OK)status=execute_frame_shadow_verify(machine,cpu,&a);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x00009ffc))return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    status=vf2_i960_cpu_enter_procedure(cpu,UINT32_C(0x00029744),UINT32_C(0x0000a000));
    if(status==VF2_OK)status=vf2_i960_cpu_return_procedure(cpu,machine);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x0000a000))return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    status=vf2_i960_cpu_enter_procedure(cpu,VF2_FRAME_BUFFER_GATE_ENTRY,UINT32_C(0x0000a004)); if(status==VF2_OK)status=execute_frame_buffer_gate(machine,cpu,&b);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x0000a004))return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    status=vf2_i960_cpu_enter_procedure(cpu,VF2_GEOMETRY_COMMAND_SETUP_ENTRY,UINT32_C(0x0000a008)); if(status==VF2_OK)status=execute_geometry_command_setup(machine,cpu,&d);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x0000a008))return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    status=vf2_i960_cpu_enter_procedure(cpu,VF2_FRAME_SCRATCH_CLEAR_ENTRY,UINT32_C(0x0000a00c)); if(status==VF2_OK)status=execute_frame_scratch_clear(machine,cpu,&e);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x0000a00c))return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    status=vf2_i960_cpu_enter_procedure(cpu,VF2_FRAME_DISPATCH_TICK_ENTRY,UINT32_C(0x0000a010)); if(status==VF2_OK)status=execute_frame_dispatch_tick(machine,cpu,&f);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x0000a010))return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    cpu->executed_instructions+=UINT64_C(7);
    report->kind=VF2_HYBRID_BRIDGE_MAIN_FINAL_CLUSTER; report->entry_address=VF2_MAIN_FINAL_CLUSTER_ENTRY; report->exit_address=cpu->ip; report->bytes_written=a.bytes_written+b.bytes_written+d.bytes_written+e.bytes_written+f.bytes_written; report->recovered_instruction_count=cpu->executed_instructions-i; report->recovered_procedure_calls=cpu->procedure_calls-c; report->recovered_procedure_returns=cpu->procedure_returns-r; report->cpu_poststate_applied=1; return VF2_OK;
}

vf2_status execute_main_geometry_prefix(
    vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report)
{
    vf2_hybrid_bridge_report a={0},b={0}; uint64_t i=cpu->executed_instructions,c=cpu->procedure_calls,r=cpu->procedure_returns; uint32_t counter=0u;
    vf2_status status=vf2_i960_cpu_enter_procedure(cpu,VF2_GEOMETRY_FRAME_COMMIT_ENTRY,UINT32_C(0x0000a018)); if(status==VF2_OK)status=execute_geometry_frame_commit(machine,cpu,&a);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x0000a018))return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    status=vf2_i960_cpu_enter_procedure(cpu,VF2_FRAME_GEOMETRY_GATE_ENTRY,UINT32_C(0x0000a01c)); if(status==VF2_OK)status=execute_frame_geometry_gate(machine,cpu,&b);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x0000a01c))return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    status=vf2_model2a_read_u32(machine,UINT32_C(0x00500020),&counter); cpu->registers[14]=counter; cpu->registers[15]=counter+UINT32_C(1);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500020), cpu->registers[15]
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    finish_recovered_control_block(cpu,UINT32_C(0x0000a030),UINT64_C(5));
    report->kind=VF2_HYBRID_BRIDGE_MAIN_GEOMETRY_PREFIX; report->entry_address=VF2_MAIN_GEOMETRY_PREFIX_ENTRY; report->exit_address=cpu->ip; report->bytes_written=a.bytes_written+b.bytes_written+sizeof(uint32_t); report->recovered_instruction_count=cpu->executed_instructions-i; report->recovered_procedure_calls=cpu->procedure_calls-c; report->recovered_procedure_returns=cpu->procedure_returns-r; report->cpu_poststate_applied=1; return VF2_OK;
}

vf2_status execute_main_texture_orchestrator_call(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report nested_report = {0};
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    vf2_status status = vf2_i960_cpu_enter_procedure(
        cpu,
        VF2_TEXTURE_ORCHESTRATOR_SAVE_ENTRY,
        UINT32_C(0x0000a034)
    );

    if (status == VF2_OK) {
        status = execute_texture_orchestrator_save_call(
            machine, cpu, &nested_report
        );
    }
    if (status != VF2_OK ||
        cpu->ip != VF2_TEXTURE_ORCHESTRATOR_BODY_ENTRY) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->executed_instructions += UINT64_C(1);

    report->kind = VF2_HYBRID_BRIDGE_MAIN_TEXTURE_ORCHESTRATOR_CALL;
    report->entry_address = VF2_MAIN_TEXTURE_ORCHESTRATOR_CALL_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = nested_report.iterations;
    report->bytes_written = nested_report.bytes_written;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_main_frame_timer_call(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report nested_report = {0};
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    vf2_status status = vf2_i960_cpu_enter_procedure(
        cpu,
        VF2_FRAME_TIMER_PREFIX_ENTRY,
        UINT32_C(0x0000a038)
    );

    if (status == VF2_OK) {
        status = execute_frame_timer_prefix(machine, cpu, &nested_report);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00010f90)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->executed_instructions += UINT64_C(1);

    report->kind = VF2_HYBRID_BRIDGE_MAIN_FRAME_TIMER_CALL;
    report->entry_address = VF2_MAIN_FRAME_TIMER_CALL_ENTRY;
    report->exit_address = cpu->ip;
    report->changed_values = nested_report.changed_values;
    report->bytes_written = nested_report.bytes_written;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_interrupt_initial_cluster(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report compose_report = {0};
    vf2_hybrid_bridge_report latch_report = {0};
    vf2_hybrid_bridge_report dispatch_report = {0};
    vf2_hybrid_bridge_report upload_report = {0};
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    vf2_status status = vf2_i960_cpu_enter_procedure(
        cpu,
        VF2_VIDEO_REGISTER_COMPOSE_ENTRY,
        UINT32_C(0x00000c04)
    );

    if (status == VF2_OK) {
        status = execute_video_register_compose(
            machine, cpu, &compose_report
        );
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00000c04)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu,
        VF2_VIDEO_INPUT_LATCH_WRITE_ENTRY,
        UINT32_C(0x00000c08)
    );
    if (status == VF2_OK) {
        status = execute_video_input_latch_write(
            machine, cpu, &latch_report
        );
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00000c08)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu,
        VF2_TEXTURE_UPLOAD_DISPATCH_ENTRY,
        UINT32_C(0x00000c0c)
    );
    if (status == VF2_OK) {
        status = execute_texture_upload_dispatch(
            machine, cpu, &dispatch_report
        );
    }
    if (status != VF2_OK || cpu->ip != VF2_PALETTE_PAGE_UPLOAD_ENTRY) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = execute_palette_page_upload(machine, cpu, &upload_report);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0004bab4)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00000c0c)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->executed_instructions += UINT64_C(4);

    report->kind = VF2_HYBRID_BRIDGE_INTERRUPT_INITIAL_CLUSTER;
    report->entry_address = VF2_INTERRUPT_INITIAL_CLUSTER_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = upload_report.iterations;
    report->rows = upload_report.rows;
    report->bytes_written =
        compose_report.bytes_written + latch_report.bytes_written +
        dispatch_report.bytes_written + upload_report.bytes_written;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}
