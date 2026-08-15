#include "texture_bridge_internal.h"
#include "recovery_internal.h"
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
    if (status != VF2_OK) {
        return status;
    }
    if (frame_mode == UINT8_C(16) || frame_mode == UINT8_C(17)) {
        const uint64_t instructions = frame_mode == UINT8_C(17)
            ? UINT64_C(67)
            : UINT64_C(66);

        set_equal_condition(cpu);
        account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
        status = finish_recovered_procedure(machine, cpu, instructions);
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_SYSTEM_MEMORY_DIAGNOSTIC;
        report->entry_address = VF2_SYSTEM_MEMORY_DIAGNOSTIC_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(4);
        report->bytes_written = 16u;
        report->recovered_instruction_count = instructions;
        report->recovered_procedure_calls = UINT64_C(1);
        report->recovered_procedure_returns = UINT64_C(2);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
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
    uint32_t sample_index = 0u;
    uint32_t timing_base = 0u;
    uint32_t timing_slot = 0u;
    uint32_t average = 0u;
    uint32_t sample_sum = 0u;
    uint16_t sample = 0u;
    uint16_t slot_value = 0u;
    uint8_t shift = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x0050002c), &mode);
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[4] = mode;
    cpu->registers[3] = mode & UINT32_C(0x00030000);
    if (cpu->registers[3] != 0u) {
        status = finish_recovered_procedure(machine, cpu, UINT64_C(5));
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_FRAME_COUNTER_ADVANCE;
        report->entry_address = VF2_FRAME_COUNTER_ADVANCE_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->recovered_instruction_count = UINT64_C(5);
        report->recovered_procedure_returns = UINT64_C(1);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
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
    if (status != VF2_OK || shift >= UINT8_C(27)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    mask = (UINT32_C(1) << shift) - UINT32_C(1);
    status = vf2_model2a_read_u32(machine, UINT32_C(0x0050002c), &mode);
    if (status != VF2_OK) {
        return status;
    }
    sample_index = (counter >> shift) & UINT32_C(0xff);
    if ((counter & mask) == 0u) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050016c), &timing_base
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, timing_base + UINT32_C(0x3342), &timing_slot,
                sizeof(uint8_t)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        for (sample_index = 0u; sample_index < UINT32_C(256); ++sample_index) {
            status = vf2_model2a_read(
                machine, UINT32_C(0x0050d100) + sample_index * 2u,
                &sample, sizeof(sample)
            );
            if (status != VF2_OK) {
                return status;
            }
            {
                const uint32_t extended_sample =
                    (sample & UINT16_C(0x8000)) != 0u
                        ? UINT32_C(0xffff0000) | (uint32_t)sample
                        : (uint32_t)sample;
                sample_sum += extended_sample;
            }
        }
        average = sample_sum >> (shift + UINT32_C(5));
        if (average > UINT32_C(7)) {
            average = UINT32_C(7);
        }
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050d004), &average, sizeof(uint8_t)
        );
        if (status == VF2_OK) {
            average = sample_sum >> shift;
            if (average > UINT32_C(0xff)) {
                average = UINT32_C(0xff);
            }
            status = vf2_model2a_write(
                machine, UINT32_C(0x01d03000), &average, sizeof(uint8_t)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x0059c000), &average, sizeof(uint8_t)
            );
        }
        if (status == VF2_OK) {
            timing_slot = UINT32_C(0x00011510) + average + timing_slot * 8u;
            status = vf2_model2a_read(
                machine, timing_slot, &sample, sizeof(uint8_t)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x0050d005), &sample, sizeof(uint8_t)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        sample = 0u;
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050d100) + ((counter >> shift) & 0xffu) * 2u,
            &sample, sizeof(sample)
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    if ((mode & UINT32_C(0x000cffc0)) != 0u) {
        status = finish_recovered_procedure(machine, cpu, UINT64_C(5));
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_FRAME_COUNTER_ADVANCE;
        report->entry_address = VF2_FRAME_COUNTER_ADVANCE_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->recovered_instruction_count = UINT64_C(5);
        report->recovered_procedure_returns = UINT64_C(1);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
    if ((counter & mask) == 0u) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x0050d100) + ((counter >> shift) & 0xffu) * 2u,
            &slot_value, sizeof(slot_value)
        );
        if (status == VF2_OK) {
            slot_value = (uint16_t)(slot_value + UINT16_C(1));
            status = vf2_model2a_write(
                machine,
                UINT32_C(0x0050d100) + ((counter >> shift) & 0xffu) * 2u,
                &slot_value, sizeof(slot_value)
            );
        }
    }
    if (status != VF2_OK) {
        return status;
    }
    status = finish_recovered_procedure(
        machine, cpu, (counter & mask) == 0u ? UINT64_C(75) : UINT64_C(21)
    );
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_FRAME_COUNTER_ADVANCE;
    report->entry_address = VF2_FRAME_COUNTER_ADVANCE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = (counter & mask) == 0u ? UINT64_C(6) : UINT64_C(1);
    report->bytes_written = (counter & mask) == 0u ? 6u : 4u;
    report->recovered_instruction_count =
        (counter & mask) == 0u ? UINT64_C(75) : UINT64_C(21);
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

static vf2_status compute_table_crc16(
    const vf2_model2a *machine,
    uint32_t source,
    uint32_t count,
    uint16_t *result
)
{
    return vf2_recovered_table_crc16(machine, source, 0u, count, result);
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

static vf2_status fill_tile_plane_spaces(
    vf2_model2a *machine,
    uint32_t base,
    uint32_t columns,
    uint32_t rows
)
{
    uint32_t row = 0u;
    uint32_t column = 0u;

    for (row = 0u; row < rows; ++row) {
        for (column = 0u; column < columns; ++column) {
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

static vf2_status clear_tile_plane_64x48(
    vf2_model2a *machine,
    uint32_t base
)
{
    return fill_tile_plane_spaces(
        machine, base, UINT32_C(64), UINT32_C(48)
    );
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
    status = compute_table_crc16(
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
        status = clear_tile_plane_64x48(machine, UINT32_C(0x01004000));
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
        status = clear_tile_plane_64x48(machine, UINT32_C(0x01000000));
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

static vf2_status execute_frame_phase17_bit7_index11(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    uint32_t saved_g4,
    uint8_t flagged_phase_index
)
{
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    const uint32_t frame_dispatch_return =
        cpu->local_frames[cpu->local_frame_depth - 1u].registers[2];
    vf2_hybrid_bridge_report meter_report;
    uint32_t indirect_target = 0u;
    uint32_t base = 0u;
    uint32_t saved_g9 = 0u;
    uint32_t player0 = 0u;
    uint32_t player1 = 0u;
    uint32_t screen_record = 0u;
    uint32_t screen_source = 0u;
    uint32_t screen_destination = 0u;
    uint32_t base_flags = 0u;
    uint16_t crc = 0u;
    uint64_t characters = 0u;
    uint64_t accounted = 0u;
    uint8_t mode = 0u;
    uint8_t phase_a5 = 0u;
    uint8_t system_flags = 0u;
    int first_visit = 0;
    int terminal_reset = 0;
    uint64_t expected_instructions = 0u;
    uint64_t expected_calls = 0u;
    uint64_t expected_returns = 0u;
    vf2_status status = VF2_OK;

    memset(&meter_report, 0, sizeof(meter_report));
    if (flagged_phase_index != UINT8_C(0x8b) ||
        cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(
        machine,
        UINT32_C(0x0005fea8) +
            (uint32_t)(flagged_phase_index & UINT8_C(0x7f)) * UINT32_C(8),
        &indirect_target
    );
    if (status != VF2_OK || indirect_target != UINT32_C(0x0005ef60)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0050016c), &base
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500804), &player0
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500808), &player1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3324), &mode, sizeof(mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, base + UINT32_C(0x3320), &base_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x005000a5), &phase_a5, sizeof(phase_a5)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x00500171), &system_flags,
            sizeof(system_flags)
        );
    }
    first_visit = phase_a5 == 0u;
    if (status != VF2_OK || mode == UINT8_C(25) ||
        (base_flags & UINT32_C(3)) != 0u ||
        (!first_visit && phase_a5 != UINT8_C(0xff)) ||
        (first_visit && (system_flags & UINT8_C(1)) == 0u)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    expected_instructions =
        first_visit ? UINT64_C(13286) : UINT64_C(626);
    expected_calls = first_visit ? UINT64_C(27) : UINT64_C(25);
    expected_returns = first_visit ? UINT64_C(28) : UINT64_C(26);

    /* Recreate the ROM call chain a6c0 -> 10b5c -> 58fe0. Keeping these
     * frames real, instead of merely adjusting aggregate counters, preserves
     * the local-window and stack bytes subsequently touched by 0x20f0. */
    status = vf2_i960_cpu_enter_procedure(
        cpu, UINT32_C(0x00010b5c), UINT32_C(0x0000a6f4)
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[3] = UINT32_C(0xff);
    cpu->registers[1] += UINT32_C(4);
    status = vf2_model2a_write_u32(
        machine, cpu->registers[1] - UINT32_C(4), saved_g4
    );
    if (status == VF2_OK) {
        status = vf2_i960_cpu_enter_procedure(
            cpu, UINT32_C(0x00058fe0), UINT32_C(0x00010b78)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[VF2_I960_G0_REGISTER + 4u] = base;
    cpu->registers[VF2_I960_G0_REGISTER + 7u] = player0;
    cpu->registers[VF2_I960_G0_REGISTER + 8u] = player1;
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00500700), &cpu->registers[9]
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500704), &cpu->registers[8]
        );
    }
    cpu->registers[3] =
        (uint32_t)(flagged_phase_index & UINT8_C(0x7f));
    if (status != VF2_OK) {
        return status;
    }

    /* 0x0005ef60 saves g9, then the observed mode takes the 0x20f0 call. */
    cpu->registers[1] += UINT32_C(4);
    saved_g9 = cpu->registers[VF2_I960_G0_REGISTER + 9u];
    status = vf2_model2a_write_u32(
        machine, cpu->registers[1] - UINT32_C(4), saved_g9
    );
    cpu->registers[VF2_I960_G0_REGISTER + 9u] = base;
    cpu->registers[15] = mode;
    if (status == VF2_OK) {
        status = vf2_i960_cpu_enter_procedure(
            cpu, VF2_GAME_METER_UPDATE_ENTRY, UINT32_C(0x0005ef90)
        );
    }
    if (status == VF2_OK) {
        status = execute_game_meter_update(machine, cpu, &meter_report);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0005ef90)) {
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

    /* 0x0005ff54 -> 0x00009480: CRC 15 bytes at base+0x3320. */
    status = vf2_i960_cpu_enter_procedure(
        cpu, UINT32_C(0x0005ff54), UINT32_C(0x0005efa0)
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[15] = base;
    cpu->registers[VF2_I960_G0_REGISTER] = base + UINT32_C(0x3320);
    cpu->registers[VF2_I960_G0_REGISTER + 2u] = UINT32_C(15);
    cpu->registers[VF2_I960_G0_REGISTER + 1u] = 0u;
    status = vf2_i960_cpu_enter_procedure(
        cpu, UINT32_C(0x00009480), UINT32_C(0x0005ff70)
    );
    if (status == VF2_OK) {
        status = compute_table_crc16(
            machine, base + UINT32_C(0x3320), UINT32_C(15), &crc
        );
    }
    if (status == VF2_OK) {
        cpu->registers[VF2_I960_G0_REGISTER] = crc;
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status == VF2_OK && cpu->ip == UINT32_C(0x0005ff70)) {
        status = write_u16(machine, UINT32_C(0x01d03300), crc);
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0005efa0)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[3] = UINT32_C(0xff);
    cpu->registers[4] = phase_a5;
    if (first_visit) {
        /* First visit: clear the plane and draw the phase-11 diagnostic
         * label, then arm the 320-frame countdown. */
        cpu->registers[VF2_I960_G0_REGISTER + 9u] = UINT32_C(0x01000000);
        cpu->registers[VF2_I960_G0_REGISTER] = UINT32_C(64);
        cpu->registers[VF2_I960_G0_REGISTER + 1u] = UINT32_C(48);
        status = vf2_i960_cpu_enter_procedure(
            cpu, UINT32_C(0x00008ef0), UINT32_C(0x0005efc4)
        );
        if (status == VF2_OK) {
            status = clear_tile_plane_64x48(
                machine, UINT32_C(0x01000000)
            );
        }
        if (status == VF2_OK) {
            status = vf2_i960_cpu_return_procedure(cpu, machine);
        }
        if (status == VF2_OK) {
            /* 0x00008ef0 leaves g1 cleared on the observed 64x48 fill. */
            cpu->registers[VF2_I960_G0_REGISTER + 1u] = 0u;
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x0005ff1c), &screen_record
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, screen_record, &screen_destination
            );
        }
        if (status != VF2_OK ||
            screen_record > UINT32_MAX - UINT32_C(4)) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        screen_source = screen_record + UINT32_C(4);
        cpu->registers[15] = screen_record;
        cpu->registers[VF2_I960_G0_REGISTER + 9u] = screen_destination;
        cpu->registers[VF2_I960_G0_REGISTER] = screen_source;
        status = vf2_i960_cpu_enter_procedure(
            cpu, UINT32_C(0x00007fc0), UINT32_C(0x0005efd8)
        );
        if (status == VF2_OK) {
            status = copy_diagnostic_text(
                machine, screen_source, screen_destination, &characters
            );
        }
        if (status == VF2_OK) {
            status = vf2_i960_cpu_return_procedure(cpu, machine);
        }
        if (status == VF2_OK) {
            cpu->registers[15] = UINT32_C(320);
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00500024), UINT32_C(320)
            );
        }
        if (status == VF2_OK) {
            const uint8_t ff = UINT8_C(0xff);
            cpu->registers[15] = UINT32_C(0xff);
            status = vf2_model2a_write(
                machine, UINT32_C(0x005000a5), &ff, sizeof(ff)
            );
        }
        cpu->registers[6] = system_flags;
    } else {
        uint32_t countdown = 0u;

        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500024), &countdown
        );
        --countdown;
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00500024), countdown
            );
        }
        cpu->registers[3] = countdown;
        if (status != VF2_OK) {
            return status;
        }
        if ((int32_t)countdown <= 0) {
            uint16_t layer = 0u;
            uint32_t layer_control = 0u;
            const uint32_t reset_words[4] = {
                UINT32_C(0x52455320), UINT32_C(0x4e4c2053),
                UINT32_C(0x4e204544), UINT32_C(0x20514555)
            };
            const uint8_t zero = 0u;

            terminal_reset = 1;
            expected_instructions = UINT64_C(13194);
            expected_calls = UINT64_C(27);
            expected_returns = UINT64_C(25);

            /* 0x0005f07c: terminal test-mode countdown. The ROM does not
             * return through the phase wrappers. It disables the two layer
             * bits, clears transient gameplay state and the diagnostic tile
             * plane, emits the RESET sentinel, then branches directly to the
             * boot entry at 0x000000b0. */
            cpu->registers[15] = UINT32_C(0x8000);
            status = write_u16(
                machine, UINT32_C(0x00500082), UINT16_C(0x8000)
            );
            cpu->registers[3] = UINT32_C(0x0100a00c);
            if (status == VF2_OK) {
                status = read_u16(machine, cpu->registers[3], &layer);
            }
            layer &= UINT16_C(0x7fff);
            cpu->registers[4] = (uint32_t)layer;
            if (status == VF2_OK) {
                status = write_u16(machine, cpu->registers[3], layer);
            }
            if (status == VF2_OK) {
                status = read_u16(
                    machine, cpu->registers[3] + UINT32_C(2), &layer
                );
            }
            layer &= UINT16_C(0x7fff);
            cpu->registers[4] = (uint32_t)layer;
            if (status == VF2_OK) {
                status = write_u16(
                    machine, cpu->registers[3] + UINT32_C(2), layer
                );
            }
            cpu->registers[4] = 0u;
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x0050009c), &zero, sizeof(zero)
                );
            }

            cpu->registers[VF2_I960_G0_REGISTER + 9u] =
                UINT32_C(0x01000000);
            cpu->registers[VF2_I960_G0_REGISTER] = UINT32_C(64);
            cpu->registers[VF2_I960_G0_REGISTER + 1u] = UINT32_C(48);
            if (status == VF2_OK) {
                status = vf2_i960_cpu_enter_procedure(
                    cpu, UINT32_C(0x00008ef0), UINT32_C(0x0005f0c8)
                );
            }
            if (status == VF2_OK) {
                status = clear_tile_plane_64x48(
                    machine, UINT32_C(0x01000000)
                );
            }
            if (status == VF2_OK) {
                status = vf2_i960_cpu_return_procedure(cpu, machine);
            }
            if (status == VF2_OK) {
                cpu->registers[VF2_I960_G0_REGISTER + 9u] =
                    UINT32_C(0x01001800);
                cpu->registers[VF2_I960_G0_REGISTER + 1u] = 0u;
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x0050081c), &cpu->registers[3]
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, cpu->registers[3], &layer_control
                );
            }
            layer_control &= ~UINT32_C(1);
            cpu->registers[15] = layer_control;
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, cpu->registers[3], layer_control
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, cpu->registers[3], &layer_control
                );
            }
            layer_control &= ~UINT32_C(2);
            cpu->registers[15] = layer_control;
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, cpu->registers[3], layer_control
                );
            }
            cpu->registers[15] = 0u;
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500708), 0u
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500704), 0u
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500700), 0u
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x0050070c), 0u
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, cpu->registers[1] - UINT32_C(4),
                    &cpu->registers[VF2_I960_G0_REGISTER + 4u]
                );
            }
            cpu->registers[1] -= UINT32_C(4);
            cpu->registers[3] = UINT32_C(0x00e80004);
            cpu->registers[4] = 0u;
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, cpu->registers[3], 0u
                );
            }
            if (status == VF2_OK) {
                status = vf2_i960_cpu_enter_procedure(
                    cpu, UINT32_C(0x0006116c), UINT32_C(0x0005f138)
                );
            }
            if (status == VF2_OK) {
                cpu->registers[8] = reset_words[0];
                cpu->registers[9] = reset_words[1];
                cpu->registers[10] = reset_words[2];
                cpu->registers[11] = reset_words[3];
                status = vf2_model2a_write(
                    machine, UINT32_C(0x0059cfe0), reset_words,
                    sizeof(reset_words)
                );
            }
            if (status == VF2_OK) {
                status = vf2_i960_cpu_return_procedure(cpu, machine);
            }
            if (status == VF2_OK) {
                cpu->ip = UINT32_C(0x000000b0);
            }
        }
    }
    if (status != VF2_OK) {
        return status;
    }

    if (!terminal_reset) {
        /* ret from 0x5ef60/58fe0, wrapper restore, then ret from a6c0. */
        set_equal_condition(cpu);
        status = vf2_i960_cpu_return_procedure(cpu, machine);
        if (status != VF2_OK || cpu->ip != UINT32_C(0x00010b78)) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        status = vf2_model2a_read_u32(
            machine, cpu->registers[1] - UINT32_C(4),
            &cpu->registers[VF2_I960_G0_REGISTER + 4u]
        );
        cpu->registers[1] -= UINT32_C(4);
        if (status == VF2_OK) {
            status = vf2_i960_cpu_return_procedure(cpu, machine);
        }
        if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a6f4)) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        if (status == VF2_OK) {
            status = vf2_i960_cpu_return_procedure(cpu, machine);
        }
        if (status != VF2_OK || cpu->ip != frame_dispatch_return) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
    }

    accounted = cpu->executed_instructions - start_instructions;
    if (accounted > expected_instructions) {
        return VF2_ERROR_UNSUPPORTED;
    }
    cpu->executed_instructions += expected_instructions - accounted;
    if (cpu->procedure_calls - start_calls != expected_calls ||
        cpu->procedure_returns - start_returns != expected_returns) {
        return VF2_ERROR_UNSUPPORTED;
    }

    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->rows = characters;
    report->changed_values =
        UINT64_C(3) + meter_report.changed_values +
        (first_visit ? characters + UINT64_C(2) : UINT64_C(1));
    report->bytes_written =
        5u + sizeof(uint32_t) * 2u + meter_report.bytes_written +
        sizeof(uint16_t) +
        (first_visit
            ? (size_t)UINT32_C(64 * 48 * 2) +
                (size_t)characters * 2u + sizeof(uint32_t) + sizeof(uint8_t)
            : sizeof(uint32_t));
    report->recovered_instruction_count = expected_instructions;
    report->recovered_procedure_calls = expected_calls;
    report->recovered_procedure_returns = expected_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status phase17_zero_initialize_control_fighter(
    vf2_model2a *machine,
    uint32_t fighter
)
{
    uint32_t zero = 0u;
    uint16_t zero16 = 0u;
    uint8_t input = 0u;
    uint8_t one = UINT8_C(1);
    vf2_status status = VF2_OK;

    status = vf2_model2a_write_u32(
        machine, fighter + UINT32_C(0x1208), UINT32_C(8)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, fighter + UINT32_C(0x1208), zero
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, fighter + UINT32_C(0x1218), zero
        );
    }
    if (status == VF2_OK) {
        status = write_u16(
            machine, fighter + UINT32_C(0x120e), zero16
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, fighter + UINT32_C(0x121c), zero
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, fighter + UINT32_C(0x123c), zero
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, fighter + UINT32_C(0x1200), &input, sizeof(input)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, fighter + UINT32_C(0x1280), &input, sizeof(input)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, fighter + UINT32_C(0x1281), &input, sizeof(input)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, fighter + UINT32_C(0x1202), &one, sizeof(one)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, fighter + UINT32_C(0x1203), &one, sizeof(one)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, fighter + UINT32_C(0x1204), &one, sizeof(one)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, fighter + UINT32_C(0x1205), &one, sizeof(one)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, fighter + UINT32_C(0x1206), &one, sizeof(one)
        );
    }
    return status;
}

static float phase17_zero_float_from_bits(uint32_t bits)
{
    float value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t phase17_zero_float_to_bits(float value)
{
    uint32_t bits = 0u;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static int32_t phase17_zero_round_nearest_even(float value)
{
    const double input = (double)value;
    const int64_t truncated = (int64_t)input;
    const double fraction = input - (double)truncated;
    int64_t rounded = truncated;

    if (fraction > 0.5 ||
        (fraction == 0.5 && (truncated & INT64_C(1)) != 0)) {
        ++rounded;
    } else if (fraction < -0.5 ||
               (fraction == -0.5 && (truncated & INT64_C(1)) != 0)) {
        --rounded;
    }
    return (int32_t)rounded;
}

static vf2_status phase17_zero_render_decimal(
    vf2_model2a *machine,
    int32_t value,
    uint32_t destination
)
{
    uint32_t magnitude = value < 0 ? (uint32_t)(-(int64_t)value)
                                   : (uint32_t)value;
    uint16_t tiles[6];
    uint32_t index = 0u;
    vf2_status status = VF2_OK;

    tiles[0] = value < 0 ? UINT16_C(0x802d) : UINT16_C(0x8020);
    if (magnitude <= UINT32_C(8191)) {
        tiles[1] = UINT16_C(0x8020);
        for (index = 0u; index < 4u; ++index) {
            status = read_u16(
                machine,
                UINT32_C(0x02040000) + magnitude * UINT32_C(8) +
                    index * UINT32_C(2),
                &tiles[index + 2u]
            );
            if (status != VF2_OK) {
                return status;
            }
        }
    } else {
        for (index = 0u; index < 5u; ++index) {
            const uint32_t place = UINT32_C(4) - index;
            uint32_t divisor = 1u;
            uint32_t digit = 0u;
            uint32_t inner = 0u;
            for (inner = 0u; inner < place; ++inner) {
                divisor *= UINT32_C(10);
            }
            digit = (magnitude / divisor) % UINT32_C(10);
            tiles[index + 1u] =
                (uint16_t)(UINT16_C(0x8030) + (uint16_t)digit);
        }
    }
    for (index = 0u; status == VF2_OK && index < 6u; ++index) {
        status = write_u16(
            machine, destination + index * UINT32_C(2), tiles[index]
        );
    }
    return status;
}

static vf2_status phase17_zero_render_decimal_inline(
    vf2_model2a *machine,
    int32_t value,
    uint32_t destination,
    uint32_t inline_source
)
{
    vf2_status status = phase17_zero_render_decimal(machine, value, destination);
    uint64_t characters = 0u;

    if (status == VF2_OK) {
        status = copy_diagnostic_text(
            machine, inline_source, destination + UINT32_C(14), &characters
        );
    }
    return status;
}

static vf2_status phase17_zero_render_float_inline(
    vf2_model2a *machine,
    uint32_t bits,
    float scale,
    uint32_t destination,
    uint32_t inline_source
)
{
    const float scaled = phase17_zero_float_from_bits(bits) * scale;
    return phase17_zero_render_decimal_inline(
        machine, phase17_zero_round_nearest_even(scaled), destination,
        inline_source
    );
}

static vf2_status phase17_zero_render_ratio_inline(
    vf2_model2a *machine,
    uint16_t value,
    uint32_t destination,
    uint32_t inline_source
)
{
    const uint32_t scaled = ((uint32_t)value * UINT32_C(3600)) /
                            UINT32_C(0xffff);
    return phase17_zero_render_decimal_inline(
        machine, (int32_t)scaled, destination, inline_source
    );
}

static vf2_status phase17_zero_render_hex_inline(
    vf2_model2a *machine,
    uint32_t value,
    uint32_t destination,
    uint32_t inline_source,
    uint32_t width
)
{
    const uint32_t start = destination - UINT32_C(2);
    uint32_t index = 0u;
    int started = 0;
    vf2_status status = VF2_OK;
    uint64_t characters = 0u;

    for (index = 0u; status == VF2_OK && index < width; ++index) {
        const uint32_t shift = (width - UINT32_C(1) - index) * UINT32_C(4);
        const uint32_t nibble = shift >= UINT32_C(32)
            ? 0u : ((value >> shift) & UINT32_C(0xf));
        uint16_t tile = UINT16_C(0x8020);
        if (nibble != 0u || started || index + UINT32_C(1) == width) {
            started = 1;
            status = read_u16(
                machine,
                UINT32_C(0x02000200) + nibble * UINT32_C(2),
                &tile
            );
        }
        if (status == VF2_OK) {
            status = write_u16(
                machine, start + index * UINT32_C(2), tile
            );
        }
    }
    if (status == VF2_OK) {
        const uint32_t text_destination = destination +
            (width == UINT32_C(9) ? UINT32_C(18) : UINT32_C(14));
        status = copy_diagnostic_text(
            machine, inline_source, text_destination, &characters
        );
    }
    return status;
}

static vf2_status phase17_zero_render_hex6_inline(
    vf2_model2a *machine,
    uint32_t value,
    uint32_t destination,
    uint32_t inline_source
)
{
    return phase17_zero_render_hex_inline(
        machine, value & UINT32_C(0xffff), destination, inline_source,
        UINT32_C(7)
    );
}

static vf2_status phase17_zero_render_hex8_inline(
    vf2_model2a *machine,
    uint32_t value,
    uint32_t destination,
    uint32_t inline_source
)
{
    return phase17_zero_render_hex_inline(
        machine, value, destination, inline_source, UINT32_C(9)
    );
}

static vf2_status phase17_zero_copy_text(
    vf2_model2a *machine,
    uint32_t source,
    uint32_t destination
)
{
    uint64_t characters = 0u;
    return copy_diagnostic_text(machine, source, destination, &characters);
}

static vf2_status phase17_zero_read_s8(
    vf2_model2a *machine,
    uint32_t address,
    int32_t *value
)
{
    int8_t raw = 0;
    const vf2_status status = vf2_model2a_read(
        machine, address, &raw, sizeof(raw)
    );
    if (status == VF2_OK) {
        *value = (int32_t)raw;
    }
    return status;
}

static vf2_status phase17_zero_read_s16(
    vf2_model2a *machine,
    uint32_t address,
    int32_t *value
)
{
    uint16_t raw = 0u;
    const vf2_status status = read_u16(machine, address, &raw);
    if (status == VF2_OK) {
        *value = (int32_t)(int16_t)raw;
    }
    return status;
}

static vf2_status phase17_zero_write_float(
    vf2_model2a *machine,
    uint32_t address,
    float value
)
{
    return vf2_model2a_write_u32(
        machine, address, phase17_zero_float_to_bits(value)
    );
}

static vf2_status phase17_zero_adjust_float(
    vf2_model2a *machine,
    uint32_t address,
    float delta
)
{
    uint32_t bits = 0u;
    vf2_status status = vf2_model2a_read_u32(machine, address, &bits);
    if (status == VF2_OK) {
        status = phase17_zero_write_float(
            machine, address, phase17_zero_float_from_bits(bits) + delta
        );
    }
    return status;
}

static vf2_status phase17_zero_render_motion_name(
    vf2_model2a *machine,
    int32_t motion_index,
    uint32_t destination,
    uint32_t *descriptor_out
)
{
    uint32_t descriptor = 0u;
    uint32_t source = 0u;
    vf2_status status = VF2_OK;

    if (motion_index < 0 || motion_index > INT32_C(0x54f)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(
        machine,
        UINT32_C(0x02120004) + (uint32_t)motion_index * UINT32_C(4),
        &descriptor
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, descriptor, &source);
    }
    if (status == VF2_OK) {
        status = phase17_zero_copy_text(machine, source, destination);
    }
    if (status == VF2_OK && descriptor_out != NULL) {
        *descriptor_out = descriptor;
    }
    return status;
}

static vf2_status phase17_zero_screen6_render_motion_name(
    vf2_model2a *machine,
    int32_t motion_index,
    uint32_t destination,
    uint32_t *descriptor_out
)
{
    uint32_t table = 0u;
    uint32_t source = 0u;
    vf2_status status = VF2_OK;

    if (motion_index < 0 || motion_index > INT32_C(0x54f)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x02120004), &table
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, table + (uint32_t)motion_index * UINT32_C(4),
            &source
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_copy_text(machine, source, destination);
    }
    if (status == VF2_OK && descriptor_out != NULL) {
        *descriptor_out = source;
    }
    return status;
}

static vf2_status phase17_zero_apply_camera_angle_controls(
    vf2_model2a *machine,
    uint32_t control,
    uint32_t input_flags,
    uint32_t navigation_flags
)
{
    uint32_t flags = navigation_flags;
    uint16_t x_angle = 0u;
    uint16_t y_angle = 0u;
    uint32_t step = UINT32_C(8);
    vf2_status status = VF2_OK;

    if ((input_flags & (UINT32_C(1) << 9u)) != 0u) {
        step = UINT32_C(128);
    }
    if ((input_flags & (UINT32_C(1) << 10u)) != 0u) {
        flags = input_flags;
    }
    status = read_u16(machine, control + UINT32_C(0x24), &x_angle);
    if (status == VF2_OK) {
        status = read_u16(machine, control + UINT32_C(0x26), &y_angle);
    }
    if (status != VF2_OK) {
        return status;
    }
    if ((flags & (UINT32_C(1) << 21u)) != 0u) {
        x_angle = (uint16_t)(x_angle + (uint16_t)step);
    }
    if ((flags & (UINT32_C(1) << 20u)) != 0u) {
        x_angle = (uint16_t)(x_angle - (uint16_t)step);
    }
    if ((flags & (UINT32_C(1) << 23u)) != 0u) {
        y_angle = (uint16_t)(y_angle + (uint16_t)step);
    }
    if ((flags & (UINT32_C(1) << 22u)) != 0u) {
        y_angle = (uint16_t)(y_angle - (uint16_t)step);
    }
    status = write_u16(machine, control + UINT32_C(0x24), x_angle);
    if (status == VF2_OK) {
        status = write_u16(machine, control + UINT32_C(0x26), y_angle);
    }
    return status;
}

static vf2_status phase17_zero_copro_command(
    vf2_model2a *machine,
    uint32_t command,
    const uint32_t *arguments,
    size_t argument_count,
    uint32_t *results,
    size_t result_count
)
{
    const uint32_t port = UINT32_C(0x00884000);
    size_t index = 0u;
    vf2_status status = vf2_model2a_write_u32(machine, port, command);

    for (index = 0u; status == VF2_OK && index < argument_count; ++index) {
        status = vf2_model2a_write_u32(machine, port, arguments[index]);
    }
    for (index = 0u; status == VF2_OK && index < result_count; ++index) {
        status = vf2_model2a_read_u32(machine, port, &results[index]);
    }
    return status;
}

static vf2_status phase17_zero_screen6_adjust_follow(
    vf2_model2a *machine,
    uint32_t control,
    uint32_t accumulator_offset,
    uint32_t value_offset,
    float delta
)
{
    uint32_t accumulator_bits = 0u;
    uint32_t value_bits = 0u;
    float accumulator = 0.0f;
    float value = 0.0f;
    vf2_status status = vf2_model2a_read_u32(
        machine, control + accumulator_offset, &accumulator_bits
    );

    if (status == VF2_OK) {
        accumulator = phase17_zero_float_from_bits(accumulator_bits) + delta;
        accumulator_bits = phase17_zero_float_to_bits(accumulator);
        status = vf2_model2a_write_u32(
            machine, control + accumulator_offset, accumulator_bits
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, control + value_offset, &value_bits
        );
    }
    if (status == VF2_OK) {
        value = phase17_zero_float_from_bits(value_bits);
        value += (accumulator - value) * 0.25f;
        status = vf2_model2a_write_u32(
            machine, control + value_offset,
            phase17_zero_float_to_bits(value)
        );
    }
    return status;
}

static vf2_status phase17_zero_screen6_force_blank(
    vf2_model2a *machine,
    uint32_t control,
    uint32_t player0,
    uint8_t *de_flags
)
{
    int32_t motion = 0;
    vf2_status status = VF2_OK;

    *de_flags = (uint8_t)(*de_flags | UINT8_C(4));
    status = vf2_model2a_write(
        machine, control + UINT32_C(0xde), de_flags, sizeof(*de_flags)
    );
    if (status == VF2_OK) {
        status = phase17_zero_read_s16(
            machine, UINT32_C(0x00508020), &motion
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, player0 + UINT32_C(0x194), (uint32_t)motion
        );
    }
    return status;
}

static vf2_status phase17_zero_screen6_toggle_objects(
    vf2_model2a *machine,
    uint32_t control,
    uint32_t player1,
    uint8_t *de_flags
)
{
    uint32_t flags = 0u;
    uint32_t associated = 0u;
    uint8_t motion = 0u;
    vf2_status status = VF2_OK;

    if ((*de_flags & UINT8_C(1)) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    *de_flags = (uint8_t)(*de_flags | UINT8_C(1));
    status = vf2_model2a_write(
        machine, control + UINT32_C(0xde), de_flags, sizeof(*de_flags)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, player1, &flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, player1, flags | (UINT32_C(1) << 31u)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, player1 + UINT32_C(0x0c), UINT32_C(0x00013f08)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, player1 + UINT32_C(0x1b0), &motion, sizeof(motion)
        );
    }
    if (status == VF2_OK) {
        motion = (uint8_t)(motion % UINT8_C(13));
        status = vf2_model2a_write(
            machine, player1 + UINT32_C(0x1b1), &motion, sizeof(motion)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050086c), &associated
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, associated, &flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, associated, flags | (UINT32_C(1) << 31u)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, associated + UINT32_C(0x0c), UINT32_C(0x000640f4)
        );
    }
    return status;
}

static vf2_status phase17_zero_screen6_mode1_target(
    vf2_model2a *machine,
    uint32_t control,
    uint32_t player0,
    uint32_t input_flags
)
{
    uint32_t player_x = 0u;
    uint32_t player_z = 0u;
    vf2_status status = vf2_model2a_read_u32(
        machine, player0 + UINT32_C(0x18), &player_x
    );

    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player0 + UINT32_C(0x20), &player_z
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, control + UINT32_C(0xc0), player_x
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, control + UINT32_C(0xc4), UINT32_C(0x3f800000)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, control + UINT32_C(0xc8), player_z
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_screen6_adjust_follow(
            machine, control, UINT32_C(0xc0), UINT32_C(0xb4), 0.0f
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_screen6_adjust_follow(
            machine, control, UINT32_C(0xc4), UINT32_C(0xb8), 0.0f
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_screen6_adjust_follow(
            machine, control, UINT32_C(0xc8), UINT32_C(0xbc), 0.0f
        );
    }
    if (status == VF2_OK &&
        (input_flags & (UINT32_C(1) << 21u)) != 0u) {
        status = phase17_zero_adjust_float(
            machine, UINT32_C(0x00501084), 2.0f
        );
        if (status == VF2_OK) {
            status = phase17_zero_adjust_float(
                machine, UINT32_C(0x00501088), 2.0f
            );
        }
    }
    if (status == VF2_OK &&
        (input_flags & (UINT32_C(1) << 20u)) != 0u) {
        status = phase17_zero_adjust_float(
            machine, UINT32_C(0x00501084), -2.0f
        );
        if (status == VF2_OK) {
            status = phase17_zero_adjust_float(
                machine, UINT32_C(0x00501088), -2.0f
            );
        }
    }
    return status;
}

static vf2_status phase17_zero_screen6_apply_held_controls(
    vf2_model2a *machine,
    uint32_t control,
    uint32_t input_flags
)
{
    vf2_status status = VF2_OK;

    if ((input_flags & (UINT32_C(1) << 13u)) != 0u) {
        status = phase17_zero_screen6_adjust_follow(
            machine, control, UINT32_C(0xd4), UINT32_C(0x20), 0.05f
        );
    } else if ((input_flags & (UINT32_C(1) << 12u)) != 0u) {
        status = phase17_zero_screen6_adjust_follow(
            machine, control, UINT32_C(0xd4), UINT32_C(0x20), -0.05f
        );
    }
    if (status == VF2_OK &&
        (input_flags & (UINT32_C(1) << 14u)) != 0u) {
        status = phase17_zero_screen6_adjust_follow(
            machine, control, UINT32_C(0xcc), UINT32_C(0x18), 0.05f
        );
    } else if (status == VF2_OK &&
               (input_flags & (UINT32_C(1) << 15u)) != 0u) {
        status = phase17_zero_screen6_adjust_follow(
            machine, control, UINT32_C(0xcc), UINT32_C(0x18), -0.05f
        );
    }
    if (status == VF2_OK &&
        (input_flags & (UINT32_C(1) << 8u)) != 0u) {
        status = phase17_zero_screen6_adjust_follow(
            machine, control, UINT32_C(0xd0), UINT32_C(0x1c), -0.05f
        );
    } else if (status == VF2_OK &&
               (input_flags & (UINT32_C(1) << 9u)) != 0u) {
        status = phase17_zero_screen6_adjust_follow(
            machine, control, UINT32_C(0xd0), UINT32_C(0x1c), 0.05f
        );
    }

    if (status == VF2_OK &&
        (input_flags & (UINT32_C(1) << 21u)) != 0u) {
        status = phase17_zero_screen6_adjust_follow(
            machine, control, UINT32_C(0xc8), UINT32_C(0xbc), 0.05f
        );
    } else if (status == VF2_OK &&
               (input_flags & (UINT32_C(1) << 20u)) != 0u) {
        status = phase17_zero_screen6_adjust_follow(
            machine, control, UINT32_C(0xc8), UINT32_C(0xbc), -0.05f
        );
    }
    if (status == VF2_OK &&
        (input_flags & (UINT32_C(1) << 22u)) != 0u) {
        status = phase17_zero_screen6_adjust_follow(
            machine, control, UINT32_C(0xc0), UINT32_C(0xb4), 0.05f
        );
    } else if (status == VF2_OK &&
               (input_flags & (UINT32_C(1) << 23u)) != 0u) {
        status = phase17_zero_screen6_adjust_follow(
            machine, control, UINT32_C(0xc0), UINT32_C(0xb4), -0.05f
        );
    }
    if (status == VF2_OK &&
        (input_flags & (UINT32_C(1) << 16u)) != 0u) {
        status = phase17_zero_screen6_adjust_follow(
            machine, control, UINT32_C(0xc4), UINT32_C(0xb8), -0.05f
        );
    } else if (status == VF2_OK &&
               (input_flags & (UINT32_C(1) << 17u)) != 0u) {
        status = phase17_zero_screen6_adjust_follow(
            machine, control, UINT32_C(0xc4), UINT32_C(0xb8), 0.05f
        );
    }
    return status;
}

static vf2_status phase17_zero_screen6_mode2_update_character(
    vf2_model2a *machine,
    uint32_t fighter,
    uint32_t other_fighter,
    uint32_t associated_pointer_address,
    uint32_t navigation_flags
)
{
    uint8_t character = 0u;
    uint8_t other_character = 0u;
    uint8_t motion = 0u;
    uint32_t associated = 0u;
    uint32_t flags = 0u;
    vf2_status status = vf2_model2a_read(
        machine, fighter + UINT32_C(0x1b0), &character, sizeof(character)
    );

    if (status != VF2_OK) {
        return status;
    }
    if ((navigation_flags & (UINT32_C(1) << 16u)) != 0u) {
        character = character == 0u ? UINT8_C(25) : (uint8_t)(character - 1u);
    } else if ((navigation_flags & (UINT32_C(1) << 17u)) != 0u) {
        character = character >= UINT8_C(25) ? 0u : (uint8_t)(character + 1u);
    } else {
        return VF2_OK;
    }
    status = vf2_model2a_read(
        machine, other_fighter + UINT32_C(0x1b0),
        &other_character, sizeof(other_character)
    );
    if (status == VF2_OK && character == other_character) {
        if (character >= UINT8_C(13)) {
            character = (uint8_t)(character - UINT8_C(13));
        } else {
            character = (uint8_t)(character + UINT8_C(13));
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, fighter + UINT32_C(0x1b0),
            &character, sizeof(character)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, fighter + UINT32_C(0x0c), UINT32_C(0x00013f08)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, fighter, UINT32_C(0x80000000)
        );
    }
    if (status == VF2_OK) {
        motion = (uint8_t)(character % UINT8_C(13));
        status = vf2_model2a_write(
            machine, fighter + UINT32_C(0x1b1), &motion, sizeof(motion)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, associated_pointer_address, &associated
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, associated, &flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, associated, flags | (UINT32_C(1) << 31u)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, associated + UINT32_C(0x0c), UINT32_C(0x000640f4)
        );
    }
    return status;
}

static vf2_status phase17_zero_screen6_mode2(
    vf2_model2a *machine,
    uint32_t control,
    uint32_t player0,
    uint32_t player1,
    uint32_t input_flags,
    uint32_t navigation_flags,
    uint8_t de_flags
)
{
    const uint32_t step_bits = UINT32_C(0x3d4ccccd);
    uint32_t vector[3] = {0u, 0u, 0u};
    uint32_t rotated[3] = {0u, 0u, 0u};
    uint16_t matrix_angle = 0u;
    uint16_t fighter_angle = 0u;
    size_t index = 0u;
    vf2_status status = read_u16(
        machine, control + UINT32_C(0x26), &matrix_angle
    );

    /* The zero-state corridor proven here starts from the identity matrix.
     * A non-zero matrix angle requires the still-unrecovered 0x6d command. */
    if (status != VF2_OK) {
        return status;
    }
    if (matrix_angle != 0u &&
        (input_flags & ((UINT32_C(1) << 12u) | (UINT32_C(1) << 13u) |
                        (UINT32_C(1) << 14u) | (UINT32_C(1) << 15u))) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    if ((input_flags & (UINT32_C(1) << 13u)) != 0u) {
        vector[2] = step_bits;
    } else if ((input_flags & (UINT32_C(1) << 12u)) != 0u) {
        vector[2] = step_bits ^ UINT32_C(0x80000000);
    }
    if ((input_flags & (UINT32_C(1) << 14u)) != 0u) {
        vector[0] = step_bits;
    } else if ((input_flags & (UINT32_C(1) << 15u)) != 0u) {
        vector[0] = step_bits ^ UINT32_C(0x80000000);
    }

    if (status == VF2_OK) {
        status = phase17_zero_copro_command(
            machine, UINT32_C(0x14802929), vector, 3u, rotated, 3u
        );
    }
    for (index = 0u; index < 3u && status == VF2_OK; ++index) {
        uint32_t current = 0u;
        status = vf2_model2a_read_u32(
            machine, player0 + UINT32_C(0x18) + (uint32_t)index * UINT32_C(4),
            &current
        );
        if (status == VF2_OK) {
            uint32_t add_arguments[2] = {rotated[index], current};
            status = phase17_zero_copro_command(
                machine, UINT32_C(0x09801313), add_arguments, 2u,
                &current, 1u
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, player0 + UINT32_C(0x18) +
                    (uint32_t)index * UINT32_C(4), current
            );
        }
    }

    if (status == VF2_OK &&
        (input_flags & (UINT32_C(1) << 23u)) != 0u) {
        status = read_u16(machine, player0 + UINT32_C(0x26), &fighter_angle);
        if (status == VF2_OK) {
            fighter_angle = (uint16_t)(fighter_angle + UINT16_C(0x80));
            status = write_u16(
                machine, player0 + UINT32_C(0x26), fighter_angle
            );
        }
    } else if (status == VF2_OK &&
               (input_flags & (UINT32_C(1) << 22u)) != 0u) {
        status = read_u16(machine, player0 + UINT32_C(0x26), &fighter_angle);
        if (status == VF2_OK) {
            fighter_angle = (uint16_t)(fighter_angle - UINT16_C(0x80));
            status = write_u16(
                machine, player0 + UINT32_C(0x26), fighter_angle
            );
        }
    }

    if (status == VF2_OK &&
        (input_flags & (UINT32_C(1) << 18u)) == 0u) {
        status = phase17_zero_screen6_mode2_update_character(
            machine, player0, player1, UINT32_C(0x00500868),
            navigation_flags
        );
    }
    if (status == VF2_OK && (de_flags & UINT8_C(1)) != 0u) {
        if ((input_flags & (UINT32_C(1) << 21u)) != 0u) {
            status = read_u16(machine, player1 + UINT32_C(0x26), &fighter_angle);
            if (status == VF2_OK) {
                fighter_angle = (uint16_t)(fighter_angle + UINT16_C(0x80));
                status = write_u16(
                    machine, player1 + UINT32_C(0x26), fighter_angle
                );
            }
        } else if ((input_flags & (UINT32_C(1) << 20u)) != 0u) {
            status = read_u16(machine, player1 + UINT32_C(0x26), &fighter_angle);
            if (status == VF2_OK) {
                fighter_angle = (uint16_t)(fighter_angle - UINT16_C(0x80));
                status = write_u16(
                    machine, player1 + UINT32_C(0x26), fighter_angle
                );
            }
        }
        if (status == VF2_OK &&
            (input_flags & (UINT32_C(1) << 18u)) != 0u) {
            status = phase17_zero_screen6_mode2_update_character(
                machine, player1, player0, UINT32_C(0x0050086c),
                navigation_flags
            );
        }
    }
    return status;
}

static vf2_status phase17_zero_screen6_mode2_motion(
    vf2_model2a *machine,
    uint32_t navigation_flags,
    uint8_t de_flags
)
{
    int32_t motion = 0;
    vf2_status status = VF2_OK;

    if ((de_flags & UINT8_C(4)) != 0u) {
        return VF2_OK;
    }
    status = phase17_zero_read_s16(
        machine, UINT32_C(0x00508020), &motion
    );
    if (status == VF2_OK &&
        (navigation_flags & (UINT32_C(1) << 9u)) != 0u) {
        ++motion;
        if (motion > 0x54f) {
            motion = 0;
        }
    } else if (status == VF2_OK &&
               (navigation_flags & (UINT32_C(1) << 8u)) != 0u) {
        --motion;
        if (motion < 0) {
            motion = 0x54f;
        }
    }
    if (status == VF2_OK) {
        status = write_u16(
            machine, UINT32_C(0x00508020), (uint16_t)motion
        );
    }
    return status;
}

static vf2_status phase17_zero_screen6_update_angles(
    vf2_model2a *machine,
    uint32_t control
)
{
    uint32_t position[3] = {0u, 0u, 0u};
    uint32_t target[3] = {0u, 0u, 0u};
    uint32_t delta[3] = {0u, 0u, 0u};
    uint32_t magnitude = 0u;
    uint32_t angle = 0u;
    uint16_t desired_pitch = 0u;
    uint16_t desired_yaw = 0u;
    uint16_t desired_roll = 0u;
    uint16_t current = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    for (index = 0u; index < 3u && status == VF2_OK; ++index) {
        status = vf2_model2a_read_u32(
            machine, control + UINT32_C(0xb4) + (uint32_t)index * UINT32_C(4),
            &target[index]
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, control + UINT32_C(0x18) + (uint32_t)index * UINT32_C(4),
                &position[index]
            );
        }
        if (status == VF2_OK) {
            const uint32_t arguments[2] = {target[index], position[index]};
            status = phase17_zero_copro_command(
                machine, UINT32_C(0x0a001414), arguments, 2u,
                &delta[index], 1u
            );
        }
    }
    if (status == VF2_OK) {
        const uint32_t arguments[2] = {delta[0], delta[2]};
        status = phase17_zero_copro_command(
            machine, UINT32_C(0x16802d2d), arguments, 2u,
            &magnitude, 1u
        );
    }
    if (status == VF2_OK) {
        const uint32_t arguments[2] = {magnitude, delta[1]};
        status = phase17_zero_copro_command(
            machine, UINT32_C(0x13802727), arguments, 2u,
            &angle, 1u
        );
        desired_pitch = (uint16_t)angle;
    }
    if (status == VF2_OK) {
        const uint32_t arguments[2] = {
            delta[2], delta[0] ^ UINT32_C(0x80000000)
        };
        status = phase17_zero_copro_command(
            machine, UINT32_C(0x13802727), arguments, 2u,
            &angle, 1u
        );
        desired_yaw = (uint16_t)angle;
    }
    if (status == VF2_OK) {
        status = read_u16(machine, control + UINT32_C(0xdc), &desired_roll);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, control + UINT32_C(0xd8), desired_pitch);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, control + UINT32_C(0xda), desired_yaw);
    }
    if (status == VF2_OK) {
        const uint16_t desired[3] = {desired_pitch, desired_yaw, desired_roll};
        const uint32_t current_offsets[3] = {
            UINT32_C(0x24), UINT32_C(0x26), UINT32_C(0x28)
        };
        for (index = 0u; index < 3u && status == VF2_OK; ++index) {
            int32_t difference = 0;
            status = read_u16(machine, control + current_offsets[index], &current);
            if (status != VF2_OK) {
                break;
            }
            difference = (int32_t)(int16_t)(uint16_t)(desired[index] - current);
            difference >>= 2;
            current = (uint16_t)(current + (uint16_t)difference);
            status = write_u16(machine, control + current_offsets[index], current);
        }
    }
    return status;
}

static vf2_status phase17_zero_render_missing_body(
    vf2_model2a *machine,
    uint8_t menu_index,
    uint32_t player0,
    uint32_t player1,
    uint32_t control,
    uint32_t input_flags,
    uint32_t navigation_flags,
    uint32_t runtime_flags,
    int entry_path,
    uint32_t *final_g0,
    uint32_t *final_g9
)
{
    vf2_status status = VF2_OK;
    uint32_t bits = 0u;
    int32_t signed_value = 0;
    uint16_t short_value = 0u;

    if (final_g0 == NULL || final_g9 == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *final_g0 = 0u;
    *final_g9 = 0u;

    switch (menu_index) {
    case UINT8_C(1): {
        int32_t motion = 0;
        uint32_t descriptor = 0u;
        uint32_t fighter_flags = 0u;
        status = phase17_zero_read_s16(
            machine, UINT32_C(0x00508020), &motion
        );
        if (status != VF2_OK) {
            return status;
        }
        if (!entry_path) {
            if ((navigation_flags & (UINT32_C(1) << 12u)) != 0u ||
                (input_flags & (UINT32_C(1) << 14u)) != 0u) {
                ++motion;
                if (motion > INT32_C(0x54f)) {
                    motion = 0;
                }
            } else if ((navigation_flags & (UINT32_C(1) << 13u)) != 0u ||
                       (input_flags & (UINT32_C(1) << 15u)) != 0u) {
                --motion;
                if (motion < 0) {
                    motion = INT32_C(0x54f);
                }
            }
        }
        status = write_u16(
            machine, UINT32_C(0x00508020), (uint16_t)motion
        );
        if (status == VF2_OK && entry_path) {
            status = vf2_model2a_write_u32(
                machine, player0 + UINT32_C(0x194), (uint32_t)motion
            );
        }
        if (status == VF2_OK && (entry_path ||
            (navigation_flags & (UINT32_C(1) << 8u)) != 0u)) {
            if (!entry_path) {
                status = vf2_model2a_write_u32(
                    machine, player0 + UINT32_C(0x194), (uint32_t)motion
                );
            }
            if (status == VF2_OK) {
                status = fill_tile_plane_spaces(
                    machine, UINT32_C(0x01000298), UINT32_C(52), UINT32_C(1)
                );
            }
        }
        if (status == VF2_OK) {
            status = fill_tile_plane_spaces(
                machine, UINT32_C(0x01000298), UINT32_C(40), UINT32_C(1)
            );
        }
        if (status == VF2_OK) {
            status = phase17_zero_render_decimal_inline(
                machine, motion, UINT32_C(0x01000294), UINT32_C(0x00055674)
            );
        }
        if (status == VF2_OK) {
            status = phase17_zero_copy_text(
                machine, UINT32_C(0x00055688), UINT32_C(0x0100028a)
            );
        }
        if (status == VF2_OK) {
            status = phase17_zero_render_motion_name(
                machine, motion, UINT32_C(0x010002a2), &descriptor
            );
        }
        if (status == VF2_OK) {
            status = phase17_zero_read_s16(machine, descriptor, &signed_value);
        }
        if (status == VF2_OK) {
            status = phase17_zero_render_decimal_inline(
                machine, signed_value, UINT32_C(0x01000322),
                UINT32_C(0x000556d0)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, player0, &fighter_flags);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, player0, fighter_flags & ~(UINT32_C(1) << 5u)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, player1, &fighter_flags);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, player1, fighter_flags & ~(UINT32_C(1) << 5u)
            );
        }
        *final_g0 = UINT32_C(0x00006874);
        *final_g9 = UINT32_C(0x010003a2);
        return status;
    }
    case UINT8_C(2): {
        uint8_t selector = 0u;
        if (!entry_path) {
            status = vf2_model2a_read(
                machine, UINT32_C(0x00508030), &selector, sizeof(selector)
            );
            if (status != VF2_OK) {
                return status;
            }
            if ((navigation_flags & (UINT32_C(1) << 12u)) != 0u) {
                selector = selector >= UINT8_C(2) ? 0u : (uint8_t)(selector + 1u);
            }
            if ((navigation_flags & (UINT32_C(1) << 13u)) != 0u) {
                selector = selector == 0u ? UINT8_C(2) : (uint8_t)(selector - 1u);
            }
            status = vf2_model2a_write(
                machine, UINT32_C(0x00508030), &selector, sizeof(selector)
            );
        }
        if (status == VF2_OK) {
            status = phase17_zero_read_s8(
                machine, UINT32_C(0x00508037), &signed_value
            );
        }
        if (status == VF2_OK) {
            status = phase17_zero_render_decimal_inline(
                machine, signed_value, UINT32_C(0x0100028a),
                UINT32_C(0x00055778)
            );
        }
        if (status == VF2_OK) {
            status = phase17_zero_read_s8(
                machine, UINT32_C(0x00508036), &signed_value
            );
        }
        if (status == VF2_OK) {
            status = phase17_zero_render_decimal_inline(
                machine, signed_value, UINT32_C(0x0100030a),
                UINT32_C(0x00055790)
            );
        }
        if (status == VF2_OK) {
            status = phase17_zero_read_s16(
                machine, UINT32_C(0x00508034), &signed_value
            );
        }
        if (status == VF2_OK) {
            status = phase17_zero_render_decimal_inline(
                machine, signed_value, UINT32_C(0x0100038a),
                UINT32_C(0x000557a8)
            );
        }
        *final_g0 = UINT32_C(0x00006e6f);
        *final_g9 = UINT32_C(0x0100040a);
        return status;
    }
    case UINT8_C(3): {
        if (!entry_path &&
            (navigation_flags & UINT32_C(0x700)) == UINT32_C(0x700)) {
            status = vf2_model2a_write_u32(machine, player0 + UINT32_C(0x18), 0u);
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(machine, player0 + UINT32_C(0x1c), 0u);
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(machine, player0 + UINT32_C(0x20), 0u);
            }
            if (status == VF2_OK) {
                status = write_u16(machine, player0 + UINT32_C(0x26), UINT16_C(0xc000));
            }
        } else if (!entry_path && (input_flags & (UINT32_C(1) << 9u)) != 0u) {
            status = read_u16(machine, player0 + UINT32_C(0x26), &short_value);
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 15u)) != 0u) {
                short_value = (uint16_t)(short_value - UINT16_C(0x200));
            }
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 14u)) != 0u) {
                short_value = (uint16_t)(short_value + UINT16_C(0x200));
            }
            if (status == VF2_OK) {
                status = write_u16(machine, player0 + UINT32_C(0x26), short_value);
            }
        } else if (!entry_path) {
            if ((input_flags & (UINT32_C(1) << 15u)) != 0u) {
                status = phase17_zero_adjust_float(machine, player0 + UINT32_C(0x18), -0.01f);
            }
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 14u)) != 0u) {
                status = phase17_zero_adjust_float(machine, player0 + UINT32_C(0x18), 0.01f);
            }
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 12u)) != 0u) {
                status = phase17_zero_adjust_float(machine, player0 + UINT32_C(0x20), -0.01f);
            }
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 13u)) != 0u) {
                status = phase17_zero_adjust_float(machine, player0 + UINT32_C(0x20), 0.01f);
            }
        }
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, player0 + UINT32_C(0x18), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine, bits, 100.0f, UINT32_C(0x0100028a), UINT32_C(0x0005550c));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, player0 + UINT32_C(0x1c), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine, bits, 100.0f, UINT32_C(0x0100030a), UINT32_C(0x00055530));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, player0 + UINT32_C(0x20), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine, bits, 100.0f, UINT32_C(0x0100038a), UINT32_C(0x00055554));
        if (status == VF2_OK) status = read_u16(machine, player0 + UINT32_C(0x26), &short_value);
        if (status == VF2_OK) status = phase17_zero_render_ratio_inline(machine, short_value, UINT32_C(0x0100040a), UINT32_C(0x0005556c));
        if (status == VF2_OK) status = phase17_zero_render_hex6_inline(machine, short_value, UINT32_C(0x0100048a), UINT32_C(0x00055584));
        *final_g0 = 0u;
        *final_g9 = UINT32_C(0x0100050a);
        return status;
    }
    case UINT8_C(5): {
        const float step = (!entry_path && (input_flags & (UINT32_C(1) << 9u)) != 0u) ? 0.1f : 0.01f;
        uint32_t selected = control + UINT32_C(0x20);
        if (!entry_path) {
            if ((input_flags & (UINT32_C(1) << 15u)) != 0u) status = phase17_zero_adjust_float(machine, control + UINT32_C(0x18), -step);
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 14u)) != 0u) status = phase17_zero_adjust_float(machine, control + UINT32_C(0x18), step);
            if ((input_flags & (UINT32_C(1) << 8u)) != 0u) selected = control + UINT32_C(0x1c);
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 13u)) != 0u) status = phase17_zero_adjust_float(machine, selected, step);
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 12u)) != 0u) status = phase17_zero_adjust_float(machine, selected, -step);
            if (status == VF2_OK) status = phase17_zero_apply_camera_angle_controls(machine, control, input_flags, navigation_flags);
        }
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, control + UINT32_C(0x18), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine, bits, 100.0f, UINT32_C(0x0100028a), UINT32_C(0x000558a4));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, control + UINT32_C(0x1c), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine, bits, 100.0f, UINT32_C(0x0100030a), UINT32_C(0x000558c8));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, control + UINT32_C(0x20), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine, bits, 100.0f, UINT32_C(0x0100038a), UINT32_C(0x000558ec));
        if (status == VF2_OK) status = read_u16(machine, control + UINT32_C(0x24), &short_value);
        if (status == VF2_OK) status = phase17_zero_render_hex6_inline(machine, short_value, UINT32_C(0x0100040a), UINT32_C(0x00055904));
        if (status == VF2_OK) status = read_u16(machine, control + UINT32_C(0x26), &short_value);
        if (status == VF2_OK) status = phase17_zero_render_hex6_inline(machine, short_value, UINT32_C(0x0100048a), UINT32_C(0x0005591c));
        *final_g0 = 0u;
        *final_g9 = UINT32_C(0x0100050a);
        return status;
    }
    case UINT8_C(6): {
        uint8_t camera_mode = 0u;
        uint8_t de_flags = 0u;
        uint32_t mode_name = 0u;
        uint32_t descriptor = 0u;
        int32_t motion = 0;
        uint8_t stage = 0u;

        status = vf2_model2a_read(machine, control + UINT32_C(0xde), &de_flags, sizeof(de_flags));
        de_flags = (uint8_t)(de_flags & ~UINT8_C(4));
        if ((runtime_flags & (UINT32_C(1) << 9u)) != 0u) de_flags = (uint8_t)(de_flags | UINT8_C(4));
        if (status == VF2_OK) status = vf2_model2a_write(machine, control + UINT32_C(0xde), &de_flags, sizeof(de_flags));
        if (status == VF2_OK) status = vf2_model2a_read(machine, control + UINT32_C(0xb1), &camera_mode, sizeof(camera_mode));
        if (status == VF2_OK) {
            const uint8_t active = camera_mode == 0u ? UINT8_C(1) : 0u;
            status = vf2_model2a_write(machine, control + UINT32_C(0xb0), &active, sizeof(active));
        }
        if (status == VF2_OK && !entry_path &&
            (navigation_flags & (UINT32_C(1) << 4u)) != 0u) {
            camera_mode = (uint8_t)(camera_mode + UINT8_C(1));
            if (camera_mode > UINT8_C(2)) {
                camera_mode = 0u;
            }
            status = vf2_model2a_write(
                machine, control + UINT32_C(0xb1), &camera_mode,
                sizeof(camera_mode)
            );
        }
        if (status == VF2_OK && entry_path) {
            const uint8_t zero = 0u;
            const uint8_t one = UINT8_C(1);
            uint16_t angle = 0u;
            status = vf2_model2a_write(machine, control + UINT32_C(0xb1), &zero, sizeof(zero));
            if (status == VF2_OK) status = vf2_model2a_write_u32(machine, control + UINT32_C(0xb4), 0u);
            if (status == VF2_OK) status = vf2_model2a_write_u32(machine, control + UINT32_C(0xb8), 0u);
            if (status == VF2_OK) status = vf2_model2a_write_u32(machine, control + UINT32_C(0xbc), 0u);
            if (status == VF2_OK) status = vf2_model2a_write(machine, control + UINT32_C(0xb0), &one, sizeof(one));
            if (status == VF2_OK) status = vf2_model2a_write(machine, control + UINT32_C(0x40), &zero, sizeof(zero));
            if (status == VF2_OK) status = read_u16(machine, control + UINT32_C(0x24), &angle);
            if (status == VF2_OK) status = write_u16(machine, control + UINT32_C(0xd8), angle);
            if (status == VF2_OK) status = read_u16(machine, control + UINT32_C(0x26), &angle);
            if (status == VF2_OK) status = write_u16(machine, control + UINT32_C(0xda), angle);
            if (status == VF2_OK) status = read_u16(machine, control + UINT32_C(0x28), &angle);
            if (status == VF2_OK) status = write_u16(machine, control + UINT32_C(0xdc), angle);
            if (status == VF2_OK) status = vf2_model2a_read_u32(machine, control + UINT32_C(0x18), &bits);
            if (status == VF2_OK) status = vf2_model2a_write_u32(machine, control + UINT32_C(0xcc), bits);
            if (status == VF2_OK) status = vf2_model2a_read_u32(machine, control + UINT32_C(0x1c), &bits);
            if (status == VF2_OK) status = vf2_model2a_write_u32(machine, control + UINT32_C(0xd0), bits);
            if (status == VF2_OK) status = vf2_model2a_read_u32(machine, control + UINT32_C(0x20), &bits);
            if (status == VF2_OK) status = vf2_model2a_write_u32(machine, control + UINT32_C(0xd4), bits);
            if (status == VF2_OK) {
                uint32_t flags = 0u;
                status = vf2_model2a_read_u32(machine, player1, &flags);
                if (status == VF2_OK) status = vf2_model2a_write_u32(machine, player1, flags | (UINT32_C(1) << 31u));
                if (status == VF2_OK) status = vf2_model2a_write_u32(machine, player1 + UINT32_C(0x0c), UINT32_C(0x00013f08));
            }
            if (status == VF2_OK) {
                uint32_t associated = 0u, flags = 0u;
                status = vf2_model2a_read_u32(machine, UINT32_C(0x0050086c), &associated);
                if (status == VF2_OK) status = vf2_model2a_read_u32(machine, associated, &flags);
                if (status == VF2_OK) status = vf2_model2a_write_u32(machine, associated, flags | (UINT32_C(1) << 31u));
                if (status == VF2_OK) status = vf2_model2a_write_u32(machine, associated + UINT32_C(0x0c), UINT32_C(0x000640f4));
            }
            if (status == VF2_OK) {
                de_flags = (uint8_t)(de_flags | UINT8_C(1));
                status = vf2_model2a_write(machine, control + UINT32_C(0xde), &de_flags, sizeof(de_flags));
            }
        }
        if (status == VF2_OK && !entry_path &&
            machine->copro_read_callback != NULL &&
            machine->copro_write_callback != NULL) {
            if (camera_mode == 0u) {
                status = phase17_zero_screen6_apply_held_controls(
                    machine, control, input_flags
                );
            } else if (camera_mode == UINT8_C(1)) {
                const uint32_t position_only = input_flags &
                    ((UINT32_C(1) << 8u) | (UINT32_C(1) << 9u) |
                     (UINT32_C(1) << 12u) | (UINT32_C(1) << 13u) |
                     (UINT32_C(1) << 14u) | (UINT32_C(1) << 15u));
                status = phase17_zero_screen6_apply_held_controls(
                    machine, control, position_only
                );
                if (status == VF2_OK) {
                    status = phase17_zero_screen6_mode1_target(
                        machine, control, player0, input_flags
                    );
                }
            } else if (camera_mode == UINT8_C(2)) {
                status = phase17_zero_screen6_mode2(
                    machine, control, player0, player1, input_flags,
                    navigation_flags, de_flags
                );
                if (status == VF2_OK) {
                    status = phase17_zero_screen6_mode2_motion(
                        machine, navigation_flags, de_flags
                    );
                }
            } else {
                status = VF2_ERROR_UNSUPPORTED;
            }
            if (status == VF2_OK &&
                (navigation_flags & (UINT32_C(1) << 10u)) != 0u) {
                status = phase17_zero_screen6_force_blank(
                    machine, control, player0, &de_flags
                );
            }
            if (status == VF2_OK) {
                status = phase17_zero_screen6_update_angles(machine, control);
            }
        }
        if (status == VF2_OK && !entry_path &&
            (navigation_flags & (UINT32_C(1) << 5u)) != 0u) {
            status = phase17_zero_screen6_toggle_objects(
                machine, control, player1, &de_flags
            );
        }
        if (status == VF2_OK && !entry_path &&
            (de_flags & UINT8_C(4)) != 0u) {
            status = fill_tile_plane_spaces(
                machine, UINT32_C(0x01000000), UINT32_C(62), UINT32_C(48)
            );
            *final_g0 = UINT32_C(62);
            *final_g9 = UINT32_C(0x01001800);
            return status;
        }
        if (status == VF2_OK) status = phase17_zero_copy_text(machine, UINT32_C(0x00055b7c), UINT32_C(0x01000186));
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00056040) + ((uint32_t)camera_mode & UINT32_C(3)) * UINT32_C(4), &mode_name);
        }
        if (status == VF2_OK) status = phase17_zero_copy_text(machine, mode_name, UINT32_C(0x0100028a));
        if (status == VF2_OK) status = phase17_zero_copy_text(machine, UINT32_C(0x00055c3c), UINT32_C(0x0100038a));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, control + UINT32_C(0x18), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine, bits, 100.0f, UINT32_C(0x0100040a), UINT32_C(0x00055c64));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, control + UINT32_C(0x1c), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine, bits, 100.0f, UINT32_C(0x0100048a), UINT32_C(0x00055c84));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, control + UINT32_C(0x20), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine, bits, 100.0f, UINT32_C(0x0100050a), UINT32_C(0x00055ca4));
        if (status == VF2_OK) status = phase17_zero_copy_text(machine, UINT32_C(0x00055d08), UINT32_C(0x0100058a));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, control + UINT32_C(0xb4), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine, bits, 100.0f, UINT32_C(0x0100060a), UINT32_C(0x00055d34));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, control + UINT32_C(0xb8), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine, bits, 100.0f, UINT32_C(0x0100068a), UINT32_C(0x00055d54));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, control + UINT32_C(0xbc), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine, bits, 100.0f, UINT32_C(0x0100070a), UINT32_C(0x00055d74));
        if (status == VF2_OK) status = vf2_model2a_read(machine, UINT32_C(0x00500064), &stage, sizeof(stage));
        if (status == VF2_OK) status = phase17_zero_render_decimal_inline(machine, (int32_t)stage + 1, UINT32_C(0x0100080a), UINT32_C(0x00055dec));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00501084), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine, bits, 1.0f, UINT32_C(0x0100090a), UINT32_C(0x00055e6c));
        if (status == VF2_OK) status = phase17_zero_copy_text(machine, UINT32_C(0x00055ed8), UINT32_C(0x0100168a));
        if (status == VF2_OK) status = phase17_zero_read_s16(machine, UINT32_C(0x00508020), &motion);
        if (status == VF2_OK) status = phase17_zero_screen6_render_motion_name(machine, motion, UINT32_C(0x0100168a), &descriptor);
        if (status == VF2_OK) status = phase17_zero_copy_text(machine, UINT32_C(0x00055f3c), UINT32_C(0x0100170a));
        if (status == VF2_OK) status = phase17_zero_render_decimal_inline(machine, motion, UINT32_C(0x0100171c), UINT32_C(0x00055f5c));
        if (status == VF2_OK) status = phase17_zero_copy_text(machine, UINT32_C(0x00055f94), UINT32_C(0x01000a0a));
        if (status == VF2_OK) status = phase17_zero_copy_text(machine, UINT32_C(0x00055fb0), UINT32_C(0x01000a8a));
        if (status == VF2_OK) status = phase17_zero_copy_text(machine, UINT32_C(0x00055fec), UINT32_C(0x01000a2e));
        if (status == VF2_OK) status = phase17_zero_copy_text(
            machine,
            (de_flags & UINT8_C(1)) != 0u
                ? UINT32_C(0x00055ffc)
                : UINT32_C(0x00056010),
            UINT32_C(0x01000aae)
        );
        *final_g0 = entry_path || (de_flags & UINT8_C(1)) != 0u
            ? UINT32_C(0x00002020)
            : UINT32_C(0x0000454c);
        *final_g9 = UINT32_C(0x01000b2e);
        return status;
    }
    case UINT8_C(7):
        if (!entry_path) {
            if ((input_flags & (UINT32_C(1) << 15u)) != 0u) status = phase17_zero_adjust_float(machine, control + UINT32_C(0x48), -0.01f);
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 14u)) != 0u) status = phase17_zero_adjust_float(machine, control + UINT32_C(0x48), 0.01f);
        }
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, control + UINT32_C(0x48), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine, bits, 100.0f, UINT32_C(0x0100028a), UINT32_C(0x00057324));
        *final_g0 = UINT32_C(0x00657661);
        *final_g9 = UINT32_C(0x0100030a);
        return status;
    case UINT8_C(9): {
        uint8_t material = 0u;
        if (!entry_path) {
            status = vf2_model2a_read(machine, UINT32_C(0x00530000), &material, sizeof(material));
            if (status == VF2_OK && (navigation_flags & (UINT32_C(1) << 9u)) != 0u) material = (uint8_t)(material + UINT8_C(1));
            if (status == VF2_OK && (navigation_flags & (UINT32_C(1) << 8u)) != 0u) material = (uint8_t)(material - UINT8_C(1));
            if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x00530000), &material, sizeof(material));
        } else {
            status = vf2_model2a_read(machine, UINT32_C(0x00530000), &material, sizeof(material));
        }
        if (status == VF2_OK) status = fill_tile_plane_spaces(machine, UINT32_C(0x0100028a), UINT32_C(8), UINT32_C(1));
        if (status == VF2_OK) status = phase17_zero_render_hex8_inline(machine, material, UINT32_C(0x0100038a), UINT32_C(0x00057ae0));
        if (status == VF2_OK) { uint8_t v=0; status=vf2_model2a_read(machine,UINT32_C(0x00530001),&v,1); if(status==VF2_OK) status=phase17_zero_render_hex8_inline(machine,v,UINT32_C(0x0100040a),UINT32_C(0x00057b04)); }
        if (status == VF2_OK) { uint8_t v=0; status=vf2_model2a_read(machine,UINT32_C(0x00530002),&v,1); if(status==VF2_OK) status=phase17_zero_render_hex8_inline(machine,v,UINT32_C(0x0100048a),UINT32_C(0x00057b28)); }
        if (status == VF2_OK) { uint8_t v=0; status=vf2_model2a_read(machine,UINT32_C(0x00530003),&v,1); if(status==VF2_OK) status=phase17_zero_render_hex8_inline(machine,v,UINT32_C(0x0100050a),UINT32_C(0x00057b4c)); }
        if (status == VF2_OK) { uint8_t v=0; status=vf2_model2a_read(machine,UINT32_C(0x00530004),&v,1); if(status==VF2_OK) status=phase17_zero_render_hex8_inline(machine,v,UINT32_C(0x0100058a),UINT32_C(0x00057b70)); }
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x005010c4), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine, bits, 1000.0f, UINT32_C(0x0100060a), UINT32_C(0x00057b9c));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00530104), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine, bits, 10000.0f, UINT32_C(0x0100068a), UINT32_C(0x00057bc8));
        *final_g0 = 0u;
        *final_g9 = UINT32_C(0x0100070a);
        return status;
    }
    case UINT8_C(10): {
        uint8_t mode = 0u;
        if (!entry_path && (navigation_flags & UINT32_C(0x700)) == UINT32_C(0x700)) {
            status = write_u16(machine, UINT32_C(0x0053011c), 0u);
            if (status == VF2_OK) status = write_u16(machine, UINT32_C(0x00530120), 0u);
            if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00530110), 0u);
            if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00530114), UINT32_C(0x3f800000));
            if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00530118), 0u);
        } else if (!entry_path) {
            status = vf2_model2a_read(
                machine, UINT32_C(0x0053010c), &mode, sizeof(mode)
            );
            if (status == VF2_OK &&
                (navigation_flags & (UINT32_C(1) << 9u)) != 0u) {
                mode = UINT8_C(1);
                status = vf2_model2a_write(
                    machine, UINT32_C(0x0053010c), &mode, sizeof(mode)
                );
            }
            if (status == VF2_OK &&
                (navigation_flags & (UINT32_C(1) << 8u)) != 0u) {
                mode = 0u;
                status = vf2_model2a_write(
                    machine, UINT32_C(0x0053010c), &mode, sizeof(mode)
                );
            }
            if (status == VF2_OK &&
                (input_flags & (UINT32_C(1) << 9u)) != 0u) {
                if ((input_flags & (UINT32_C(1) << 15u)) != 0u) {
                    status = read_u16(
                        machine, UINT32_C(0x00530120), &short_value
                    );
                    if (status == VF2_OK) {
                        status = write_u16(
                            machine, UINT32_C(0x00530120),
                            (uint16_t)(short_value - UINT16_C(0x200))
                        );
                    }
                }
                if (status == VF2_OK &&
                    (input_flags & (UINT32_C(1) << 14u)) != 0u) {
                    status = read_u16(
                        machine, UINT32_C(0x00530120), &short_value
                    );
                    if (status == VF2_OK) {
                        status = write_u16(
                            machine, UINT32_C(0x00530120),
                            (uint16_t)(short_value + UINT16_C(0x200))
                        );
                    }
                }
                if (status == VF2_OK &&
                    (input_flags & (UINT32_C(1) << 13u)) != 0u) {
                    status = read_u16(
                        machine, UINT32_C(0x0053011c), &short_value
                    );
                    if (status == VF2_OK) {
                        status = write_u16(
                            machine, UINT32_C(0x0053011c),
                            (uint16_t)(short_value - UINT16_C(0x200))
                        );
                    }
                }
                if (status == VF2_OK &&
                    (input_flags & (UINT32_C(1) << 12u)) != 0u) {
                    status = read_u16(
                        machine, UINT32_C(0x0053011c), &short_value
                    );
                    if (status == VF2_OK) {
                        status = write_u16(
                            machine, UINT32_C(0x0053011c),
                            (uint16_t)(short_value + UINT16_C(0x200))
                        );
                    }
                }
            } else if (status == VF2_OK &&
                       (input_flags & (UINT32_C(1) << 10u)) != 0u) {
                if ((input_flags & (UINT32_C(1) << 12u)) != 0u) {
                    status = phase17_zero_adjust_float(
                        machine, UINT32_C(0x00530114), -0.01f
                    );
                }
                if (status == VF2_OK &&
                    (input_flags & (UINT32_C(1) << 13u)) != 0u) {
                    status = phase17_zero_adjust_float(
                        machine, UINT32_C(0x00530114), 0.01f
                    );
                }
            } else if (status == VF2_OK) {
                if ((input_flags & (UINT32_C(1) << 15u)) != 0u) {
                    status = phase17_zero_adjust_float(
                        machine, UINT32_C(0x00530110), -0.01f
                    );
                }
                if (status == VF2_OK &&
                    (input_flags & (UINT32_C(1) << 14u)) != 0u) {
                    status = phase17_zero_adjust_float(
                        machine, UINT32_C(0x00530110), 0.01f
                    );
                }
                if (status == VF2_OK &&
                    (input_flags & (UINT32_C(1) << 12u)) != 0u) {
                    status = phase17_zero_adjust_float(
                        machine, UINT32_C(0x00530118), -0.01f
                    );
                }
                if (status == VF2_OK &&
                    (input_flags & (UINT32_C(1) << 13u)) != 0u) {
                    status = phase17_zero_adjust_float(
                        machine, UINT32_C(0x00530118), 0.01f
                    );
                }
            }
        }
        if (status == VF2_OK) status = phase17_zero_read_s16(machine, UINT32_C(0x00530108), &signed_value);
        if (status == VF2_OK) status = phase17_zero_render_hex6_inline(machine, (uint32_t)signed_value, UINT32_C(0x0100028a), entry_path ? UINT32_C(0x00057ef8) : UINT32_C(0x00057ef8));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00530110), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine,bits,100.0f,UINT32_C(0x0100030a),UINT32_C(0x00057f20));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00530114), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine,bits,100.0f,UINT32_C(0x0100038a),UINT32_C(0x00057f48));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00530118), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine,bits,100.0f,UINT32_C(0x0100040a),UINT32_C(0x00057f70));
        if (status == VF2_OK) status = read_u16(machine, UINT32_C(0x00530120), &short_value);
        if (status == VF2_OK) status = phase17_zero_render_ratio_inline(machine,short_value,UINT32_C(0x0100048a),UINT32_C(0x00057f8c));
        if (status == VF2_OK) status = phase17_zero_render_hex6_inline(machine,short_value,UINT32_C(0x0100050a),UINT32_C(0x00057fa8));
        if (status == VF2_OK) status = read_u16(machine, UINT32_C(0x0053011c), &short_value);
        if (status == VF2_OK) status = phase17_zero_render_ratio_inline(machine,short_value,UINT32_C(0x0100058a),UINT32_C(0x00057fc8));
        if (status == VF2_OK) status = phase17_zero_render_hex6_inline(machine,short_value,UINT32_C(0x0100060a),UINT32_C(0x00057fe4));
        *final_g0=0u; *final_g9=UINT32_C(0x0100068a); return status;
    }
    case UINT8_C(12):
        if (!entry_path) {
            status = phase17_zero_read_s16(machine, control + UINT32_C(0xf8), &signed_value);
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 12u)) != 0u) signed_value -= 16;
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 13u)) != 0u) signed_value += 16;
            if (status == VF2_OK) status = write_u16(machine, control + UINT32_C(0xf8), (uint16_t)signed_value);
            if (status == VF2_OK) status = phase17_zero_render_decimal_inline(machine,signed_value,UINT32_C(0x0100028a),UINT32_C(0x0005747c));
        } else {
            status = phase17_zero_read_s16(machine, UINT32_C(0x00501408), &signed_value);
            if (status == VF2_OK) status = phase17_zero_render_decimal_inline(machine,signed_value,UINT32_C(0x0100028a),UINT32_C(0x00057424));
        }
        *final_g0=0u; *final_g9=UINT32_C(0x0100030a); return status;
    case UINT8_C(13): {
        uint32_t flags = 0u;
        uint32_t texture = 0u;
        uint32_t global_mode = 0u;
        if (entry_path) {
            *final_g0=UINT32_C(0x00455255); *final_g9=UINT32_C(0x01000206); return VF2_OK;
        }
        runtime_flags |= UINT32_C(1) << 9u;
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00508000), runtime_flags);
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &flags);
        if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00500068), flags | (UINT32_C(1) << 16u));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500700), &flags);
        if (status == VF2_OK && (flags & (UINT32_C(1) << 10u)) == 0u) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00509800), &texture);
            if (status == VF2_OK && (navigation_flags & (UINT32_C(1) << 14u)) != 0u) ++texture;
            if (status == VF2_OK && (navigation_flags & (UINT32_C(1) << 15u)) != 0u) --texture;
            if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500020), &global_mode);
            if (status == VF2_OK && (global_mode & UINT32_C(1)) != 0u) {
                if ((input_flags & (UINT32_C(1) << 12u)) != 0u) ++texture;
                if ((input_flags & (UINT32_C(1) << 13u)) != 0u) --texture;
            }
            texture = (texture + UINT32_C(86)) % UINT32_C(86);
            if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00509800), texture);
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 22u)) != 0u) status=phase17_zero_adjust_float(machine,UINT32_C(0x00509804),0.005f);
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 23u)) != 0u) status=phase17_zero_adjust_float(machine,UINT32_C(0x00509804),-0.005f);
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 21u)) != 0u) status=phase17_zero_adjust_float(machine,UINT32_C(0x00509808),0.005f);
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 20u)) != 0u) status=phase17_zero_adjust_float(machine,UINT32_C(0x00509808),-0.005f);
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 16u)) != 0u) status=phase17_zero_adjust_float(machine,UINT32_C(0x0050980c),0.005f);
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 18u)) != 0u) status=phase17_zero_adjust_float(machine,UINT32_C(0x0050980c),-0.005f);
        }
        if (status == VF2_OK) {
            uint32_t buffer_index = 0u;
            uint32_t texture_x = 0u;
            uint32_t texture_y = 0u;
            uint32_t texture_z = 0u;
            uint32_t texture_z_squared = 0u;
            uint8_t one = UINT8_C(1);
            uint8_t sixty = UINT8_C(0x60);

            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x005001e4), &buffer_index
            );
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00509804), &texture_x
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00509808), &texture_y
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x0050980c), &texture_z
                );
            }
            if (status == VF2_OK) {
                const float texture_z_value =
                    phase17_zero_float_from_bits(texture_z);
                texture_z_squared = phase17_zero_float_to_bits(
                    texture_z_value * texture_z_value
                );
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x0090e000) + buffer_index, texture_x
                );
            }
            if (status == VF2_OK) {
                buffer_index = (buffer_index + UINT32_C(4)) &
                               ~UINT32_C(0x100);
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x0090e000) + buffer_index, texture_y
                );
            }
            if (status == VF2_OK) {
                buffer_index = (buffer_index + UINT32_C(4)) &
                               ~UINT32_C(0x100);
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x0090e000) + buffer_index,
                    texture_z_squared
                );
            }
            if (status == VF2_OK) {
                uint8_t buffer_byte = 0u;
                buffer_index += UINT32_C(4);
                buffer_byte = (uint8_t)buffer_index;
                status = vf2_model2a_write(
                    machine, UINT32_C(0x005001e4), &buffer_byte,
                    sizeof(buffer_byte)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00501010), &one, sizeof(one)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x0050101c), &sixty, sizeof(sixty)
                );
            }
        }
        if (status == VF2_OK) {
            uint16_t kind = 0u;
            uint8_t display = 0u;
            uint32_t label_source = UINT32_C(0x0004d2e8);
            uint32_t name_dest = UINT32_C(0x010000ea);
            status = read_u16(machine, UINT32_C(0x0055c2f2), &kind);
            if (status == VF2_OK && kind == UINT16_C(1)) label_source = UINT32_C(0x0004d30c);
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00503100), texture
                );
            }
            if (status == VF2_OK) status = phase17_zero_copy_text(machine,label_source,UINT32_C(0x010000e2));
            if (status == VF2_OK) status = vf2_model2a_read(machine,UINT32_C(0x0050002b),&display,1);
            if (display == UINT8_C(12) || display == UINT8_C(13)) name_dest=UINT32_C(0x010040ea);
            if (status == VF2_OK) status = phase17_zero_copy_text(machine,UINT32_C(0x0004d377)+texture*UINT32_C(32),name_dest);
        }
        if (status == VF2_OK) status = phase17_zero_render_decimal_inline(machine,(int32_t)texture,UINT32_C(0x0100028a),UINT32_C(0x00058374));
        if (status == VF2_OK) {
            uint32_t busy=0u; status=vf2_model2a_read_u32(machine,UINT32_C(0x00555000),&busy);
            if (status==VF2_OK) status=phase17_zero_copy_text(machine,busy==UINT32_C(1)?UINT32_C(0x0005839c):UINT32_C(0x00058390),UINT32_C(0x0100030a));
        }
        if (status == VF2_OK) status=phase17_zero_copy_text(machine,UINT32_C(0x000583b4),UINT32_C(0x0100038a));
        if (status == VF2_OK) status=phase17_zero_copy_text(machine,UINT32_C(0x000583d4),UINT32_C(0x0100040a));
        if (status == VF2_OK) status=phase17_zero_copy_text(machine,UINT32_C(0x000583f0),UINT32_C(0x0100048a));
        if (status == VF2_OK) status=phase17_zero_copy_text(machine,UINT32_C(0x0005840c),UINT32_C(0x0100050a));
        if (status == VF2_OK) status=phase17_zero_copy_text(machine,UINT32_C(0x00058428),UINT32_C(0x0100058a));
        if (status == VF2_OK) status=phase17_zero_copy_text(machine,UINT32_C(0x00058444),UINT32_C(0x0100060a));
        *final_g0=UINT32_C(0x0065766f); *final_g9=UINT32_C(0x0100068a); return status;
    }
    default:
        return VF2_ERROR_UNSUPPORTED;
    }
}

static vf2_status phase17_zero_render_index4_selector(
    vf2_model2a *machine,
    uint32_t *final_g0,
    uint32_t *final_g9
)
{
    int32_t value = 0;
    uint8_t selector = 0u;
    uint32_t source = 0u;
    vf2_status status = fill_tile_plane_spaces(
        machine, UINT32_C(0x01000298), UINT32_C(52), UINT32_C(1)
    );

    if (status == VF2_OK) {
        status = phase17_zero_read_s8(
            machine, UINT32_C(0x00508040), &value
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_render_decimal_inline(
            machine, value, UINT32_C(0x0100029c), UINT32_C(0x000570e4)
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_copy_text(
            machine, UINT32_C(0x000570f8), UINT32_C(0x0100028a)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x00508040), &selector, sizeof(selector)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            UINT32_C(0x00058d78) + (uint32_t)selector * UINT32_C(4),
            &source
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_copy_text(
            machine, source, UINT32_C(0x010002aa)
        );
    }
    if (status == VF2_OK) {
        *final_g0 = source;
        *final_g9 = UINT32_C(0x010002aa);
    }
    return status;
}

static vf2_status phase17_zero_render_index4_control(
    vf2_model2a *machine,
    uint32_t control,
    uint8_t selector,
    uint32_t *final_g0,
    uint32_t *final_g9
)
{
    int32_t value = 0;
    uint32_t source = 0u;
    vf2_status status = vf2_model2a_write(
        machine, control + UINT32_C(0x40), &selector, sizeof(selector)
    );

    if (status == VF2_OK) {
        status = fill_tile_plane_spaces(
            machine, UINT32_C(0x01000398), UINT32_C(52), UINT32_C(1)
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_read_s8(
            machine, control + UINT32_C(0x40), &value
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_render_decimal_inline(
            machine, value, UINT32_C(0x0100039c), UINT32_C(0x0005714c)
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_copy_text(
            machine, UINT32_C(0x00057160), UINT32_C(0x0100038a)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            UINT32_C(0x00058d78) + (uint32_t)selector * UINT32_C(4),
            &source
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_copy_text(
            machine, source, UINT32_C(0x010003aa)
        );
    }
    if (status == VF2_OK) {
        *final_g0 = source;
        *final_g9 = UINT32_C(0x010003aa);
    }
    return status;
}

static vf2_status phase17_zero_render_index4_entry(
    vf2_model2a *machine,
    uint32_t control,
    uint32_t *final_g0,
    uint32_t *final_g9
)
{
    int32_t value = 0;
    uint8_t selector = 0u;
    uint32_t source = 0u;
    vf2_status status = fill_tile_plane_spaces(
        machine, UINT32_C(0x01000298), UINT32_C(52), UINT32_C(1)
    );

    if (status == VF2_OK) {
        status = phase17_zero_read_s8(
            machine, UINT32_C(0x00508040), &value
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_render_decimal_inline(
            machine, value, UINT32_C(0x0100029c), UINT32_C(0x000570e4)
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_copy_text(
            machine, UINT32_C(0x000570f8), UINT32_C(0x0100028a)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x00508040), &selector, sizeof(selector)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            UINT32_C(0x00058d78) + (uint32_t)selector * UINT32_C(4),
            &source
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_copy_text(
            machine, source, UINT32_C(0x010002aa)
        );
    }
    if (status == VF2_OK) {
        status = fill_tile_plane_spaces(
            machine, UINT32_C(0x01000398), UINT32_C(52), UINT32_C(1)
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_read_s8(
            machine, control + UINT32_C(0x40), &value
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_render_decimal_inline(
            machine, value, UINT32_C(0x0100039c), UINT32_C(0x0005714c)
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_copy_text(
            machine, UINT32_C(0x00057160), UINT32_C(0x0100038a)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, control + UINT32_C(0x40), &selector, sizeof(selector)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            UINT32_C(0x00058d78) + (uint32_t)selector * UINT32_C(4),
            &source
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_copy_text(
            machine, source, UINT32_C(0x010003aa)
        );
    }
    if (status == VF2_OK) {
        *final_g0 = source;
        *final_g9 = UINT32_C(0x010003aa);
    }
    return status;
}

static vf2_status execute_frame_phase17_zero_control_menu(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    const uint32_t outer_stack = cpu->registers[1];
    const uint32_t entry_g11 =
        cpu->registers[VF2_I960_G0_REGISTER + 11u];
    const uint32_t entry_g12 =
        cpu->registers[VF2_I960_G0_REGISTER + 12u];
    vf2_hybrid_bridge_report text_report;
    uint32_t runtime_flags = 0u;
    uint32_t player0 = 0u;
    uint32_t player1 = 0u;
    uint32_t control = 0u;
    uint32_t descriptor = 0u;
    uint32_t descriptor_target = 0u;
    uint32_t table_target = 0u;
    uint32_t input_flags = 0u;
    uint32_t previous_flags = 0u;
    uint32_t navigation_flags = 0u;
    uint32_t input_descriptor = 0u;
    uint16_t fighter_control = 0u;
    uint8_t menu_index = 0u;
    uint8_t control_a = 0u;
    uint8_t control_b = 0u;
    uint64_t expected_instructions = 0u;
    uint64_t idle_wrapper_adjustment = 0u;
    uint32_t effective_input_flags = 0u;
    uint32_t effective_previous_flags = 0u;
    vf2_status status = VF2_OK;

    memset(&text_report, 0, sizeof(text_report));
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00508000), &runtime_flags
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500804), &player0
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500808), &player1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500814), &control
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050070c), &previous_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500700), &input_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500704), &navigation_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x00508008), &menu_index, sizeof(menu_index)
        );
    }
    if (status == VF2_OK &&
        (input_flags & (UINT32_C(1) << 5u)) != 0u &&
        (navigation_flags & ((UINT32_C(1) << 12u) |
                             (UINT32_C(1) << 13u))) != 0u) {
        const int forward =
            (navigation_flags & (UINT32_C(1) << 12u)) != 0u;
        uint8_t next_index = menu_index;
        uint32_t entry_target = 0u;
        uint32_t inline_source = 0u;
        uint32_t target13_flags = 0u;
        uint32_t transition_final_g0 = 0u;
        uint32_t transition_final_g9 = 0u;
        uint64_t transition_instructions = 0u;
        uint64_t transition_extra_calls = UINT64_C(3);
        uint64_t text_bytes = 0u;

        if (forward) {
            next_index = (uint8_t)(menu_index + UINT8_C(1));
            if (next_index >= UINT8_C(14)) {
                next_index = 0u;
            }
        } else {
            next_index = menu_index == 0u ? UINT8_C(13)
                                          : (uint8_t)(menu_index - UINT8_C(1));
        }
        if (next_index == UINT8_C(0)) {
            entry_target = UINT32_C(0x00055198);
            inline_source = UINT32_C(0x000551b4);
            transition_extra_calls = UINT64_C(6);
            transition_instructions =
                (runtime_flags & (UINT32_C(1) << 9u)) != 0u
                    ? (forward ? UINT64_C(12348) : UINT64_C(12346))
                    : (forward ? UINT64_C(12475) : UINT64_C(12473));
        } else if (next_index == UINT8_C(1)) {
            entry_target = UINT32_C(0x00055598);
            inline_source = UINT32_C(0x000555a8);
            transition_extra_calls = UINT64_C(11);
            transition_instructions = UINT64_C(13016);
        } else if (next_index == UINT8_C(2)) {
            entry_target = UINT32_C(0x0005570c);
            inline_source = UINT32_C(0x0005571c);
            transition_extra_calls = UINT64_C(9);
            transition_instructions = UINT64_C(12589);
        } else if (next_index == UINT8_C(3)) {
            entry_target = UINT32_C(0x000553e4);
            inline_source = UINT32_C(0x000553f4);
            transition_extra_calls = UINT64_C(13);
            transition_instructions = UINT64_C(12966);
        } else if (next_index == UINT8_C(4)) {
            entry_target = UINT32_C(0x00057098);
            inline_source = UINT32_C(0x000570a8);
            transition_extra_calls = UINT64_C(13);
            transition_instructions = UINT64_C(13243);
        } else if (next_index == UINT8_C(5)) {
            entry_target = UINT32_C(0x000557b4);
            inline_source = UINT32_C(0x000557c4);
            transition_extra_calls = UINT64_C(13);
            transition_instructions = UINT64_C(12972);
        } else if (next_index == UINT8_C(6)) {
            entry_target = UINT32_C(0x0005599c);
            inline_source = UINT32_C(0x000559d0);
            transition_extra_calls = UINT64_C(33);
            transition_instructions = UINT64_C(15150);
        } else if (next_index == UINT8_C(7)) {
            entry_target = UINT32_C(0x0005729c);
            inline_source = UINT32_C(0x000572ac);
            transition_extra_calls = UINT64_C(5);
            transition_instructions = UINT64_C(12416);
        } else if (next_index == UINT8_C(8)) {
            entry_target = UINT32_C(0x00057488);
            inline_source = UINT32_C(0x00057498);
            transition_instructions = UINT64_C(12254);
        } else if (next_index == UINT8_C(9)) {
            entry_target = UINT32_C(0x00057704);
            inline_source = UINT32_C(0x00057714);
            transition_extra_calls = UINT64_C(17);
            transition_instructions = UINT64_C(13481);
        } else if (next_index == UINT8_C(10)) {
            entry_target = UINT32_C(0x00057c60);
            inline_source = UINT32_C(0x00057c70);
            transition_extra_calls = UINT64_C(19);
            transition_instructions = UINT64_C(13386);
        } else if (next_index == UINT8_C(11)) {
            entry_target = UINT32_C(0x000575b8);
            inline_source = UINT32_C(0x000575c8);
            transition_instructions = UINT64_C(12249);
        } else if (next_index == UINT8_C(12)) {
            entry_target = UINT32_C(0x00057430);
            inline_source = UINT32_C(0x00057440);
            transition_extra_calls = UINT64_C(5);
            transition_instructions = UINT64_C(12365);
        } else if (next_index == UINT8_C(13)) {
            entry_target = UINT32_C(0x00057ff8);
            inline_source = UINT32_C(0x00058008);
            transition_instructions =
                (!forward && menu_index == 0u)
                    ? UINT64_C(12289)
                    : UINT64_C(12288);
        } else {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00055124) +
                (uint32_t)next_index * UINT32_C(8),
            &table_target
        );
        if (status != VF2_OK || table_target != entry_target ||
            (previous_flags & (UINT32_C(1) << 5u)) == 0u) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        status = write_u16(
            machine, UINT32_C(0x00500082), UINT16_C(0x8000)
        );
        if (status == VF2_OK) {
            const uint8_t zero = 0u;
            status = vf2_model2a_write(
                machine, control + UINT32_C(0xb0), &zero, sizeof(zero)
            );
        }
        if (status == VF2_OK) {
            uint8_t state = 0u;
            status = vf2_model2a_read(
                machine, UINT32_C(0x00500085), &state, sizeof(state)
            );
            state = (uint8_t)(state | UINT8_C(1));
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00500085), &state, sizeof(state)
                );
            }
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x00508008), &next_index, sizeof(next_index)
            );
        }
        if (status == VF2_OK) {
            status = fill_tile_plane_spaces(
                machine, UINT32_C(0x01000000), UINT32_C(62), UINT32_C(48)
            );
        }
        if (status == VF2_OK) {
            cpu->registers[VF2_I960_G0_REGISTER] = UINT32_C(62);
            cpu->registers[VF2_I960_G0_REGISTER + 1u] = 0u;
            cpu->registers[VF2_I960_G0_REGISTER + 6u] = control;
            cpu->registers[VF2_I960_G0_REGISTER + 7u] = player0;
            cpu->registers[VF2_I960_G0_REGISTER + 8u] = player1;
            cpu->registers[VF2_I960_G0_REGISTER + 9u] = UINT32_C(0x01001800);
            cpu->registers[VF2_I960_G14_REGISTER] = UINT32_C(0x000550d4);
        }
        if (status == VF2_OK &&
            !(next_index == UINT8_C(0) &&
              (runtime_flags & (UINT32_C(1) << 9u)) != 0u)) {
            cpu->registers[VF2_I960_G0_REGISTER + 9u] = UINT32_C(0x01000186);
            cpu->registers[14] = inline_source;
            cpu->ip = UINT32_C(0x00009444);
            status = execute_inline_text_thunk(machine, cpu, &text_report);
            text_bytes = (uint64_t)text_report.bytes_written;
        }
        if (status == VF2_OK &&
            (next_index == UINT8_C(1) || next_index == UINT8_C(2) ||
             next_index == UINT8_C(3) || next_index == UINT8_C(5) ||
             next_index == UINT8_C(6) || next_index == UINT8_C(7) ||
             next_index == UINT8_C(9) || next_index == UINT8_C(10) ||
             next_index == UINT8_C(12))) {
            status = phase17_zero_render_missing_body(
                machine, next_index, player0, player1, control,
                0u, 0u, runtime_flags, 1,
                &transition_final_g0, &transition_final_g9
            );
            if (status == VF2_OK) {
                cpu->registers[VF2_I960_G0_REGISTER] = transition_final_g0;
                cpu->registers[VF2_I960_G0_REGISTER + 9u] = transition_final_g9;
            }
        }
        if (status == VF2_OK && next_index == UINT8_C(4)) {
            status = phase17_zero_render_index4_entry(
                machine, control, &transition_final_g0, &transition_final_g9
            );
            if (status == VF2_OK) {
                cpu->registers[VF2_I960_G0_REGISTER] = transition_final_g0;
                cpu->registers[VF2_I960_G0_REGISTER + 9u] = transition_final_g9;
            }
        }
        if (status == VF2_OK && next_index == UINT8_C(0)) {
            uint8_t first_visit = 0u;
            uint32_t associated = 0u;
            uint32_t associated_flags = 0u;
            uint32_t fighter_flags = 0u;
            uint32_t input_control = 0u;
            uint32_t input_control_flags = 0u;
            uint8_t fighter_mode = 0u;
            const uint32_t fighters[2] = {player0, player1};
            const uint32_t associated_slots[2] = {
                UINT32_C(0x00500868), UINT32_C(0x0050086c)
            };
            uint32_t fighter_index = 0u;

            status = vf2_model2a_read(
                machine, UINT32_C(0x00508050), &first_visit,
                sizeof(first_visit)
            );
            if (status != VF2_OK || first_visit != 0u) {
                return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
            }
            first_visit = UINT8_C(1);
            status = vf2_model2a_write(
                machine, UINT32_C(0x00508050), &first_visit,
                sizeof(first_visit)
            );
            for (fighter_index = 0u; status == VF2_OK && fighter_index < 2u;
                 ++fighter_index) {
                status = vf2_model2a_read(
                    machine, fighters[fighter_index] + UINT32_C(0x1b0),
                    &fighter_mode, sizeof(fighter_mode)
                );
                if (status == VF2_OK) {
                    fighter_mode = fighter_mode >= UINT8_C(13)
                        ? (uint8_t)(fighter_mode - UINT8_C(13))
                        : fighter_mode;
                    status = vf2_model2a_write(
                        machine, fighters[fighter_index] + UINT32_C(0x1b1),
                        &fighter_mode, sizeof(fighter_mode)
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(
                        machine, fighters[fighter_index] + UINT32_C(0x0c),
                        UINT32_C(0x00013f08)
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(
                        machine, fighters[fighter_index], &fighter_flags
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(
                        machine, fighters[fighter_index],
                        (fighter_flags & UINT32_C(0xff000000)) |
                            (UINT32_C(1) << 31u)
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(
                        machine, associated_slots[fighter_index], &associated
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(
                        machine, associated, &associated_flags
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(
                        machine, associated,
                        associated_flags | (UINT32_C(1) << 31u)
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(
                        machine, associated + UINT32_C(0x0c),
                        UINT32_C(0x000640f4)
                    );
                }
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x0050083c), &input_control
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, input_control, &input_control_flags
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, input_control,
                    input_control_flags | (UINT32_C(1) << 3u)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500840), &input_control
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, input_control, &input_control_flags
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, input_control,
                    input_control_flags | (UINT32_C(1) << 3u)
                );
            }
            if (status == VF2_OK) {
                const uint8_t one = UINT8_C(1);
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00500056), &one, sizeof(one)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x0050081c), &descriptor
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, descriptor + UINT32_C(0x0c), &descriptor_target
                );
            }
            if (status != VF2_OK ||
                descriptor_target != UINT32_C(0x0001b9ac)) {
                return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, outer_stack + UINT32_C(128),
                    UINT32_C(0x000550d4)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, descriptor + UINT32_C(0x0c),
                    UINT32_C(0x0001b9dc)
                );
            }
            if (status == VF2_OK) {
                const uint8_t zero = 0u;
                status = vf2_model2a_write(
                    machine, player0 + UINT32_C(0x1200),
                    &zero, sizeof(zero)
                );
            }
            if (status == VF2_OK) {
                const uint8_t zero = 0u;
                status = vf2_model2a_write(
                    machine, player1 + UINT32_C(0x1200),
                    &zero, sizeof(zero)
                );
            }
            if (status == VF2_OK) {
                status = phase17_zero_initialize_control_fighter(
                    machine, player0
                );
            }
            if (status == VF2_OK) {
                status = phase17_zero_initialize_control_fighter(
                    machine, player1
                );
            }
            if (status == VF2_OK) {
                cpu->registers[VF2_I960_G0_REGISTER + 7u] = player1;
                cpu->registers[VF2_I960_G0_REGISTER + 8u] = player0;
                cpu->registers[VF2_I960_G0_REGISTER + 13u] = descriptor;
            }
        }
        if (status == VF2_OK && next_index == UINT8_C(13)) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00509800), 0u
            );
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00509804), 0u
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00509808), UINT32_C(0xbf07ae14)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x0050980c), UINT32_C(0x3fa28f5c)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00508000), &runtime_flags
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00508000),
                    runtime_flags & ~(UINT32_C(1) << 9u)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500068), &target13_flags
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500068),
                    target13_flags & ~(UINT32_C(1) << 16u)
                );
            }
        }
        if (status != VF2_OK) {
            return status;
        }
        if (next_index == UINT8_C(0)) {
            set_equal_condition(cpu);
            account_nested_procedure(cpu, UINT64_C(6), UINT64_C(6));
            if (cpu->maximum_local_frame_depth < cpu->local_frame_depth + 4u) {
                cpu->maximum_local_frame_depth = cpu->local_frame_depth + 4u;
            }
        } else {
            set_equal_condition(cpu);
            account_nested_procedure(
                cpu, transition_extra_calls, transition_extra_calls
            );
            if (cpu->maximum_local_frame_depth < cpu->local_frame_depth + 3u) {
                cpu->maximum_local_frame_depth = cpu->local_frame_depth + 3u;
            }
        }
        status = finish_recovered_procedure(
            machine, cpu,
            transition_instructions -
                (cpu->executed_instructions - start_instructions)
        );
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->changed_values = next_index == UINT8_C(13)
            ? UINT64_C(13) : UINT64_C(5);
        report->bytes_written =
            (size_t)(UINT32_C(62) * UINT32_C(48) * UINT32_C(2)) +
            (size_t)text_bytes + 5u +
            (next_index == UINT8_C(13) ? 24u : 0u);
        report->recovered_instruction_count = transition_instructions;
        report->recovered_procedure_calls =
            cpu->procedure_calls - start_calls;
        report->recovered_procedure_returns =
            cpu->procedure_returns - start_returns;
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
    if (status != VF2_OK) {
        return status;
    }
    effective_input_flags = input_flags & ~(UINT32_C(1) << 5u);
    effective_previous_flags = previous_flags & ~(UINT32_C(1) << 5u);
    if ((input_flags & (UINT32_C(1) << 5u)) != 0u) {
        uint8_t state = 0u;

        status = vf2_model2a_read(
            machine, UINT32_C(0x00500085), &state, sizeof(state)
        );
        if (status != VF2_OK) {
            return status;
        }
        if (menu_index == UINT8_C(13) && (state & UINT8_C(1)) == 0u) {
            uint32_t texture_flags = 0u;

            status = write_u16(
                machine, UINT32_C(0x00500082), UINT16_C(0x8000)
            );
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00508000),
                    runtime_flags & ~(UINT32_C(1) << 9u)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500068), &texture_flags
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500068),
                    texture_flags & ~(UINT32_C(1) << 16u)
                );
            }
            if (status != VF2_OK) {
                return status;
            }
            cpu->registers[VF2_I960_G0_REGISTER + 6u] = control;
            cpu->registers[VF2_I960_G0_REGISTER + 7u] = player0;
            cpu->registers[VF2_I960_G0_REGISTER + 8u] = player1;
            cpu->registers[VF2_I960_G14_REGISTER] = UINT32_C(0x000550d4);
            account_nested_procedure(cpu, UINT64_C(2), UINT64_C(2));
            if (cpu->maximum_local_frame_depth < cpu->local_frame_depth + 2u) {
                cpu->maximum_local_frame_depth = cpu->local_frame_depth + 2u;
            }
            status = finish_recovered_procedure(
                machine, cpu,
                UINT64_C(43) -
                    (cpu->executed_instructions - start_instructions)
            );
            if (status != VF2_OK) {
                return status;
            }
            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->changed_values = UINT64_C(3);
            report->bytes_written = 10u;
            report->recovered_instruction_count = UINT64_C(43);
            report->recovered_procedure_calls =
                cpu->procedure_calls - start_calls;
            report->recovered_procedure_returns =
                cpu->procedure_returns - start_returns;
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if ((state & UINT8_C(1)) != 0u) {
            status = write_u16(
                machine, UINT32_C(0x00500082), UINT16_C(0x8000)
            );
            if (status != VF2_OK) {
                return status;
            }
            cpu->registers[VF2_I960_G0_REGISTER + 6u] = control;
            cpu->registers[VF2_I960_G0_REGISTER + 7u] = player0;
            cpu->registers[VF2_I960_G0_REGISTER + 8u] = player1;
            cpu->registers[VF2_I960_G14_REGISTER] = 0u;
            account_nested_procedure(cpu, UINT64_C(2), UINT64_C(2));
            if (cpu->maximum_local_frame_depth < cpu->local_frame_depth + 2u) {
                cpu->maximum_local_frame_depth = cpu->local_frame_depth + 2u;
            }
            status = finish_recovered_procedure(
                machine, cpu,
                UINT64_C(27) -
                    (cpu->executed_instructions - start_instructions)
            );
            if (status != VF2_OK) {
                return status;
            }
            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->recovered_instruction_count = UINT64_C(27);
            report->recovered_procedure_calls =
                cpu->procedure_calls - start_calls;
            report->recovered_procedure_returns =
                cpu->procedure_returns - start_returns;
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        idle_wrapper_adjustment = UINT64_C(2);
    } else if (((input_flags ^ previous_flags) &
                (UINT32_C(1) << 5u)) != 0u) {
        uint8_t state = 0u;

        status = vf2_model2a_read(
            machine, UINT32_C(0x00500085), &state, sizeof(state)
        );
        if (status == VF2_OK) {
            state = (uint8_t)(state & ~UINT8_C(1));
            status = vf2_model2a_write(
                machine, UINT32_C(0x00500085), &state, sizeof(state)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        idle_wrapper_adjustment = UINT64_C(3);
    }
    if (menu_index == UINT8_C(1) || menu_index == UINT8_C(2) ||
        menu_index == UINT8_C(3) || menu_index == UINT8_C(5) ||
        menu_index == UINT8_C(6) || menu_index == UINT8_C(7) ||
        menu_index == UINT8_C(9) || menu_index == UINT8_C(10) ||
        menu_index == UINT8_C(12) || menu_index == UINT8_C(13)) {
        static const uint32_t idle_targets[14] = {
            UINT32_C(0x00055330), UINT32_C(0x000555c4),
            UINT32_C(0x0005572c), UINT32_C(0x00055408),
            UINT32_C(0x00057188), UINT32_C(0x000557d8),
            UINT32_C(0x00055a70), UINT32_C(0x000572bc),
            UINT32_C(0x000574a4), UINT32_C(0x00057724),
            UINT32_C(0x00057c7c), UINT32_C(0x000575d4),
            UINT32_C(0x00057450), UINT32_C(0x0005804c)
        };
        static const uint64_t instruction_counts[14] = {
            UINT64_C(267), UINT64_C(534), UINT64_C(318), UINT64_C(695),
            UINT64_C(37), UINT64_C(695), UINT64_C(3063), UINT64_C(146),
            UINT64_C(45), UINT64_C(1282), UINT64_C(1154), UINT64_C(40),
            UINT64_C(118), UINT64_C(1745)
        };
        static const uint64_t nested_calls[14] = {
            UINT64_C(6), UINT64_C(9), UINT64_C(8), UINT64_C(12),
            UINT64_C(2), UINT64_C(13), UINT64_C(37), UINT64_C(4),
            UINT64_C(2), UINT64_C(17), UINT64_C(18), UINT64_C(2),
            UINT64_C(4), UINT64_C(16)
        };
        static const uint32_t depth_deltas[14] = {
            UINT32_C(4), UINT32_C(3), UINT32_C(3), UINT32_C(3),
            UINT32_C(2), UINT32_C(3), UINT32_C(3), UINT32_C(3),
            UINT32_C(2), UINT32_C(3), UINT32_C(3), UINT32_C(2),
            UINT32_C(3), UINT32_C(4)
        };
        uint32_t final_g0 = 0u;
        uint32_t final_g1 = 0u;
        uint32_t final_g9 = 0u;
        int32_t control_instruction_adjustment = 0;
        uint64_t active_nested_calls = nested_calls[menu_index];

        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00055128) +
                (uint32_t)menu_index * UINT32_C(8),
            &table_target
        );
        if (status != VF2_OK || table_target != idle_targets[menu_index]) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        if (effective_previous_flags != effective_input_flags) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (menu_index == UINT8_C(3)) {
            if (navigation_flags != 0u) {
                return VF2_ERROR_UNSUPPORTED;
            }
            switch (effective_input_flags) {
            case 0u:
                break;
            case UINT32_C(1) << 12u:
            case UINT32_C(1) << 13u:
            case UINT32_C(1) << 14u:
            case UINT32_C(1) << 15u:
                control_instruction_adjustment = 5;
                break;
            case UINT32_C(1) << 9u:
            case (UINT32_C(1) << 9u) | (UINT32_C(1) << 12u):
            case (UINT32_C(1) << 9u) | (UINT32_C(1) << 13u):
                control_instruction_adjustment = -7;
                break;
            case (UINT32_C(1) << 9u) | (UINT32_C(1) << 14u):
                control_instruction_adjustment = 4;
                break;
            case (UINT32_C(1) << 9u) | (UINT32_C(1) << 15u):
                control_instruction_adjustment = 9;
                break;
            default:
                return VF2_ERROR_UNSUPPORTED;
            }
        } else if (menu_index == UINT8_C(5)) {
            if (navigation_flags != 0u) {
                return VF2_ERROR_UNSUPPORTED;
            }
            switch (effective_input_flags) {
            case 0u:
                break;
            case UINT32_C(1) << 8u:
                control_instruction_adjustment = -1;
                break;
            case UINT32_C(1) << 9u:
                control_instruction_adjustment = 2;
                break;
            case UINT32_C(1) << 12u:
            case UINT32_C(1) << 13u:
            case UINT32_C(1) << 14u:
            case UINT32_C(1) << 15u:
                control_instruction_adjustment = 5;
                break;
            case (UINT32_C(1) << 8u) | (UINT32_C(1) << 12u):
            case (UINT32_C(1) << 8u) | (UINT32_C(1) << 13u):
                control_instruction_adjustment = 4;
                break;
            case (UINT32_C(1) << 9u) | (UINT32_C(1) << 12u):
            case (UINT32_C(1) << 9u) | (UINT32_C(1) << 13u):
            case (UINT32_C(1) << 9u) | (UINT32_C(1) << 14u):
            case (UINT32_C(1) << 9u) | (UINT32_C(1) << 15u):
                control_instruction_adjustment = 7;
                break;
            default:
                return VF2_ERROR_UNSUPPORTED;
            }
        } else if (menu_index == UINT8_C(6)) {
            uint8_t camera_mode = 0u;
            uint8_t de_flags = 0u;
            const uint32_t supported_input =
                (UINT32_C(1) << 8u) | (UINT32_C(1) << 9u) |
                (UINT32_C(1) << 12u) | (UINT32_C(1) << 13u) |
                (UINT32_C(1) << 14u) | (UINT32_C(1) << 15u) |
                (UINT32_C(1) << 16u) | (UINT32_C(1) << 17u) |
                (UINT32_C(1) << 18u) |
                (UINT32_C(1) << 20u) | (UINT32_C(1) << 21u) |
                (UINT32_C(1) << 22u) | (UINT32_C(1) << 23u);
            const uint32_t supported_navigation =
                (UINT32_C(1) << 4u) | (UINT32_C(1) << 5u) |
                (UINT32_C(1) << 8u) | (UINT32_C(1) << 9u) |
                (UINT32_C(1) << 10u) |
                (UINT32_C(1) << 12u) | (UINT32_C(1) << 13u) |
                (UINT32_C(1) << 14u) | (UINT32_C(1) << 15u) |
                (UINT32_C(1) << 16u) | (UINT32_C(1) << 17u);

            status = vf2_model2a_read(
                machine, control + UINT32_C(0xb1),
                &camera_mode, sizeof(camera_mode)
            );
            if (status == VF2_OK) {
                status = vf2_model2a_read(
                    machine, control + UINT32_C(0xde),
                    &de_flags, sizeof(de_flags)
                );
            }
            if (status != VF2_OK ||
                (effective_input_flags & ~supported_input) != 0u ||
                (navigation_flags & ~supported_navigation) != 0u) {
                return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
            }
            if (camera_mode == UINT8_C(2)) {
                const uint32_t mode2_inputs =
                    (UINT32_C(1) << 12u) | (UINT32_C(1) << 13u) |
                    (UINT32_C(1) << 14u) | (UINT32_C(1) << 15u) |
                    (UINT32_C(1) << 16u) | (UINT32_C(1) << 17u) |
                    (UINT32_C(1) << 18u) |
                    (UINT32_C(1) << 20u) | (UINT32_C(1) << 21u) |
                    (UINT32_C(1) << 22u) | (UINT32_C(1) << 23u);
                const uint32_t mode2_navigation =
                    (UINT32_C(1) << 5u) |
                    (UINT32_C(1) << 8u) | (UINT32_C(1) << 9u) |
                    (UINT32_C(1) << 10u) |
                    (UINT32_C(1) << 16u) | (UINT32_C(1) << 17u);

                if ((effective_input_flags & ~mode2_inputs) != 0u ||
                    (navigation_flags & ~mode2_navigation) != 0u) {
                    return VF2_ERROR_UNSUPPORTED;
                }
                control_instruction_adjustment = -168;
                active_nested_calls = UINT64_C(36);
                final_g1 = navigation_flags;
                if ((de_flags & UINT8_C(1)) != 0u) {
                    control_instruction_adjustment += 4;
                }
                if ((effective_input_flags & (UINT32_C(1) << 13u)) != 0u) {
                    control_instruction_adjustment += 1;
                } else if ((effective_input_flags & (UINT32_C(1) << 12u)) != 0u) {
                    control_instruction_adjustment += 3;
                }
                if ((effective_input_flags & (UINT32_C(1) << 14u)) != 0u) {
                    control_instruction_adjustment += 1;
                } else if ((effective_input_flags & (UINT32_C(1) << 15u)) != 0u) {
                    control_instruction_adjustment += 3;
                }
                if ((effective_input_flags & ((UINT32_C(1) << 22u) |
                                              (UINT32_C(1) << 23u))) != 0u) {
                    control_instruction_adjustment += 3;
                }
                if ((effective_input_flags & (UINT32_C(1) << 18u)) != 0u &&
                    (de_flags & UINT8_C(1)) == 0u) {
                    control_instruction_adjustment -= 3;
                }
                if ((de_flags & UINT8_C(1)) != 0u &&
                    (effective_input_flags & ((UINT32_C(1) << 20u) |
                                              (UINT32_C(1) << 21u))) != 0u) {
                    control_instruction_adjustment += 3;
                }
                if ((navigation_flags & (UINT32_C(1) << 5u)) != 0u) {
                    control_instruction_adjustment += 23;
                }
                if ((navigation_flags & (UINT32_C(1) << 9u)) != 0u) {
                    control_instruction_adjustment += 18;
                } else if ((navigation_flags & (UINT32_C(1) << 8u)) != 0u) {
                    control_instruction_adjustment += 75;
                }
                if ((navigation_flags & ((UINT32_C(1) << 16u) |
                                         (UINT32_C(1) << 17u))) != 0u &&
                    ((effective_input_flags & (UINT32_C(1) << 18u)) == 0u ||
                     (de_flags & UINT8_C(1)) != 0u)) {
                    control_instruction_adjustment += 19;
                }
            }
            if (camera_mode != UINT8_C(2) &&
                (navigation_flags & (UINT32_C(1) << 4u)) != 0u) {
                control_instruction_adjustment -= 34;
                active_nested_calls += UINT64_C(1);
            }
            if (camera_mode != UINT8_C(2) &&
                (navigation_flags & (UINT32_C(1) << 5u)) != 0u) {
                control_instruction_adjustment += 23;
            }
            if ((navigation_flags & (UINT32_C(1) << 10u)) != 0u) {
                control_instruction_adjustment = camera_mode == UINT8_C(2)
                    ? 9287 : 9456;
                active_nested_calls = camera_mode == UINT8_C(2)
                    ? UINT64_C(8) : UINT64_C(9);
                if (camera_mode == UINT8_C(2)) {
                    final_g1 = 0u;
                }
            }
            if ((runtime_flags & (UINT32_C(1) << 9u)) != 0u) {
                control_instruction_adjustment = camera_mode == UINT8_C(2)
                    ? 9284 : 9457;
                active_nested_calls = camera_mode == UINT8_C(2)
                    ? UINT64_C(8) : UINT64_C(9);
                if (camera_mode == UINT8_C(2)) {
                    final_g1 = 0u;
                }
            }
            if (camera_mode != UINT8_C(2)) {
                if ((effective_input_flags & (UINT32_C(1) << 13u)) != 0u) {
                    control_instruction_adjustment += 1;
                } else if ((effective_input_flags & (UINT32_C(1) << 12u)) != 0u) {
                    control_instruction_adjustment += 3;
                }
                if ((effective_input_flags & (UINT32_C(1) << 14u)) != 0u) {
                    control_instruction_adjustment += 1;
                } else if ((effective_input_flags & (UINT32_C(1) << 15u)) != 0u) {
                    control_instruction_adjustment += 3;
                }
                if ((effective_input_flags & ((UINT32_C(1) << 8u) |
                                              (UINT32_C(1) << 9u))) != 0u) {
                    control_instruction_adjustment += 2;
                }
                if ((effective_input_flags & (UINT32_C(1) << 21u)) != 0u) {
                    control_instruction_adjustment += 1;
                } else if ((effective_input_flags & (UINT32_C(1) << 20u)) != 0u) {
                    control_instruction_adjustment += 3;
                }
                if ((effective_input_flags & (UINT32_C(1) << 22u)) != 0u) {
                    control_instruction_adjustment += 1;
                } else if ((effective_input_flags & (UINT32_C(1) << 23u)) != 0u) {
                    control_instruction_adjustment += 3;
                }
                if ((effective_input_flags & ((UINT32_C(1) << 16u) |
                                              (UINT32_C(1) << 17u))) != 0u) {
                    control_instruction_adjustment += 2;
                }
            }
        } else if (menu_index == UINT8_C(9)) {
            if (effective_input_flags != 0u) {
                return VF2_ERROR_UNSUPPORTED;
            }
            switch (navigation_flags) {
            case 0u:
                break;
            case UINT32_C(1) << 8u:
                control_instruction_adjustment = 8;
                break;
            case UINT32_C(1) << 9u:
                control_instruction_adjustment = 2;
                break;
            default:
                return VF2_ERROR_UNSUPPORTED;
            }
        } else if (menu_index == UINT8_C(10)) {
            switch (navigation_flags) {
            case 0u:
                break;
            case UINT32_C(0x700):
                if (effective_input_flags != 0u) {
                    return VF2_ERROR_UNSUPPORTED;
                }
                control_instruction_adjustment = -7;
                break;
            default:
                return VF2_ERROR_UNSUPPORTED;
            }
            if (navigation_flags == 0u) {
                switch (effective_input_flags) {
                case 0u:
                case UINT32_C(1) << 8u:
                    break;
                case UINT32_C(1) << 9u:
                    control_instruction_adjustment = -3;
                    break;
                case UINT32_C(1) << 12u:
                case UINT32_C(1) << 13u:
                case UINT32_C(1) << 14u:
                case UINT32_C(1) << 15u:
                    control_instruction_adjustment = 5;
                    break;
                case (UINT32_C(1) << 9u) | (UINT32_C(1) << 14u):
                    control_instruction_adjustment = 8;
                    break;
                case (UINT32_C(1) << 9u) | (UINT32_C(1) << 15u):
                    control_instruction_adjustment = 13;
                    break;
                default:
                    return VF2_ERROR_UNSUPPORTED;
                }
            }
        } else if (menu_index == UINT8_C(12)) {
            if (navigation_flags != 0u) {
                return VF2_ERROR_UNSUPPORTED;
            }
            switch (effective_input_flags) {
            case 0u:
                break;
            case UINT32_C(1) << 12u:
            case UINT32_C(1) << 13u:
                control_instruction_adjustment = 1;
                break;
            default:
                return VF2_ERROR_UNSUPPORTED;
            }
        } else if (menu_index == UINT8_C(13)) {
            switch (navigation_flags) {
            case 0u:
                break;
            case UINT32_C(1) << 14u:
                if (effective_input_flags != 0u) {
                    return VF2_ERROR_UNSUPPORTED;
                }
                control_instruction_adjustment = -7;
                break;
            case UINT32_C(1) << 15u:
                if (effective_input_flags != 0u) {
                    return VF2_ERROR_UNSUPPORTED;
                }
                control_instruction_adjustment = 1;
                break;
            default:
                return VF2_ERROR_UNSUPPORTED;
            }
            if (navigation_flags == 0u) {
                switch (effective_input_flags) {
                case 0u:
                    break;
                case UINT32_C(1) << 16u:
                case UINT32_C(1) << 18u:
                case UINT32_C(1) << 20u:
                case UINT32_C(1) << 21u:
                case UINT32_C(1) << 22u:
                case UINT32_C(1) << 23u:
                    control_instruction_adjustment = 7;
                    break;
                default:
                    return VF2_ERROR_UNSUPPORTED;
                }
            }
        } else if (effective_input_flags != 0u || navigation_flags != 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = phase17_zero_render_missing_body(
            machine, menu_index, player0, player1, control,
            effective_input_flags, navigation_flags, runtime_flags, 0,
            &final_g0, &final_g9
        );
        if (status == VF2_OK) {
            status = write_u16(
                machine, UINT32_C(0x00500082), UINT16_C(0x8000)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[VF2_I960_G0_REGISTER] = final_g0;
        cpu->registers[VF2_I960_G0_REGISTER + 1u] = final_g1;
        cpu->registers[VF2_I960_G0_REGISTER + 2u] = 0u;
        cpu->registers[VF2_I960_G0_REGISTER + 3u] = 0u;
        cpu->registers[VF2_I960_G0_REGISTER + 4u] = 0u;
        cpu->registers[VF2_I960_G0_REGISTER + 5u] = 0u;
        cpu->registers[VF2_I960_G0_REGISTER + 6u] = control;
        cpu->registers[VF2_I960_G0_REGISTER + 7u] = player0;
        cpu->registers[VF2_I960_G0_REGISTER + 8u] = player1;
        cpu->registers[VF2_I960_G0_REGISTER + 9u] = final_g9;
        cpu->registers[VF2_I960_G0_REGISTER + 10u] = 0u;
        cpu->registers[VF2_I960_G0_REGISTER + 11u] = entry_g11;
        cpu->registers[VF2_I960_G0_REGISTER + 12u] = entry_g12;
        cpu->registers[VF2_I960_G0_REGISTER + 13u] = 0u;
        cpu->registers[VF2_I960_G14_REGISTER] = UINT32_C(0x000550d4);
        cpu->registers[VF2_I960_G0_REGISTER + 15u] = 0u;
        set_equal_condition(cpu);
        account_nested_procedure(
            cpu, active_nested_calls, active_nested_calls
        );
        if (cpu->maximum_local_frame_depth <
            cpu->local_frame_depth + depth_deltas[menu_index]) {
            cpu->maximum_local_frame_depth =
                cpu->local_frame_depth + depth_deltas[menu_index];
        }
        expected_instructions = (uint64_t)(
            (int64_t)instruction_counts[menu_index] +
            (int64_t)idle_wrapper_adjustment +
            (int64_t)control_instruction_adjustment
        );
        status = finish_recovered_procedure(
            machine, cpu,
            expected_instructions -
                (cpu->executed_instructions - start_instructions)
        );
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->recovered_instruction_count = expected_instructions;
        report->recovered_procedure_calls =
            cpu->procedure_calls - start_calls;
        report->recovered_procedure_returns =
            cpu->procedure_returns - start_returns;
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
    if (menu_index == UINT8_C(4) || menu_index == UINT8_C(8) ||
        menu_index == UINT8_C(11)) {
        uint32_t expected_target = 0u;

        if (menu_index == UINT8_C(4)) {
            expected_target = UINT32_C(0x00057188);
            expected_instructions = UINT64_C(37) + idle_wrapper_adjustment;
        } else if (menu_index == UINT8_C(8)) {
            expected_target = UINT32_C(0x000574a4);
            expected_instructions = UINT64_C(45) + idle_wrapper_adjustment;
        } else {
            expected_target = UINT32_C(0x000575d4);
            expected_instructions = UINT64_C(40) + idle_wrapper_adjustment;
        }
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00055128) +
                (uint32_t)menu_index * UINT32_C(8),
            &table_target
        );
        if (status != VF2_OK || table_target != expected_target) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        if (menu_index == UINT8_C(4)) {
            uint8_t selector = 0u;
            uint32_t final_g0 = 0u;
            uint32_t final_g9 = 0u;
            uint64_t active_nested_calls = UINT64_C(2);
            uint32_t active_depth_delta = UINT32_C(2);
            const uint32_t supported_input =
                (UINT32_C(1) << 12u) | (UINT32_C(1) << 13u);
            const uint32_t supported_navigation =
                (UINT32_C(1) << 8u) | (UINT32_C(1) << 12u) |
                (UINT32_C(1) << 13u);

            if ((effective_input_flags & ~supported_input) != 0u ||
                effective_previous_flags != effective_input_flags ||
                (navigation_flags & ~supported_navigation) != 0u) {
                return VF2_ERROR_UNSUPPORTED;
            }
            if (navigation_flags != 0u) {
                status = vf2_model2a_read(
                    machine, UINT32_C(0x00508040), &selector,
                    sizeof(selector)
                );
            }
            if (status == VF2_OK &&
                (navigation_flags & (UINT32_C(1) << 12u)) != 0u) {
                selector = (uint8_t)(selector + UINT8_C(1));
                if (selector >= UINT8_C(17)) {
                    selector = 0u;
                }
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00508040), &selector,
                    sizeof(selector)
                );
                if (status == VF2_OK) {
                    status = phase17_zero_render_index4_selector(
                        machine, &final_g0, &final_g9
                    );
                }
                expected_instructions =
                    UINT64_C(555) + idle_wrapper_adjustment;
                active_nested_calls = UINT64_C(7);
                active_depth_delta = UINT32_C(3);
            } else if (status == VF2_OK &&
                       (navigation_flags & (UINT32_C(1) << 13u)) != 0u) {
                selector = selector == 0u
                    ? UINT8_C(16)
                    : (uint8_t)(selector - UINT8_C(1));
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00508040), &selector,
                    sizeof(selector)
                );
                if (status == VF2_OK) {
                    status = phase17_zero_render_index4_selector(
                        machine, &final_g0, &final_g9
                    );
                }
                expected_instructions =
                    UINT64_C(500) + idle_wrapper_adjustment;
                active_nested_calls = UINT64_C(7);
                active_depth_delta = UINT32_C(3);
            } else if (status == VF2_OK &&
                       (navigation_flags & (UINT32_C(1) << 8u)) != 0u) {
                status = phase17_zero_render_index4_control(
                    machine, control, selector, &final_g0, &final_g9
                );
                expected_instructions =
                    UINT64_C(471) + idle_wrapper_adjustment;
                active_nested_calls = UINT64_C(7);
                active_depth_delta = UINT32_C(3);
            }
            if (status != VF2_OK) {
                return status;
            }
            if (navigation_flags != 0u) {
                cpu->registers[VF2_I960_G0_REGISTER] = final_g0;
                cpu->registers[VF2_I960_G0_REGISTER + 9u] = final_g9;
                set_equal_condition(cpu);
            }
            account_nested_procedure(
                cpu, active_nested_calls, active_nested_calls
            );
            if (cpu->maximum_local_frame_depth <
                cpu->local_frame_depth + active_depth_delta) {
                cpu->maximum_local_frame_depth =
                    cpu->local_frame_depth + active_depth_delta;
            }
        } else {
            const uint32_t supported_input =
                (UINT32_C(1) << 14u) | (UINT32_C(1) << 15u);
            const uint32_t supported_navigation =
                (UINT32_C(1) << 8u) | (UINT32_C(1) << 9u);
            if ((effective_input_flags & ~supported_input) != 0u ||
                effective_previous_flags != effective_input_flags ||
                (navigation_flags & ~supported_navigation) != 0u) {
                return VF2_ERROR_UNSUPPORTED;
            }
            if ((navigation_flags & (UINT32_C(1) << 8u)) != 0u) {
                status = vf2_model2a_write_u32(
                    machine, player0 + UINT32_C(0x194), UINT32_C(4)
                );
                expected_instructions += UINT64_C(2);
            }
            if (status == VF2_OK &&
                (navigation_flags & (UINT32_C(1) << 9u)) != 0u) {
                status = vf2_model2a_write_u32(
                    machine, player0 + UINT32_C(0x194), UINT32_C(1)
                );
                expected_instructions += UINT64_C(2);
            }
            if (status == VF2_OK && menu_index == UINT8_C(8)) {
                uint16_t value = 0u;
                status = read_u16(
                    machine, player0 + UINT32_C(0x158), &value
                );
                if (status == VF2_OK &&
                    (effective_input_flags & (UINT32_C(1) << 15u)) != 0u) {
                    if (value > UINT16_C(0xa000)) {
                        value = (uint16_t)(value - UINT16_C(192));
                        status = write_u16(
                            machine, player0 + UINT32_C(0x158), value
                        );
                        expected_instructions += UINT64_C(5);
                    }
                    expected_instructions -= UINT64_C(1);
                } else if (status == VF2_OK &&
                           (effective_input_flags & (UINT32_C(1) << 14u)) != 0u) {
                    if (value < UINT16_C(0xff00)) {
                        value = (uint16_t)(value + UINT16_C(192));
                        status = write_u16(
                            machine, player0 + UINT32_C(0x158), value
                        );
                        expected_instructions += UINT64_C(5);
                    }
                }
            } else if (status == VF2_OK && menu_index == UINT8_C(11)) {
                uint16_t value = 0u;
                status = read_u16(
                    machine, player0 + UINT32_C(0x17c), &value
                );
                if (status == VF2_OK &&
                    (effective_input_flags & (UINT32_C(1) << 15u)) != 0u) {
                    value = (uint16_t)(value - UINT16_C(192));
                    status = write_u16(
                        machine, player0 + UINT32_C(0x17c), value
                    );
                    expected_instructions += UINT64_C(1);
                } else if (status == VF2_OK &&
                           (effective_input_flags & (UINT32_C(1) << 14u)) != 0u) {
                    value = (uint16_t)(value + UINT16_C(192));
                    status = write_u16(
                        machine, player0 + UINT32_C(0x17c), value
                    );
                    expected_instructions += UINT64_C(2);
                }
            }
            if (status != VF2_OK) {
                return status;
            }
        }
        status = write_u16(
            machine, UINT32_C(0x00500082), UINT16_C(0x8000)
        );
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[VF2_I960_G0_REGISTER + 6u] = control;
        cpu->registers[VF2_I960_G0_REGISTER + 7u] = player0;
        cpu->registers[VF2_I960_G0_REGISTER + 8u] = player1;
        cpu->registers[VF2_I960_G14_REGISTER] = UINT32_C(0x000550d4);
        if (menu_index != UINT8_C(4)) {
            account_nested_procedure(cpu, UINT64_C(2), UINT64_C(2));
            if (cpu->maximum_local_frame_depth < cpu->local_frame_depth + 2u) {
                cpu->maximum_local_frame_depth = cpu->local_frame_depth + 2u;
            }
        }
        status = finish_recovered_procedure(
            machine, cpu,
            expected_instructions -
                (cpu->executed_instructions - start_instructions)
        );
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->changed_values = UINT64_C(1);
        report->bytes_written = sizeof(uint16_t);
        report->recovered_instruction_count = expected_instructions;
        report->recovered_procedure_calls =
            cpu->procedure_calls - start_calls;
        report->recovered_procedure_returns =
            cpu->procedure_returns - start_returns;
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
    if (menu_index != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00055128), &table_target
    );
    if (status != VF2_OK || table_target != UINT32_C(0x00055330)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0050081c), &descriptor
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, descriptor + UINT32_C(0x0c), &descriptor_target
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500860), &input_descriptor
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x0050a0b8), &control_a, sizeof(control_a)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x0050a0b9), &control_b, sizeof(control_b)
        );
    }
    if (status != VF2_OK || descriptor_target != UINT32_C(0x0001b9ac) ||
        (input_descriptor & (UINT32_C(1) << 28u)) != 0u ||
        control_a != 0u || control_b != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    {
        uint32_t input_control = 0u;
        uint32_t input_control_word = 0u;
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050083c), &input_control
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, input_control, &input_control_word
            );
        }
        if (status != VF2_OK ||
            (input_control_word & (UINT32_C(1) << 3u)) != 0u) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500840), &input_control
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, input_control, &input_control_word
            );
        }
        if (status != VF2_OK ||
            (input_control_word & (UINT32_C(1) << 3u)) != 0u) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
    }

    status = write_u16(
        machine, UINT32_C(0x00500082), UINT16_C(0x8000)
    );
    if (status == VF2_OK) {
        cpu->registers[14] =
            (runtime_flags & (UINT32_C(1) << 9u)) != 0u
                ? UINT32_C(0x00055370)
                : UINT32_C(0x0005534c);
        cpu->registers[25] = UINT32_C(0x01000186);
        cpu->ip = UINT32_C(0x00009444);
        status = execute_inline_text_thunk(machine, cpu, &text_report);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x005000a0), &fighter_control,
            sizeof(fighter_control)
        );
    }
    if (status == VF2_OK) {
        status = write_u16(
            machine, player0 + UINT32_C(0x1ac), fighter_control
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, player0, &previous_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, player0, previous_flags & ~(UINT32_C(1) << 5u)
        );
    }
    if (status == VF2_OK) {
        status = write_u16(
            machine, player1 + UINT32_C(0x1ac), fighter_control
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, player1, &previous_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, player1, previous_flags & ~(UINT32_C(1) << 5u)
        );
    }
    if (status == VF2_OK) {
        cpu->registers[VF2_I960_G14_REGISTER] = UINT32_C(0x000550d4);
        status = vf2_model2a_write_u32(
            machine, outer_stack + UINT32_C(128),
            cpu->registers[VF2_I960_G14_REGISTER]
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, descriptor + UINT32_C(0x0c), UINT32_C(0x0001b9dc)
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_initialize_control_fighter(machine, player0);
    }
    if (status == VF2_OK) {
        status = phase17_zero_initialize_control_fighter(machine, player1);
    }
    if (status == VF2_OK) {
        status = write_u16(
            machine, UINT32_C(0x00500082), UINT16_C(0x8000)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[VF2_I960_G0_REGISTER + 6u] = control;
    cpu->registers[VF2_I960_G0_REGISTER + 7u] = player1;
    cpu->registers[VF2_I960_G0_REGISTER + 8u] = player0;
    cpu->registers[VF2_I960_G0_REGISTER + 13u] = descriptor;
    cpu->registers[VF2_I960_G14_REGISTER] = UINT32_C(0x000550d4);
    set_equal_condition(cpu);
    account_nested_procedure(cpu, UINT64_C(5), UINT64_C(5));
    if (cpu->maximum_local_frame_depth < cpu->local_frame_depth + 4u) {
        cpu->maximum_local_frame_depth = cpu->local_frame_depth + 4u;
    }
    expected_instructions =
        ((runtime_flags & (UINT32_C(1) << 9u)) != 0u
             ? UINT64_C(266)
             : UINT64_C(267)) +
        idle_wrapper_adjustment;
    status = finish_recovered_procedure(
        machine, cpu,
        expected_instructions -
            (cpu->executed_instructions - start_instructions)
    );
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(32);
    report->bytes_written = 80u + text_report.bytes_written;
    report->recovered_instruction_count = expected_instructions;
    report->recovered_procedure_calls =
        cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns =
        cpu->procedure_returns - start_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_frame_phase17(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint32_t saved_g4 =
        cpu->registers[VF2_I960_G0_REGISTER + 4u];
    uint32_t base = 0u;
    uint32_t player0 = 0u;
    uint32_t player1 = 0u;
    uint32_t gameplay_flags = 0u;
    uint32_t phase_pointer_holder = 0u;
    uint32_t previous_phase_target = 0u;
    uint32_t next_phase_target = 0u;
    uint32_t phase_object = 0u;
    uint32_t phase_text_base = 0u;
    uint32_t phase_text_source = 0u;
    uint32_t phase_text_destination = 0u;
    uint64_t phase_text_characters = 0u;
    uint8_t phase_index = 0u;
    uint8_t next_phase_index = 0u;
    uint8_t phase_state = 0u;
    int phase_step_forward = 0;
    int phase_step_back = 0;
    int phase_step = 0;
    int phase_reset = 0;
    uint64_t recovered_instructions = UINT64_C(36);
    vf2_status status = VF2_OK;

    status = vf2_model2a_read(
        machine, UINT32_C(0x005000a6), &phase_state, sizeof(phase_state)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x005000a4), &phase_index,
            sizeof(phase_index)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500704), &gameplay_flags
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if (phase_state == 0u) {
        return execute_frame_phase17_zero_control_menu(machine, cpu, report);
    }
    if ((phase_index & UINT8_C(0x80)) != 0u) {
        return execute_frame_phase17_bit7_index11(
            machine, cpu, report, saved_g4, phase_index
        );
    }
    phase_step_forward =
        (gameplay_flags & UINT32_C(0x08001008)) != 0u;
    phase_step_back =
        !phase_step_forward &&
        (gameplay_flags & UINT32_C(0x00002000)) != 0u;
    phase_step = phase_step_forward || phase_step_back;
    phase_reset =
        (gameplay_flags & UINT32_C(0x04000104)) != 0u;
    if (phase_step_forward) {
        recovered_instructions =
            phase_index >= UINT8_C(11)
                ? UINT64_C(50)
                : UINT64_C(49);
    } else if (phase_step_back) {
        recovered_instructions =
            phase_index == 0u ? UINT64_C(50) : UINT64_C(49);
    }

    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0050016c), &base
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500804), &player0
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500808), &player1
        );
    }
    if (phase_step_forward) {
        next_phase_index = phase_index >= UINT8_C(11)
            ? UINT8_C(0)
            : (uint8_t)(phase_index + UINT8_C(1));
    } else if (phase_step_back) {
        next_phase_index = phase_index == 0u
            ? UINT8_C(11)
            : (uint8_t)(phase_index - UINT8_C(1));
    }
    if (status == VF2_OK && phase_step) {
        status = vf2_model2a_read_u32(
            machine,
            UINT32_C(0x0005feac) +
                (uint32_t)phase_index * UINT32_C(8),
            &phase_pointer_holder
        );
    }
    if (status == VF2_OK && phase_step) {
        status = vf2_model2a_read_u32(
            machine, phase_pointer_holder, &previous_phase_target
        );
    }
    if (status == VF2_OK && phase_step) {
        status = vf2_model2a_read_u32(
            machine,
            UINT32_C(0x0005feac) +
                (uint32_t)next_phase_index * UINT32_C(8),
            &phase_pointer_holder
        );
    }
    if (status == VF2_OK && phase_step) {
        status = vf2_model2a_read_u32(
            machine, phase_pointer_holder, &next_phase_target
        );
    }
    if (status == VF2_OK && phase_step &&
        (previous_phase_target < UINT32_C(4) ||
         next_phase_target < UINT32_C(4))) {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK && phase_step) {
        status = write_u16(
            machine,
            previous_phase_target - UINT32_C(4),
            UINT16_C(0x8020)
        );
    }
    if (status == VF2_OK && phase_step) {
        phase_index = next_phase_index;
        status = vf2_model2a_write(
            machine, UINT32_C(0x005000a4), &phase_index,
            sizeof(phase_index)
        );
    }
    if (status == VF2_OK && phase_step) {
        status = write_u16(
            machine,
            next_phase_target - UINT32_C(4),
            UINT16_C(0x801c)
        );
    }
    if (status == VF2_OK && phase_reset) {
        const uint8_t flagged_phase_index =
            (uint8_t)(phase_index | UINT8_C(0x80));
        const uint8_t zero = 0u;
        const uint8_t ff = UINT8_C(0xff);

        status = vf2_model2a_write(
            machine, UINT32_C(0x005000a4),
            &flagged_phase_index, sizeof(flagged_phase_index)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005000a5), &zero, sizeof(zero)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005000a7), &ff, sizeof(ff)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x00500864), &phase_object
            );
        }
        if (status == VF2_OK && phase_object > UINT32_MAX - UINT32_C(0x80)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (status == VF2_OK) {
            status = write_u16(
                machine, phase_object + UINT32_C(0x80), UINT16_C(0)
            );
        }
        if (status == VF2_OK) {
            status = clear_tile_plane_64x48(
                machine, UINT32_C(0x01000000)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine,
                UINT32_C(0x0005feac) +
                    (uint32_t)phase_index * UINT32_C(8),
                &phase_text_base
            );
        }
        if (status == VF2_OK && phase_text_base > UINT32_MAX - UINT32_C(4)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        phase_text_source = phase_text_base + UINT32_C(4);
        if (status == VF2_OK) {
            uint64_t length = 0u;
            for (length = 0u; length < UINT64_C(4096); ++length) {
                uint8_t raw = 0u;
                status = vf2_model2a_read(
                    machine,
                    phase_text_source + (uint32_t)length,
                    &raw, sizeof(raw)
                );
                if (status != VF2_OK || raw == 0u) {
                    break;
                }
            }
            if (status == VF2_OK && length == UINT64_C(4096)) {
                return VF2_ERROR_UNSUPPORTED;
            }
            phase_text_characters = length;
        }
        if (status == VF2_OK) {
            const uint32_t half_length =
                ((uint32_t)phase_text_characters + UINT32_C(1)) >> 1u;
            const uint32_t column_offset =
                (UINT32_C(32) - half_length) * UINT32_C(2);
            phase_text_destination =
                (UINT32_C(0x01000100) & ~UINT32_C(0x7f)) +
                column_offset;
            status = copy_diagnostic_text(
                machine, phase_text_source, phase_text_destination, NULL
            );
        }
        if (status == VF2_OK) {
            recovered_instructions +=
                UINT64_C(12573) + phase_text_characters * UINT64_C(12);
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, cpu->registers[1] + UINT32_C(64), saved_g4
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[VF2_I960_G0_REGISTER + 4u] = saved_g4;
    cpu->registers[VF2_I960_G0_REGISTER + 7u] = player0;
    cpu->registers[VF2_I960_G0_REGISTER + 8u] = player1;
    if (phase_step) {
        cpu->registers[VF2_I960_G0_REGISTER + 9u] =
            next_phase_target - UINT32_C(4);
    }
    if (phase_reset) {
        cpu->registers[VF2_I960_G0_REGISTER] = phase_text_source;
        cpu->registers[VF2_I960_G0_REGISTER + 1u] = 0u;
        cpu->registers[VF2_I960_G0_REGISTER + 9u] = phase_text_destination;
    }
    (void)base;
    set_equal_condition(cpu);
    account_nested_procedure(
        cpu,
        phase_reset ? UINT64_C(5) : UINT64_C(2),
        phase_reset ? UINT64_C(5) : UINT64_C(2)
    );
    if (cpu->maximum_local_frame_depth <
        cpu->local_frame_depth + (phase_reset ? 3u : 2u)) {
        cpu->maximum_local_frame_depth =
            cpu->local_frame_depth + (phase_reset ? 3u : 2u);
    }
    status = finish_recovered_procedure(
        machine, cpu, recovered_instructions
    );
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values =
        (phase_step ? UINT64_C(7) : UINT64_C(4)) +
        (phase_reset
            ? UINT64_C(3076) + phase_text_characters
            : UINT64_C(0));
    report->bytes_written =
        (phase_step ? 14u : 9u) +
        (phase_reset
            ? (size_t)UINT32_C(6149) +
                (size_t)phase_text_characters * 2u
            : 0u);
    report->recovered_instruction_count = recovered_instructions;
    report->recovered_procedure_calls =
        phase_reset ? UINT64_C(5) : UINT64_C(2);
    report->recovered_procedure_returns =
        phase_reset ? UINT64_C(6) : UINT64_C(3);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_selector3_display_text_at(
    vf2_model2a *machine, uint32_t source_base, uint32_t destination_base
)
{
    int16_t addend = 0;
    int16_t word_mode = 0;
    uint16_t raw_addend = 0u;
    uint16_t raw_mode = 0u;
    uint32_t rows = 0u;
    uint32_t columns = 0u;
    uint32_t source = source_base + UINT32_C(12);
    uint32_t row = 0u;
    vf2_status status = read_u16(machine, source_base, &raw_addend);

    addend = (int16_t)raw_addend;
    if (status == VF2_OK) {
        status = read_u16(machine, source_base + UINT32_C(2), &raw_mode);
        word_mode = (int16_t)raw_mode;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, source_base + UINT32_C(4), &rows);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, source_base + UINT32_C(8), &columns);
    }
    if (status != VF2_OK || rows > UINT32_C(4096) || columns > UINT32_C(4096)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    for (row = 0u; status == VF2_OK && row < rows; ++row) {
        uint32_t column = 0u;
        uint32_t destination = destination_base + row * UINT32_C(0x80);
        for (column = 0u; status == VF2_OK && column < columns; ++column) {
            int32_t sample = 0;
            if (word_mode == 0) {
                uint8_t raw = 0u;
                status = vf2_model2a_read(machine, source, &raw, sizeof(raw));
                sample = (int32_t)(int8_t)raw;
                source += UINT32_C(1);
            } else {
                uint16_t raw = 0u;
                status = read_u16(machine, source, &raw);
                sample = (int32_t)(int16_t)raw;
                source += UINT32_C(2);
            }
            if (status == VF2_OK) {
                status = write_u16(machine, destination,
                    (uint16_t)((int32_t)addend + sample));
                destination += UINT32_C(2);
            }
        }
    }
    return status;
}

static vf2_status execute_selector3_display_text(
    vf2_model2a *machine,
    uint32_t source_base
)
{
    return execute_selector3_display_text_at(
        machine, source_base, UINT32_C(0x01004000)
    );
}

static vf2_status execute_selector3_profile_class(
    const vf2_model2a *machine,
    uint32_t base,
    uint32_t *result
)
{
    uint32_t flags = 0u;
    uint8_t mode = 0u;
    uint8_t left = 0u;
    uint8_t right = 0u;
    uint8_t table_value = 0u;
    vf2_status status = VF2_OK;

    if (result == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read_u32(
        machine, base + UINT32_C(0x3320), &flags
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3324), &mode, sizeof(mode)
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if (mode == UINT8_C(25) && (flags & (UINT32_C(1) << 1u)) == 0u) {
        *result = UINT32_C(3);
        return VF2_OK;
    }
    if ((flags & UINT32_C(1)) != 0u) {
        *result = UINT32_C(2);
        return VF2_OK;
    }
    if ((flags & (UINT32_C(1) << 1u)) != 0u) {
        if (mode != UINT8_C(25)) {
            *result = UINT32_C(2);
            return VF2_OK;
        }
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3327), &left, sizeof(left)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, base + UINT32_C(0x3328), &right, sizeof(right)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        if (left == right) {
            *result = 0u;
            return VF2_OK;
        }
    }
    status = vf2_model2a_read(
        machine, UINT32_C(0x000028b8) + (uint32_t)mode,
        &table_value, sizeof(table_value)
    );
    if (status == VF2_OK) {
        *result = table_value == 0u ? 0u : 1u;
    }
    return status;
}

static vf2_status execute_selector3_profile_color(
    const vf2_model2a *machine,
    uint32_t base,
    uint32_t selector,
    uint32_t *result
)
{
    uint32_t flags = 0u;
    uint32_t profile_class = 0u;
    uint8_t mode = 0u;
    uint8_t byte_value = 0u;
    uint32_t table_index = 0u;
    vf2_status status = VF2_OK;

    if (result == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = execute_selector3_profile_class(
        machine, base, &profile_class
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, base + UINT32_C(0x3320), &flags
        );
    }
    if (status == VF2_OK && (flags & (UINT32_C(1) << 1u)) == 0u) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3324), &mode, sizeof(mode)
        );
        if (status == VF2_OK && profile_class == UINT32_C(3)) {
            *result = 0u;
            return VF2_OK;
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine,
                UINT32_C(0x000027ac) + (uint32_t)mode * UINT32_C(2) +
                    (profile_class == UINT32_C(2) ? 0u : selector),
                &byte_value, sizeof(byte_value)
            );
        }
        table_index = (uint32_t)byte_value;
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x000027e0) + table_index * UINT32_C(4),
                result
            );
        }
    } else if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3325), &byte_value, sizeof(byte_value)
        );
        *result = (uint32_t)byte_value;
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, base + UINT32_C(0x3326), &byte_value,
                sizeof(byte_value)
            );
        }
        if (status == VF2_OK) {
            *result = (*result << 8u) | (uint32_t)byte_value;
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, base + UINT32_C(0x3327) + selector,
                &byte_value, sizeof(byte_value)
            );
        }
        if (status == VF2_OK) {
            *result = (*result << 8u) | (uint32_t)byte_value;
        }
    }
    if (status == VF2_OK) {
        *result += UINT32_C(0x00010101);
    }
    return status;
}

static vf2_status execute_selector3_halfword_stream(
    vf2_model2a *machine,
    uint32_t start
)
{
    uint32_t cursor = start;
    size_t descriptor_count = 0u;
    vf2_status status = VF2_OK;

    while (status == VF2_OK && descriptor_count < 64u) {
        uint32_t source = 0u;
        uint32_t destination = 0u;
        uint32_t header = 0u;
        uint32_t halfwords = 0u;
        uint32_t offset = 0u;

        status = vf2_model2a_read_u32(machine, cursor, &source);
        cursor += UINT32_C(4);
        if (status != VF2_OK || source == 0u) {
            break;
        }
        status = vf2_model2a_read_u32(machine, cursor, &destination);
        cursor += UINT32_C(4);
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, source, &header);
        }
        if (status != VF2_OK || header > (UINT32_MAX >> 4u)) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        halfwords = header << 4u;
        if (halfwords > UINT32_C(65536)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        for (offset = 0u; status == VF2_OK && offset < halfwords;
             ++offset) {
            uint16_t value = 0u;

            status = read_u16(
                machine, source + UINT32_C(4) + offset * UINT32_C(2), &value
            );
            if (status == VF2_OK) {
                status = write_u16(
                    machine, destination + offset * UINT32_C(2), value
                );
            }
        }
        ++descriptor_count;
    }
    if (status != VF2_OK || descriptor_count == 64u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    return VF2_OK;
}

static vf2_status execute_selector3_register_stream(
    vf2_model2a *machine,
    uint32_t start
)
{
    uint32_t cursor = start;
    size_t descriptor_count = 0u;
    vf2_status status = VF2_OK;

    while (status == VF2_OK && descriptor_count < 64u) {
        uint32_t destination = 0u;
        uint32_t encoded_count = 0u;
        uint32_t words = 0u;
        uint32_t index = 0u;

        status = vf2_model2a_read_u32(machine, cursor, &destination);
        cursor += UINT32_C(4);
        if (status != VF2_OK || destination == 0u) {
            break;
        }
        status = vf2_model2a_read_u32(machine, cursor, &encoded_count);
        cursor += UINT32_C(4);
        if (status != VF2_OK || (int32_t)encoded_count < 0) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        words = encoded_count >> 1u;
        if (words > UINT32_C(65536)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        for (index = 0u; status == VF2_OK && index < words; ++index) {
            uint32_t value = 0u;

            status = vf2_model2a_read_u32(machine, cursor, &value);
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, destination + index * UINT32_C(4), value
                );
            }
            cursor += UINT32_C(4);
        }
        ++descriptor_count;
    }
    if (status != VF2_OK || descriptor_count == 64u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    return VF2_OK;
}

static vf2_status execute_selector3_mode0_prefix(vf2_model2a *machine)
{
    static const uint32_t palette_sources[] = {
        UINT32_C(0x000266f0), UINT32_C(0x000266f4),
        UINT32_C(0x00026700), UINT32_C(0x00026704)
    };
    static const uint32_t palette_destinations[] = {
        UINT32_C(0x018004c0), UINT32_C(0x018004e0),
        UINT32_C(0x018004d0), UINT32_C(0x018004f0)
    };
    uint32_t pointer = 0u;
    uint32_t value = 0u;
    uint32_t palette_value = 0u;
    uint32_t index = 0u;
    uint8_t zero = 0u;
    vf2_status status = VF2_OK;

    /* 0xae78: clear the mode scratch byte, seed the two palette pages,
     * clear the diagnostic plane and prepare the graphics control flags. */
    status = vf2_model2a_write(
        machine, UINT32_C(0x00503030), &zero, sizeof(zero)
    );
    for (index = 0u; status == VF2_OK && index < 4u; ++index) {
        status = vf2_model2a_read_u32(
            machine, palette_sources[index], &palette_value
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, palette_destinations[index], palette_value
            );
        }
    }
    if (status == VF2_OK) {
        uint32_t row = 0u;
        for (row = 0u; status == VF2_OK && row < UINT32_C(48); ++row) {
            uint32_t column = 0u;
            uint32_t destination = UINT32_C(0x01000000) + row * UINT32_C(0x80);
            for (column = 0u; status == VF2_OK && column < UINT32_C(62); ++column) {
                status = write_u16(machine, destination, UINT16_C(32));
                destination += UINT32_C(2);
            }
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050009c), &value
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x0050009c), value & ~UINT32_C(1)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500834), &pointer
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer, &value);
    }
    if (status == VF2_OK) {
        value |= UINT32_C(0x42800000);
        status = vf2_model2a_write_u32(machine, pointer, value);
    }
    return status;
}

static vf2_status execute_frame_selector3_b0d8(
    vf2_model2a *machine,
    uint8_t previous_phase,
    uint8_t *next_phase,
    int prefix_already_applied
)
{
    static const uint32_t clear_slots[] = {
        UINT32_C(0x00500828), UINT32_C(0x00500844),
        UINT32_C(0x00500848), UINT32_C(0x0050081c)
    };
    uint32_t pointer = 0u;
    uint32_t value = 0u;
    uint32_t task0 = 0u;
    uint32_t task1 = 0u;
    uint32_t index = 0u;
    uint8_t zero = 0u;
    uint8_t one = UINT8_C(1);
    uint8_t three = UINT8_C(3);
    uint8_t six = UINT8_C(6);
    uint8_t eight = UINT8_C(8);
    uint16_t zero16 = 0u;
    uint8_t input_zero = 0u;
    uint8_t input_default = UINT8_C(0x63);
    vf2_status status = VF2_OK;

    if (!prefix_already_applied) {
        status = execute_selector3_mode0_prefix(machine);
    }
    if (status != VF2_OK) {
        return status;
    }

    status = vf2_model2a_write(
        machine, UINT32_C(0x00500030), &((uint8_t){2u}), sizeof(uint8_t)
    );

    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500834), &pointer);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer, &value);
    }
    if (status == VF2_OK) {
        value |= UINT32_C(0x42800000);
        value &= ~UINT32_C(0x00100000);
        status = vf2_model2a_write_u32(machine, pointer, value);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x0100a004), zero16);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x0100a00c), zero16);
    }
    if (status == VF2_OK) {
        status = execute_selector3_display_text(
            machine, UINT32_C(0x02a6c15e)
        );
    }
    for (index = 0u; status == VF2_OK && index < 4u; ++index) {
        status = vf2_model2a_read_u32(machine, clear_slots[index], &pointer);
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, pointer, &value);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, pointer, value & ~UINT32_C(0x80000000)
            );
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500804), &task0);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500808), &task1);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task0 + UINT32_C(0x1b0), &eight,
                                   sizeof(eight));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task1 + UINT32_C(0x1b0), &six,
                                   sizeof(six));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task0 + UINT32_C(4), &zero,
                                   sizeof(zero));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task0 + UINT32_C(0x1b1), &eight,
                                   sizeof(eight));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task0 + UINT32_C(0x18),
                                       UINT32_C(0x447a0000));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task0 + UINT32_C(0x1c), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task0 + UINT32_C(0x20), 0u);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, task0 + UINT32_C(0x26), zero16);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task0 + UINT32_C(0xc),
                                       UINT32_C(0x00013f08));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task0 + UINT32_C(0x2a), &zero,
                                   sizeof(zero));
    }
    if (status == VF2_OK) {
        status = write_u16(machine, task0 + UINT32_C(0x624), zero16);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, task0, &value);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, task0, (value & UINT32_C(0xff000000)) |
                                UINT32_C(0x80000000)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500868), &pointer);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer, &value);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, pointer,
                                       value | UINT32_C(0x80000000));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, pointer + UINT32_C(0xc),
                                       UINT32_C(0x000640f4));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task0 + UINT32_C(0x69c),
                                   &input_zero, sizeof(input_zero));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task0 + UINT32_C(0x69d),
                                   &input_default, sizeof(input_default));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task1 + UINT32_C(0x69c),
                                   &input_zero, sizeof(input_zero));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task1 + UINT32_C(0x69d),
                                   &input_default, sizeof(input_default));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task1 + UINT32_C(4), &one,
                                   sizeof(one));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task1 + UINT32_C(0x1b1), &six,
                                   sizeof(six));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task1 + UINT32_C(0x18), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task1 + UINT32_C(0x1c),
                                       UINT32_C(0x447a0000));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task1 + UINT32_C(0x20), 0u);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, task1 + UINT32_C(0x26), zero16);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task1 + UINT32_C(0xc),
                                       UINT32_C(0x00013f08));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task1 + UINT32_C(0x2a), &zero,
                                   sizeof(zero));
    }
    if (status == VF2_OK) {
        status = write_u16(machine, task1 + UINT32_C(0x624), zero16);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, task1, &value);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, task1, (value & UINT32_C(0xff000000)) |
                                UINT32_C(0x80000000)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x0050086c), &pointer);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer, &value);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, pointer,
                                       value | UINT32_C(0x80000000));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, pointer + UINT32_C(0xc),
                                       UINT32_C(0x000640f4));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00508000), &value);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00508000),
                                       value | (UINT32_C(1) << 16u));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x00500064), &three,
                                   sizeof(three));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a00c),
                                       UINT32_C(0x40f00000));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500024),
                                       UINT32_C(256));
    }
    if (status == VF2_OK) {
        *next_phase = (uint8_t)(previous_phase + UINT8_C(1));
        status = vf2_model2a_write(machine, UINT32_C(0x00500030), next_phase,
                                   sizeof(*next_phase));
    }
    return status;
}

static vf2_status execute_selector3_profile_measure(
    const vf2_model2a *machine,
    uint32_t base,
    uint32_t *x_result,
    uint32_t *y_result
)
{
    uint32_t flags = 0u;
    uint32_t profile_class = 0u;
    uint32_t color0 = 0u;
    uint32_t color1 = 0u;
    uint32_t first = 0u;
    uint32_t second = 0u;
    uint32_t third = 0u;
    uint32_t fourth = 0u;
    uint8_t value = 0u;
    vf2_status status = VF2_OK;

    if (x_result == NULL || y_result == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = execute_selector3_profile_class(
        machine, base, &profile_class
    );
    if (status == VF2_OK) {
        status = execute_selector3_profile_color(
            machine, base, 0u, &color0
        );
    }
    if (status == VF2_OK) {
        status = execute_selector3_profile_color(
            machine, base, 1u, &color1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, base + UINT32_C(0x3320), &flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3380), &value, sizeof(value)
        );
        first = (uint32_t)value;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3388), &value, sizeof(value)
        );
        second = (uint32_t)value;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3385), &value, sizeof(value)
        );
        third = (uint32_t)value;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x338d), &value, sizeof(value)
        );
        fourth = (uint32_t)value;
    }
    if (status != VF2_OK) {
        return status;
    }
    color0 = (color0 >> 16u) & UINT32_C(0xff);
    color1 = (color1 >> 16u) & UINT32_C(0xff);
    if (profile_class == UINT32_C(3)) {
        *x_result = 0u;
        *y_result = 0u;
    } else if (color0 == 0u || color1 == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    } else if (profile_class == UINT32_C(2) &&
               (flags & (UINT32_C(1) << 1u)) != 0u) {
        *x_result = (first + second) / color0 + third;
        *y_result = fourth;
    } else {
        *x_result = first / color0 + third;
        *y_result = second / color1 + fourth;
    }
    return VF2_OK;
}

static vf2_status execute_selector3_phase1(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint8_t previous_phase,
    uint8_t *next_phase,
    int *profile_measure_called,
    int *countdown_terminal,
    uint32_t *measured_sum
)
{
    uint32_t base = 0u;
    uint32_t profile_flags = 0u;
    uint32_t runtime_flags = 0u;
    uint32_t input_flags = 0u;
    uint32_t x = 0u;
    uint32_t y = 0u;
    uint32_t sum = 0u;
    uint32_t counter = 0u;
    uint8_t mode = 0u;
    int countdown = 0;
    vf2_status status = VF2_OK;

    if (cpu == NULL || next_phase == NULL || profile_measure_called == NULL ||
        countdown_terminal == NULL || measured_sum == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *next_phase = previous_phase;
    *profile_measure_called = 0;
    *countdown_terminal = 0;
    *measured_sum = 0u;
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0050016c), &base
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3324), &mode, sizeof(mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, base + UINT32_C(0x3320), &profile_flags
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if (mode == UINT8_C(25) &&
        (profile_flags & (UINT32_C(1) << 1u)) == 0u) {
        countdown = 1;
    } else {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500708), &runtime_flags
        );
        if (status == VF2_OK && (runtime_flags & UINT32_C(3)) == 0u) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x00500704), &input_flags
            );
            if (status == VF2_OK &&
                (input_flags & UINT32_C(0x08000008)) == 0u) {
                countdown = 1;
            }
        }
    }
    if (status != VF2_OK) {
        return status;
    }
    if (!countdown) {
        const uint32_t caller_sp = cpu->registers[1];

        *profile_measure_called = 1;
        /* 0xafe0 spills g0/g1 around 0x2584; 0x26ec spills g9.
         * The two color lookups reuse the same child frame slot. */
        status = vf2_model2a_write_u32(
            machine, caller_sp + UINT32_C(0x80),
            cpu->registers[VF2_I960_G0_REGISTER]
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, caller_sp + UINT32_C(0x84),
                cpu->registers[VF2_I960_G0_REGISTER + 1u]
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, caller_sp + UINT32_C(0x140),
                cpu->registers[VF2_I960_G0_REGISTER + 9u]
            );
        }
        if (status == VF2_OK) {
            status = execute_selector3_profile_measure(
                machine, base, &x, &y
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        if ((profile_flags & UINT32_C(1)) != 0u ||
            (runtime_flags & (UINT32_C(1) << 1u)) == 0u) {
            sum = x + y;
        } else {
            sum = x;
        }
        *measured_sum = sum;
        if (sum < UINT32_C(24)) {
            *next_phase = UINT8_C(6);
            return vf2_model2a_write(
                machine, UINT32_C(0x00500030), next_phase,
                sizeof(*next_phase)
            );
        }
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00500024), &counter
    );
    if (status == VF2_OK) {
        --counter;
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500024), counter
        );
    }
    if (status == VF2_OK && (int32_t)counter <= 0) {
        *countdown_terminal = 1;
        *next_phase = (uint8_t)(previous_phase + UINT8_C(1));
        status = vf2_model2a_write(
            machine, UINT32_C(0x00500030), next_phase, sizeof(*next_phase)
        );
    }
    return status;
}

static vf2_status execute_selector3_sound_event(
    vf2_model2a *machine, uint32_t event
)
{
    uint32_t selector_mask = 0u;
    uint32_t base = 0u;
    uint32_t flags = 0u;
    uint8_t mode_flags = 0u;
    uint8_t count = 0u;
    uint8_t index = 0u;
    vf2_status status = VF2_OK;

    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0050002c), &selector_mask
    );
    if (status == VF2_OK && (selector_mask & UINT32_C(0x0c)) != 0u) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050016c), &base
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, base + UINT32_C(0x3351),
                &mode_flags, sizeof(mode_flags)
            );
        }
        if (status == VF2_OK && (mode_flags & UINT8_C(1)) != 0u) {
            return VF2_OK;
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500068), &flags
        );
    }
    if (status == VF2_OK && (flags & (UINT32_C(1) << 20u)) != 0u &&
        (event & UINT32_C(0x00ff0000)) == UINT32_C(0x009e0000)) {
        event -= UINT32_C(1) << 17u;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00e80004), UINT32_C(33)
        );
    }
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
    if (status == VF2_OK && count < UINT8_C(16)) {
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
                machine, UINT32_C(0x00504020) + (uint32_t)index * UINT32_C(4),
                event
            );
        }
        if (status == VF2_OK) {
            index = (uint8_t)((index + UINT8_C(1)) & UINT8_C(15));
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

static vf2_status execute_selector3_phase3(
    vf2_model2a *machine,
    uint8_t previous_phase,
    uint8_t *next_phase
)
{
    uint32_t flags = 0u;
    uint32_t counter = 0u;
    uint32_t terminal_gate = 0u;
    vf2_status status = VF2_OK;

    if (next_phase == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *next_phase = previous_phase;
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00500068), &flags
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500068), flags | (UINT32_C(1) << 16u)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500024), &counter
        );
    }
    if (status == VF2_OK) {
        --counter;
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500024), counter
        );
    }
    if (status == VF2_OK && counter == UINT32_C(192)) {
        status = execute_selector3_sound_event(machine, UINT32_C(0x00ad1001));
    }
    if (status == VF2_OK && (int32_t)counter > 0) {
        return VF2_OK;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00550000), &terminal_gate
        );
    }
    if (status == VF2_OK && terminal_gate != UINT32_C(1)) {
        *next_phase = (uint8_t)(previous_phase + UINT8_C(1));
        status = vf2_model2a_write(
            machine, UINT32_C(0x00500030), next_phase, sizeof(*next_phase)
        );
    }
    return status;
}

static vf2_status execute_selector3_phase4_mode_init(vf2_model2a *machine)
{
    uint8_t mode = 0u;
    uint8_t one = UINT8_C(1);
    uint16_t lane_count = 0u;
    uint16_t lane_aux = 0u;
    uint32_t table = 0u;
    uint32_t layout = 0u;
    uint32_t callback = 0u;
    uint32_t slot = 0u;
    uint32_t lane = 0u;
    uint32_t word = 0u;
    uint32_t pattern[4] = {0u, 0u, 0u, 0u};
    vf2_status status = vf2_model2a_read(
        machine, UINT32_C(0x00500064), &mode, sizeof(mode)
    );

    if (status == VF2_OK) {
        status = vf2_model2a_write(
  machine, UINT32_C(0x00503058), &mode, sizeof(mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
  machine, UINT32_C(0x00025034) + (uint32_t)mode * UINT32_C(32), &table
        );
    }
    if (status == VF2_OK) {
        status = execute_selector3_halfword_stream(machine, table);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
  machine, UINT32_C(0x00025038) + (uint32_t)mode * UINT32_C(32), &table
        );
    }
    if (status == VF2_OK) {
        status = execute_selector3_register_stream(machine, table);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
  machine, UINT32_C(0x0002503c) + (uint32_t)mode * UINT32_C(32), &layout
        );
    }
    if (status == VF2_OK) status = read_u16(machine, layout, &lane_count);
    if (status == VF2_OK) status = read_u16(machine, layout + UINT32_C(2), &lane_aux);
    if (status == VF2_OK) status = write_u16(machine, UINT32_C(0x0050303c), lane_count);
    if (status == VF2_OK) status = write_u16(machine, UINT32_C(0x0050303e), lane_aux);
    if (status != VF2_OK || lane_count == 0u || lane_count > UINT16_C(64)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    layout += UINT32_C(4);
    for (slot = 0u; status == VF2_OK && slot < UINT32_C(18); ++slot) {
        uint32_t source = 0u;
        status = vf2_model2a_read_u32(machine, layout + slot * UINT32_C(4), &source);
        for (lane = 0u; status == VF2_OK && lane < (uint32_t)lane_count; ++lane) {
  uint32_t q = 0u;
  for (q = 0u; status == VF2_OK && q < UINT32_C(16); ++q) {
      status = vf2_model2a_read_u32(
          machine, source + lane * UINT32_C(64) + q * UINT32_C(4), &word
      );
      if (status == VF2_OK) {
          status = vf2_model2a_write_u32(
              machine,
              UINT32_C(0x00579000) + slot * UINT32_C(64) +
                  lane * UINT32_C(1152) + q * UINT32_C(4),
              word
          );
      }
  }
        }
    }
    for (word = 0u; status == VF2_OK && word < UINT32_C(8); ++word) {
        status = vf2_model2a_write_u32(
  machine, UINT32_C(0x00503100) + word * UINT32_C(4), UINT32_MAX
        );
    }
    for (word = 0u; status == VF2_OK && word < UINT32_C(4); ++word) {
        status = vf2_model2a_read_u32(
  machine,
  UINT32_C(0x00579000) + ((uint32_t)lane_count - UINT32_C(1)) * UINT32_C(1152) +
      word * UINT32_C(4),
  &pattern[word]
        );
    }
    for (lane = (uint32_t)lane_count; status == VF2_OK && lane < UINT32_C(64); ++lane) {
        uint32_t repeat = 0u;
        for (repeat = 0u; status == VF2_OK && repeat < UINT32_C(8); ++repeat) {
  for (word = 0u; status == VF2_OK && word < UINT32_C(4); ++word) {
      status = vf2_model2a_write_u32(
          machine, UINT32_C(0x01004000) + lane * UINT32_C(128) +
              repeat * UINT32_C(16) + word * UINT32_C(4), pattern[word]
      );
  }
        }
    }
    for (word = 0u; status == VF2_OK && word < UINT32_C(4); ++word) {
        status = vf2_model2a_read_u32(
  machine, UINT32_C(0x00579000) + word * UINT32_C(4), &pattern[word]
        );
    }
    for (lane = 0u; status == VF2_OK && lane < UINT32_C(64); ++lane) {
        uint32_t repeat = 0u;
        for (repeat = 0u; status == VF2_OK && repeat < UINT32_C(8); ++repeat) {
  for (word = 0u; status == VF2_OK && word < UINT32_C(4); ++word) {
      status = vf2_model2a_write_u32(
          machine, UINT32_C(0x01006000) + lane * UINT32_C(128) +
              repeat * UINT32_C(16) + word * UINT32_C(4), pattern[word]
      );
  }
        }
    }
    if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x00503001), &one, sizeof(one));
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00503028), 0u);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x0050302c), 0u);
    if (status == VF2_OK) status = write_u16(machine, UINT32_C(0x0050304a), 0u);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
  machine, UINT32_C(0x0002502c) + (uint32_t)mode * UINT32_C(32), &callback
        );
    }
    if (status == VF2_OK && callback != UINT32_C(0x000244d0)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    return status;
}

static vf2_status execute_selector3_phase4_actor_record(
    vf2_model2a *machine, uint32_t descriptor, uint32_t fighter, int second,
    uint32_t *next_descriptor
)
{
    uint32_t flags = 0u;
    uint32_t associated = 0u;
    uint32_t base = 0u;
    uint32_t x = 0u;
    uint32_t z = 0u;
    uint8_t motion = 0u;
    uint8_t bit23 = 0u;
    uint8_t bit22 = 0u;
    uint8_t mode_flags = 0u;
    uint8_t zero = 0u;
    uint16_t angle = 0u;
    vf2_status status = VF2_OK;

    status = vf2_model2a_read_u32(machine, fighter, &flags);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, fighter, flags & ~(UINT32_C(1) << 31u));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
  machine, second ? UINT32_C(0x0050086c) : UINT32_C(0x00500868), &associated
        );
    }
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, associated, &flags);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, associated, flags & ~(UINT32_C(1) << 31u));
    if (status == VF2_OK) status = vf2_model2a_read(machine, descriptor + UINT32_C(12), &motion, sizeof(motion));
    if (status != VF2_OK) return status;
    if (motion == UINT8_C(31)) {
        if (next_descriptor != NULL) *next_descriptor = descriptor + UINT32_C(16);
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500828), &associated);
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, associated, &flags);
        if (status == VF2_OK) status = vf2_model2a_write_u32(machine, associated, flags & ~(UINT32_C(1) << 31u));
        return status;
    }
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, descriptor, &x);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, descriptor + UINT32_C(4), &z);
    if (status == VF2_OK) status = read_u16(machine, descriptor + UINT32_C(8), &angle);
    if (status == VF2_OK) status = vf2_model2a_read(machine, descriptor + UINT32_C(13), &bit23, sizeof(bit23));
    if (status == VF2_OK) status = vf2_model2a_read(machine, descriptor + UINT32_C(14), &bit22, sizeof(bit22));
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x0050016c), &base);
    if (status == VF2_OK) status = vf2_model2a_read(machine, base + UINT32_C(0x3351), &mode_flags, sizeof(mode_flags));
    if (status == VF2_OK) status = vf2_model2a_write(machine, fighter + UINT32_C(0x1b1), &motion, sizeof(motion));
    if (status == VF2_OK) {
        uint8_t actor_motion = (uint8_t)(motion + ((mode_flags & UINT8_C(0x40)) != 0u ? UINT8_C(13) : UINT8_C(0)));
        status = vf2_model2a_write(machine, fighter + UINT32_C(0x1b0), &actor_motion, sizeof(actor_motion));
    }
    if (status == VF2_OK) status = write_u16(machine, fighter + UINT32_C(0x26), angle);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, fighter + UINT32_C(0x18), x);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, fighter + UINT32_C(0x1c), 0u);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, fighter + UINT32_C(0x20), z);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, fighter + UINT32_C(0x0c), UINT32_C(0x00013f08));
    if (status == VF2_OK) status = vf2_model2a_write(machine, fighter + UINT32_C(0x2a), &zero, sizeof(zero));
    if (status == VF2_OK) status = write_u16(machine, fighter + UINT32_C(0x624), 0u);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, fighter, &flags);
    if (status == VF2_OK) {
        flags = (flags & UINT32_C(0xff000000)) | (UINT32_C(1) << 31u);
        if (bit23 != 0u) flags |= UINT32_C(1) << 23u; else flags &= ~(UINT32_C(1) << 23u);
        if (bit22 != 0u) flags |= UINT32_C(1) << 22u; else flags &= ~(UINT32_C(1) << 22u);
        status = vf2_model2a_write_u32(machine, fighter, flags);
    }
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, associated, &flags);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, associated, flags | (UINT32_C(1) << 31u));
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, associated + UINT32_C(0x0c), UINT32_C(0x000640f4));
    if (status == VF2_OK) {
        uint8_t state = 0u;
        status = vf2_model2a_read(machine, UINT32_C(0x0050009c), &state, sizeof(state));
        if (status == VF2_OK) {
  state = second ? (uint8_t)(state & ~UINT8_C(4)) : (uint8_t)(state & ~UINT8_C(2));
  status = vf2_model2a_write(machine, UINT32_C(0x0050009c), &state, sizeof(state));
  if (status == VF2_OK && (state & UINT8_C(6)) == 0u) {
      status = vf2_model2a_read_u32(machine, UINT32_C(0x00500828), &associated);
      if (status == VF2_OK) status = vf2_model2a_read_u32(machine, associated, &flags);
      if (status == VF2_OK) status = vf2_model2a_write_u32(machine, associated, flags | (UINT32_C(1) << 31u));
      if (status == VF2_OK) status = vf2_model2a_write_u32(machine, associated + UINT32_C(0x0c), UINT32_C(0x000221cc));
  }
        }
    }
    if (next_descriptor != NULL) *next_descriptor = descriptor + UINT32_C(16);
    return status;
}

static vf2_status execute_selector3_phase4_actor_init(vf2_model2a *machine)
{
    uint32_t player0 = 0u;
    uint32_t player1 = 0u;
    uint32_t next0 = UINT32_C(0x0201f454);
    uint32_t next1 = UINT32_C(0x0201f258);
    vf2_status status = vf2_model2a_write_u32(machine, UINT32_C(0x005001c8), next0);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x005001cc), next1);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x005001d0), UINT32_C(0x0201f5c4));
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x005001d4), UINT32_C(0x0201f38c));
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x005001e0), UINT32_C(0x000731ac));
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500804), &player0);
    if (status == VF2_OK) status = execute_selector3_phase4_actor_record(machine, next0, player0, 0, &next0);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x005001c8), next0);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500808), &player1);
    if (status == VF2_OK) status = execute_selector3_phase4_actor_record(machine, next1, player1, 1, &next1);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x005001cc), next1);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00550004), UINT32_C(0x000032c8));
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00550008), UINT32_C(0x00004e20));
    return status;
}

static vf2_status execute_selector3_phase4_timeline(vf2_model2a *machine)
{
    uint32_t total = 0u;
    uint32_t timer = 0u;
    uint32_t elapsed = 0u;
    uint32_t pointer = 0u;
    uint32_t index = 0u;
    uint16_t trigger = 0u;
    vf2_status status = vf2_model2a_read_u32(machine, UINT32_C(0x0201f388), &total);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500024), &timer);
    elapsed = total - timer;
    /* The first recovered phase-4 vector has no hits in the four actor lists. */
    for (index = 0u; status == VF2_OK && index < UINT32_C(4); ++index) {
        static const uint32_t slots[4] = {
  UINT32_C(0x005001c8), UINT32_C(0x005001d0),
  UINT32_C(0x005001cc), UINT32_C(0x005001d4)
        };
        status = vf2_model2a_read_u32(machine, slots[index], &pointer);
        if (status == VF2_OK) status = read_u16(machine, pointer + (index == 1u || index == 3u ? UINT32_C(4) : UINT32_C(10)), &trigger);
        if (status == VF2_OK && (uint32_t)trigger == elapsed) return VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x005001e0), &pointer);
    index = 0u;
    while (status == VF2_OK && index < UINT32_C(32)) {
        status = read_u16(machine, pointer, &trigger);
        if (status != VF2_OK || (uint32_t)trigger != elapsed) break;
        pointer += UINT32_C(4);
        ++index;
    }
    if (status == VF2_OK && index != UINT32_C(4)) return VF2_ERROR_UNSUPPORTED;
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x005001e0), pointer);
    return status;
}

static vf2_status execute_selector3_phase4(
    vf2_model2a *machine,
    uint8_t previous_phase,
    uint8_t *next_phase
)
{
    uint32_t flags = 0u;
    uint32_t pointer = 0u;
    uint32_t profile_flags = 0u;
    uint32_t counter = 0u;
    uint32_t phase4_table = 0u;
    uint8_t mode = 0u;
    int recovered_initial_path = 0;
    uint8_t fifteen = UINT8_C(15);
    vf2_status status = VF2_OK;

    if (next_phase == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *next_phase = previous_phase;
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00500068), &flags
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500068),
            flags & ~(UINT32_C(1) << 16u)
        );
    }
    if (status == VF2_OK) status = vf2_model2a_read(machine, UINT32_C(0x00500064), &mode, sizeof(mode));
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00025034) + (uint32_t)mode * UINT32_C(32), &phase4_table);
    if (status == VF2_OK && phase4_table != 0u) { recovered_initial_path = 1; status = execute_selector3_phase4_mode_init(machine); }
    if (status == VF2_OK) {
        status = execute_selector3_halfword_stream(
            machine, UINT32_C(0x02800394)
        );
    }
    if (status == VF2_OK) {
        status = execute_selector3_register_stream(
            machine, UINT32_C(0x02805890)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500068), &flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500068),
            flags & ~(UINT32_C(1) << 14u)
        );
    }
    if (status == VF2_OK) {
        status = clear_tile_plane_64x48(
            machine, UINT32_C(0x01000000)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500834), &pointer
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer, &flags);
    }
    if (status == VF2_OK) {
        flags |= (UINT32_C(1) << 30u) |
                 (UINT32_C(1) << 23u) |
                 (UINT32_C(1) << 25u);
        flags &= ~(UINT32_C(1) << 20u);
        status = vf2_model2a_write_u32(machine, pointer, flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0201f388), &counter
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500024), counter
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500814), &pointer
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, pointer + UINT32_C(0x40), &fifteen,
            sizeof(fifteen)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, pointer + UINT32_C(0x210),
            UINT32_C(0x0201f624)
        );
    }
    if (status == VF2_OK && recovered_initial_path) status = execute_selector3_phase4_actor_init(machine);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050016c), &pointer
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, pointer + UINT32_C(0x3320), &profile_flags
        );
    }
    if (status == VF2_OK && (profile_flags & UINT32_C(1)) == 0u) {
        status = execute_selector3_display_text_at(
            machine, UINT32_C(0x02a6d8aa), UINT32_C(0x010016da)
        );
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x01000ef4), UINT16_C(32));
        }
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x01000ef6), UINT16_C(32));
        }
    }
    if (status == VF2_OK) {
        *next_phase = (uint8_t)(previous_phase + UINT8_C(1));
        status = vf2_model2a_write(
            machine, UINT32_C(0x00500030), next_phase, sizeof(*next_phase)
        );
    }
    if (status == VF2_OK && recovered_initial_path) status = execute_selector3_phase4_timeline(machine);
    if (status == VF2_OK && recovered_initial_path) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500024), &counter);
    if (status == VF2_OK && recovered_initial_path) { --counter; status = vf2_model2a_write_u32(machine, UINT32_C(0x00500024), counter); }
    if (status == VF2_OK && recovered_initial_path && counter == 0u) {
        uint32_t fighter = 0u;
        uint32_t fighter_flags = 0u;
        uint32_t runtime_flags = 0u;
        uint32_t slot = 0u;
        uint8_t zero = 0u;

        status = vf2_model2a_write(machine, UINT32_C(0x0050009c), &zero, sizeof(zero));
        for (slot = 0u; status == VF2_OK && slot < UINT32_C(2); ++slot) {
            status = vf2_model2a_read_u32(
                machine, slot == 0u ? UINT32_C(0x00500804) : UINT32_C(0x00500808),
                &fighter
            );
            if (status == VF2_OK) status = vf2_model2a_read_u32(machine, fighter, &fighter_flags);
            if (status == VF2_OK) {
                fighter_flags &= ~((UINT32_C(1) << 23u) | (UINT32_C(1) << 22u));
                fighter_flags |= UINT32_C(1) << 26u;
                status = vf2_model2a_write_u32(machine, fighter, fighter_flags);
            }
        }
        if (status == VF2_OK) status = execute_selector3_sound_event(machine, UINT32_C(0x00bd1a60));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00508000), &runtime_flags);
        if (status == VF2_OK) status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00508000), runtime_flags & ~(UINT32_C(1) << 16u)
        );
        if (status == VF2_OK) {
            *next_phase = (uint8_t)(*next_phase + UINT8_C(1));
            status = vf2_model2a_write(machine, UINT32_C(0x00500030), next_phase, sizeof(*next_phase));
        }
    }
    return status;
}

static vf2_status execute_selector3_phase5_timeline_observed(
    vf2_model2a *machine, int *recovered
)
{
    static const uint32_t slots[4] = {
        UINT32_C(0x005001c8), UINT32_C(0x005001d0),
        UINT32_C(0x005001cc), UINT32_C(0x005001d4)
    };
    static const uint32_t trigger_offsets[4] = {
        UINT32_C(10), UINT32_C(4), UINT32_C(10), UINT32_C(4)
    };
    uint32_t total = 0u, timer = 0u, elapsed = 0u;
    uint32_t pointers[4] = {0u,0u,0u,0u};
    uint32_t event_pointer = 0u, fighter0 = 0u, fighter1 = 0u;
    uint32_t value = 0u, flags = 0u;
    uint16_t trigger = 0u;
    uint8_t control = 0u;
    uint32_t i = 0u;
    vf2_status status = VF2_OK;

    if (recovered == NULL) return VF2_ERROR_INVALID_ARGUMENT;
    *recovered = 0;
    status = vf2_model2a_read_u32(machine, UINT32_C(0x0201f388), &total);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500024), &timer);
    elapsed = total - timer;
    for (i = 0u; status == VF2_OK && i < UINT32_C(4); ++i) {
        status = vf2_model2a_read_u32(machine, slots[i], &pointers[i]);
        if (status == VF2_OK) status = read_u16(machine, pointers[i] + trigger_offsets[i], &trigger);
        if (status == VF2_OK && i == UINT32_C(1) && (uint32_t)trigger != elapsed) return VF2_OK;
        if (status == VF2_OK && i != UINT32_C(1) && (uint32_t)trigger == elapsed) return VF2_OK;
    }
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x005001e0), &event_pointer);
    if (status == VF2_OK) status = read_u16(machine, event_pointer, &trigger);
    if (status == VF2_OK && (uint32_t)trigger == elapsed) return VF2_OK;
    if (status != VF2_OK) return status;

    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500804), &fighter0);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, pointers[1], &value);
    if (status == VF2_OK && value != 0u) status = vf2_model2a_write_u32(machine, fighter0 + UINT32_C(0x194), value);
    if (status == VF2_OK && value != 0u) status = vf2_model2a_read(machine, pointers[1] + UINT32_C(6), &control, sizeof(control));
    if (status == VF2_OK && value != 0u) status = vf2_model2a_read_u32(machine, fighter0, &flags);
    if (status == VF2_OK && value != 0u) {
        flags |= UINT32_C(1) << 26u;
        if (control != 0u) flags &= ~(UINT32_C(1) << 26u);
        status = vf2_model2a_write_u32(machine, fighter0, flags);
    }
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x005001d0), pointers[1] + UINT32_C(8));
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500808), &fighter1);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, fighter0 + UINT32_C(0x18), &value);
    if (status == VF2_OK && value == UINT32_C(0x447a0000)) {
        status = vf2_model2a_read_u32(machine, fighter1, &flags);
        if (status == VF2_OK) status = vf2_model2a_write_u32(machine, fighter1, flags | (UINT32_C(1) << 23u));
    }
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, fighter1 + UINT32_C(0x18), &value);
    if (status == VF2_OK && value == UINT32_C(0x447a0000)) {
        status = vf2_model2a_read_u32(machine, fighter0, &flags);
        if (status == VF2_OK) status = vf2_model2a_write_u32(machine, fighter0, flags | (UINT32_C(1) << 23u));
    }
    if (status == VF2_OK) *recovered = 1;
    return status;
}
static vf2_status execute_selector3_phase5(
    vf2_model2a *machine,
    uint8_t previous_phase,
    uint8_t *next_phase
)
{
    uint32_t counter = 0u;
    uint32_t pointer = 0u;
    uint32_t flags = 0u;
    uint8_t zero = 0u;
    int timeline_recovered = 0;
    vf2_status status = VF2_OK;

    if (next_phase == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *next_phase = previous_phase;
    status = execute_selector3_phase5_timeline_observed(machine, &timeline_recovered);
    if (status == VF2_OK) status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00500024), &counter
    );
    if (status == VF2_OK) {
        --counter;
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500024), counter
        );
    }
    if (status == VF2_OK && counter != 0u) {
        return VF2_OK;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050009c), &zero, sizeof(zero)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500804), &pointer
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer, &flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, pointer,
            (flags & ~((UINT32_C(1) << 23u) |
                       (UINT32_C(1) << 22u))) |
                (UINT32_C(1) << 26u)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500808), &pointer
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer, &flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, pointer,
            (flags & ~((UINT32_C(1) << 23u) |
                       (UINT32_C(1) << 22u))) |
                (UINT32_C(1) << 26u)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00508000), &flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00508000),
            flags & ~(UINT32_C(1) << 16u)
        );
    }
    if (status == VF2_OK) {
        *next_phase = (uint8_t)(previous_phase + UINT8_C(1));
        status = vf2_model2a_write(
            machine, UINT32_C(0x00500030), next_phase, sizeof(*next_phase)
        );
    }
    return status;
}

static vf2_status execute_selector3_phase6(
    vf2_model2a *machine,
    uint8_t previous_phase,
    uint8_t *next_phase
)
{
    uint32_t pointer = 0u;
    uint32_t flags = 0u;
    uint32_t profile_flags = 0u;
    uint32_t destination = UINT32_C(0x010055e0);
    uint16_t zero16 = 0u;
    vf2_status status = VF2_OK;

    if (next_phase == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *next_phase = previous_phase;
    status = clear_tile_plane_64x48(
        machine, UINT32_C(0x01000000)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500834), &pointer
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer, &flags);
    }
    if (status == VF2_OK) {
        flags |= (UINT32_C(1) << 30u) |
                 (UINT32_C(1) << 23u) |
                 (UINT32_C(1) << 25u);
        flags &= ~(UINT32_C(1) << 20u);
        status = vf2_model2a_write_u32(machine, pointer, flags);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x0100a004), zero16);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x0100a00c), zero16);
    }
    if (status == VF2_OK) {
        status = execute_selector3_display_text_at(
            machine, UINT32_C(0x02a68586), UINT32_C(0x01004000)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050016c), &pointer
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, pointer + UINT32_C(0x3320), &profile_flags
        );
    }
    if (status == VF2_OK && (profile_flags & UINT32_C(1)) != 0u) {
        destination = UINT32_C(0x01005460);
    }
    if (status == VF2_OK) {
        status = execute_selector3_display_text_at(
            machine, UINT32_C(0x02a6c0da), destination
        );
    }
    if (status == VF2_OK) {
        *next_phase = (uint8_t)(previous_phase + UINT8_C(1));
        status = vf2_model2a_write(
            machine, UINT32_C(0x00500030), next_phase, sizeof(*next_phase)
        );
    }
    return status;
}

static vf2_status execute_selector3_phase7_pre_profile(
    vf2_model2a *machine, uint32_t task0, uint32_t task1
)
{
    uint32_t pointer = 0u;
    uint32_t flags = 0u;
    uint8_t zero = 0u;
    uint8_t sixty_three = UINT8_C(0x63);
    uint8_t copied = 0u;
    vf2_status status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0050086c), &pointer
    );

    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, pointer, &flags);
    if (status == VF2_OK) status = vf2_model2a_write_u32(
        machine, pointer, flags | (UINT32_C(1) << 31u));
    if (status == VF2_OK) status = vf2_model2a_write_u32(
        machine, pointer + UINT32_C(0x0c), UINT32_C(0x000640f4));

    if (status == VF2_OK) status = vf2_model2a_write(
        machine, task0 + UINT32_C(0x69c), &zero, sizeof(zero));
    if (status == VF2_OK) status = vf2_model2a_write(
        machine, task0 + UINT32_C(0x69d), &sixty_three, sizeof(sixty_three));
    if (status == VF2_OK) status = vf2_model2a_write(
        machine, task1 + UINT32_C(0x69c), &zero, sizeof(zero));
    if (status == VF2_OK) status = vf2_model2a_write(
        machine, task1 + UINT32_C(0x69d), &sixty_three, sizeof(sixty_three));

    if (status == VF2_OK) status = vf2_model2a_write(
        machine, UINT32_C(0x00500057), &zero, sizeof(zero));
    if (status == VF2_OK) status = vf2_model2a_write(
        machine, UINT32_C(0x00500058), &zero, sizeof(zero));
    if (status == VF2_OK) status = vf2_model2a_read(
        machine, UINT32_C(0x00500059), &copied, sizeof(copied));
    if (status == VF2_OK) status = vf2_model2a_write(
        machine, UINT32_C(0x00500052), &copied, sizeof(copied));
    return status;
}

static vf2_status execute_selector3_phase7_profile(
    vf2_model2a *machine, vf2_i960_cpu *cpu, uint32_t task1
)
{
    vf2_i960_cpu child;
    vf2_hybrid_bridge_report child_report;
    vf2_status status = VF2_OK;
    uint32_t index = 0u;

    child = *cpu;
    child.registers[VF2_I960_G0_REGISTER + 7u] = task1;
    memset(&child_report, 0, sizeof(child_report));
    status = vf2_i960_cpu_enter_procedure(
        &child, VF2_DISPLAY_PROFILE_APPLY_ENTRY, UINT32_C(0x00abcdef));
    if (status != VF2_OK) return status;
    child.executed_instructions += UINT64_C(1);
    status = execute_display_profile_apply(machine, &child, &child_report);
    if (status != VF2_OK || child.ip != UINT32_C(0x00abcdef))
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;

    for (index = 0u; index < UINT32_C(10); ++index) {
        cpu->registers[VF2_I960_G0_REGISTER + index] =
            child.registers[VF2_I960_G0_REGISTER + index];
    }
    return VF2_OK;
}

static vf2_status execute_selector3_phase7_post_profile(vf2_model2a *machine)
{
    static const uint32_t clear31_ptrs[3] = {
        UINT32_C(0x00500854), UINT32_C(0x0050085c), UINT32_C(0x00500860)
    };
    static const uint32_t clear3_ptrs[2] = {
        UINT32_C(0x0050083c), UINT32_C(0x00500840)
    };
    uint32_t pointer = 0u;
    uint32_t flags = 0u;
    uint32_t index = 0u;
    uint8_t thirteen = UINT8_C(13);
    vf2_status status = vf2_model2a_write_u32(
        machine, UINT32_C(0x0050a160), UINT32_C(0x3727c5ac)
    );

    for (index = 0u; status == VF2_OK && index < UINT32_C(3); ++index) {
        status = vf2_model2a_read_u32(machine, clear31_ptrs[index], &pointer);
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, pointer, &flags);
        if (status == VF2_OK) status = vf2_model2a_write_u32(
            machine, pointer, flags & ~(UINT32_C(1) << 31u));
    }
    if (status == VF2_OK) status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00500814), &pointer);
    if (status == VF2_OK) status = vf2_model2a_write(
        machine, pointer + UINT32_C(0x40), &thirteen, sizeof(thirteen));

    for (index = 0u; status == VF2_OK && index < UINT32_C(2); ++index) {
        status = vf2_model2a_read_u32(machine, clear3_ptrs[index], &pointer);
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, pointer, &flags);
        if (status == VF2_OK) status = vf2_model2a_write_u32(
            machine, pointer, flags & ~(UINT32_C(1) << 3u));
    }

    if (status == VF2_OK) status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00500828), &pointer);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, pointer, &flags);
    if (status == VF2_OK) status = vf2_model2a_write_u32(
        machine, pointer, flags | (UINT32_C(1) << 31u));
    if (status == VF2_OK) status = vf2_model2a_write_u32(
        machine, pointer + UINT32_C(0x0c), UINT32_C(0x000221cc));

    if (status == VF2_OK) status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0050081c), &pointer);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, pointer, &flags);
    if (status == VF2_OK) status = vf2_model2a_write_u32(
        machine, pointer, flags | (UINT32_C(1) << 31u));
    if (status == VF2_OK) status = vf2_model2a_write_u32(
        machine, pointer + UINT32_C(0x0c), UINT32_C(0x0001b9ac));
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, pointer, &flags);
    if (status == VF2_OK) status = vf2_model2a_write_u32(
        machine, pointer, flags | UINT32_C(3));
    return status;
}

static vf2_status execute_selector3_phase7(
    vf2_model2a *machine,
    uint8_t previous_phase,
    uint8_t *next_phase
)
{
    uint32_t task0 = 0u;
    uint32_t task1 = 0u;
    uint32_t source = 0u;
    uint32_t pointer = 0u;
    uint32_t value = 0u;
    uint8_t first_mode = 0u;
    uint8_t second_mode = 0u;
    uint8_t task0_mode = 0u;
    uint8_t task1_mode = 0u;
    uint8_t mode = 0u;
    vf2_status status = VF2_OK;

    if (next_phase == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *next_phase = previous_phase;
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00500804), &task0
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500808), &task1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500168), &source
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, source + UINT32_C(0xc), &first_mode,
            sizeof(first_mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, source + UINT32_C(0x10), &second_mode,
            sizeof(second_mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, source + UINT32_C(0xfff), &task0_mode,
            sizeof(task0_mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, source + UINT32_C(0xffe), &task1_mode,
            sizeof(task1_mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, source + UINT32_C(4), &value
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x0050a00c), value
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, task0 + UINT32_C(4), &mode, sizeof(mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, task0 + UINT32_C(0x1b0), &first_mode,
            sizeof(first_mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, task0 + UINT32_C(0x69c), &task0_mode,
            sizeof(task0_mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, task1 + UINT32_C(0x1b0), &second_mode,
            sizeof(second_mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, task1 + UINT32_C(0x69c), &task1_mode,
            sizeof(task1_mode)
        );
    }
    if (status == VF2_OK && first_mode > UINT8_C(13)) {
        task0_mode = (uint8_t)(first_mode - UINT8_C(13));
        status = vf2_model2a_write(
            machine, task0 + UINT32_C(0x1b1), &task0_mode,
            sizeof(task0_mode)
        );
    } else if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, task0 + UINT32_C(0x1b1), &first_mode,
            sizeof(first_mode)
        );
    }
    if (status == VF2_OK && second_mode > UINT8_C(13)) {
        task1_mode = (uint8_t)(second_mode - UINT8_C(13));
        status = vf2_model2a_write(
            machine, task1 + UINT32_C(0x1b1), &task1_mode,
            sizeof(task1_mode)
        );
    } else if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, task1 + UINT32_C(0x1b1), &second_mode,
            sizeof(second_mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task0 + UINT32_C(0x18),
                                       UINT32_C(0xbf800000));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task1 + UINT32_C(0x18),
                                       UINT32_C(0x3f800000));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task0 + UINT32_C(0x1c), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task1 + UINT32_C(0x1c), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task0 + UINT32_C(0x20), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task1 + UINT32_C(0x20), 0u);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, task0 + UINT32_C(0x26), UINT16_C(3u << 14));
    }
    if (status == VF2_OK) {
        status = write_u16(machine, task1 + UINT32_C(0x26), UINT16_C(1u << 14));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task0 + UINT32_C(0xc),
                                       UINT32_C(0x13f08));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task1 + UINT32_C(0xc),
                                       UINT32_C(0x13f08));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task0 + UINT32_C(0x2a), &mode,
                                   sizeof(mode));
    }
    if (status == VF2_OK) {
        status = write_u16(machine, task0 + UINT32_C(0x624), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task1 + UINT32_C(0x2a), &mode,
                                   sizeof(mode));
    }
    if (status == VF2_OK) {
        status = write_u16(machine, task1 + UINT32_C(0x624), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, task0, &value);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task0,
                                       (value & UINT32_C(0xff000000)) |
                                           (UINT32_C(1) << 31u));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, task1, &value);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task1,
                                       (value & UINT32_C(0xff000000)) |
                                           (UINT32_C(1) << 31u));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500868), &pointer);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer, &value);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, pointer,
                                       value | (UINT32_C(1) << 31u));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, pointer + UINT32_C(0xc),
                                       UINT32_C(0x640f4));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task1 + UINT32_C(4), &((uint8_t){1}),
                                   sizeof(uint8_t));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task1 + UINT32_C(0x18),
                                       UINT32_C(0x3f800000));
    }
    if (status == VF2_OK) {
        status = execute_selector3_phase7_pre_profile(machine, task0, task1);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0050005c),
                                       UINT32_C(0x11d84));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500060),
                                       UINT32_C(0x11d8c));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x0050004c), &((uint8_t){2}),
                                   sizeof(uint8_t));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x0050005b), &mode,
                                  sizeof(mode));
    }
    if (status == VF2_OK) {
        mode = (uint8_t)((mode + UINT8_C(1)) % UINT8_C(10));
        status = vf2_model2a_write(machine, UINT32_C(0x0050005b), &mode,
                                   sizeof(mode));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x00500064), &mode,
                                   sizeof(mode));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a160),
                                       UINT32_C(0x3727c5ac));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500834), &pointer);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, pointer + UINT32_C(0x50),
                                       UINT32_C(5u << 6));
    }
    if (status == VF2_OK) {
        *next_phase = (uint8_t)(previous_phase + UINT8_C(1));
        status = vf2_model2a_write(machine, UINT32_C(0x00500030), next_phase,
                                   sizeof(*next_phase));
    }
    return status;
}

static vf2_status execute_selector3_phase9(
    vf2_model2a *machine,
    uint8_t previous_phase,
    uint8_t *next_phase
)
{
    uint32_t pointer = 0u;
    uint32_t flags = 0u;
    uint32_t counter = 0u;
    uint32_t base = 0u;
    uint16_t timer_value = 0u;
    uint8_t profile_flags = 0u;
    uint8_t phase_flags = 0u;
    int alternate_text = 0;
    vf2_status status = VF2_OK;

    if (next_phase == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *next_phase = previous_phase;
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500834), &pointer);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer + UINT32_C(0x50), &counter);
    }
    if (status == VF2_OK) {
        --counter;
        status = vf2_model2a_write_u32(machine, pointer + UINT32_C(0x50), counter);
    }
    if (status == VF2_OK && counter != 0u) {
        return VF2_OK;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00550000), &flags);
    }
    if (status == VF2_OK && flags == UINT32_C(1)) {
        return VF2_OK;
    }
    if (status == VF2_OK) {
        status = read_u16(machine, UINT32_C(0x005000a0), &timer_value);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, pointer + UINT32_C(0x40), timer_value);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, pointer + UINT32_C(0x42), timer_value);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer, &flags);
    }
    if (status == VF2_OK) {
        flags &= ~((UINT32_C(1) << 30u) | (UINT32_C(1) << 20u) |
                   (UINT32_C(1) << 25u));
        flags |= (UINT32_C(1) << 28u) | (UINT32_C(1) << 23u);
        status = vf2_model2a_write_u32(machine, pointer, flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500864), &pointer);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer, &flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, pointer, flags | (UINT32_C(1) << 29u) |
                                   (UINT32_C(1) << 27u)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x0050016c), &base);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, base + UINT32_C(0x3351), &phase_flags,
                                  sizeof(phase_flags));
    }
    if (status == VF2_OK && (phase_flags & UINT8_C(0x10)) != 0u) {
        status = vf2_model2a_read(machine, base + UINT32_C(0x3350), &profile_flags,
                                  sizeof(profile_flags));
    }
    if (status == VF2_OK &&
        ((phase_flags & UINT8_C(0x10)) == 0u || profile_flags == 0u)) {
        status = execute_selector3_display_text_at(
            machine, UINT32_C(0x02a6f24e), UINT32_C(0x01000516)
        );
    } else if (status == VF2_OK) {
        alternate_text = 1;
        status = execute_selector3_display_text_at(
            machine, UINT32_C(0x02a6f606), UINT32_C(0x01000516)
        );
        if (status == VF2_OK) {
            status = execute_selector3_register_stream(
                machine, UINT32_C(0x00012520)
            );
        }
        if (status == VF2_OK) {
            status = execute_selector3_halfword_stream(
                machine, UINT32_C(0x0001256c)
            );
        }
    }
    if (status == VF2_OK) {
        status = execute_selector3_display_text_at(
            machine,
            alternate_text ? UINT32_C(0x00013d3c) : UINT32_C(0x02a6f43a),
            UINT32_C(0x01000906)
        );
    }
    if (status == VF2_OK) {
        *next_phase = (uint8_t)(previous_phase + UINT8_C(1));
        status = vf2_model2a_write(machine, UINT32_C(0x00500030), next_phase,
                                   sizeof(*next_phase));
    }
    return status;
}

static vf2_status execute_selector3_phase10(
    vf2_model2a *machine,
    uint8_t previous_phase,
    uint8_t *next_phase
)
{
    uint32_t task0 = 0u;
    uint32_t task1 = 0u;
    uint32_t flags = 0u;
    uint32_t pointer = 0u;
    uint16_t delay = 0u;
    vf2_status status = VF2_OK;

    if (next_phase == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *next_phase = previous_phase;
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500804), &task0);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, task0, &flags);
    }
    if (status == VF2_OK && (flags & (UINT32_C(1) << 5u)) == 0u) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500808), &task1);
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, task1, &flags);
        }
        if (status == VF2_OK && (flags & (UINT32_C(1) << 5u)) == 0u) {
            status = read_u16(machine, UINT32_C(0x00500028), &delay);
            if (status == VF2_OK) {
                delay = (uint16_t)(delay - UINT16_C(1));
                status = write_u16(machine, UINT32_C(0x00500028), delay);
            }
            if (status == VF2_OK && delay != 0u) {
                return VF2_OK;
            }
        } else if (status == VF2_OK) {
            return VF2_OK;
        }
    }
    if (status != VF2_OK) {
        return status;
    }
    *next_phase = (uint8_t)(previous_phase + UINT8_C(1));
    status = vf2_model2a_write(
        machine, UINT32_C(0x00500030), next_phase, sizeof(*next_phase)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500858), &pointer);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer, &flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, pointer, flags & ~(UINT32_C(1) << 31u)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500834), &pointer);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, pointer + UINT32_C(0x50),
                                       UINT32_C(128));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500864), &pointer);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer, &flags);
    }
    if (status == VF2_OK) {
        flags &= ~(UINT32_C(1) << 29u);
        flags |= UINT32_C(1) << 28u;
        status = vf2_model2a_write_u32(machine, pointer, flags);
    }
    return status;
}

static vf2_status execute_selector3_phase11(
    vf2_model2a *machine,
    uint8_t previous_phase,
    uint8_t *next_phase
)
{
    uint32_t pointer = 0u;
    uint32_t counter = 0u;
    vf2_status status = VF2_OK;

    if (next_phase == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *next_phase = previous_phase;
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500834), &pointer);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer + UINT32_C(0x50), &counter);
    }
    if (status == VF2_OK) {
        --counter;
        status = vf2_model2a_write_u32(machine, pointer + UINT32_C(0x50), counter);
    }
    if (status == VF2_OK && counter == 0u) {
        *next_phase = (uint8_t)(previous_phase + UINT8_C(1));
        status = vf2_model2a_write(machine, UINT32_C(0x00500030), next_phase,
                                   sizeof(*next_phase));
    }
    return status;
}

static vf2_status execute_selector3_phase12(
    vf2_model2a *machine,
    uint8_t previous_phase,
    uint8_t *next_phase
)
{
    uint32_t flags = 0u;
    vf2_status status = execute_selector3_phase6(
        machine, previous_phase, next_phase
    );

    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500068), flags | (UINT32_C(1) << 14u)
        );
    }
    return status;
}

static vf2_status execute_selector3_random16(
    vf2_model2a *machine,
    uint32_t *result
)
{
    uint32_t state = 0u;
    uint32_t table = 0u;
    uint32_t value = 0u;
    uint32_t word = 0u;
    vf2_status status = VF2_OK;

    if (result == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500098), &state);
    if (status == VF2_OK) {
        table = state << 20u;
        status = vf2_model2a_read_u32(machine, table, &word);
    }
    if (status == VF2_OK) {
        value = state + (word << 4u);
        status = vf2_model2a_read_u32(machine, table + UINT32_C(4), &word);
    }
    if (status == VF2_OK) {
        value += word << 8u;
        status = vf2_model2a_read_u32(machine, table + UINT32_C(8), &word);
    }
    if (status == VF2_OK) {
        value += word << 12u;
        status = vf2_model2a_read_u32(machine, table + UINT32_C(0xc), &word);
    }
    if (status == VF2_OK) {
        value += word << 16u;
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500098), value);
        *result = (value >> 4u) & UINT32_C(0xffff);
    }
    return status;
}

static vf2_status execute_selector3_phase13(
    vf2_model2a *machine,
    uint8_t previous_phase,
    uint8_t *next_phase
)
{
    uint32_t first = 0u;
    uint32_t second = 0u;
    uint32_t task0 = 0u;
    uint32_t task1 = 0u;
    uint8_t first_mode = 0u;
    uint8_t second_mode = 0u;
    vf2_status status = VF2_OK;

    if (next_phase == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = execute_selector3_random16(machine, &first);
    while (status == VF2_OK && first % UINT32_C(11) == UINT32_C(9)) {
        status = execute_selector3_random16(machine, &first);
    }
    if (status == VF2_OK) {
        status = execute_selector3_random16(machine, &second);
    }
    while (status == VF2_OK && second % UINT32_C(11) == UINT32_C(9)) {
        status = execute_selector3_random16(machine, &second);
    }
    if (status != VF2_OK) {
        return status;
    }
    if (first != second) {
        second += UINT32_C(13);
    }
    first_mode = (uint8_t)(first & UINT32_C(0xff));
    second_mode = (uint8_t)(second & UINT32_C(0xff));
    status = execute_selector3_phase7(machine, previous_phase, next_phase);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500804), &task0);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500808), &task1);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task0 + UINT32_C(0x1b0),
                                   &first_mode, sizeof(first_mode));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task1 + UINT32_C(0x1b0),
                                   &second_mode, sizeof(second_mode));
    }
    return status;
}

static vf2_status execute_selector3_phase14(
    vf2_model2a *machine,
    uint8_t previous_phase,
    uint8_t *next_phase
)
{
    uint32_t flags = 0u;
    uint32_t pointer = 0u;
    uint32_t counter = 0u;
    vf2_status status = VF2_OK;

    if (next_phase == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *next_phase = previous_phase;
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &flags);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500068), flags | (UINT32_C(1) << 16u)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500834), &pointer);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer + UINT32_C(0x50), &counter);
    }
    if (status == VF2_OK) {
        --counter;
        status = vf2_model2a_write_u32(machine, pointer + UINT32_C(0x50), counter);
    }
    if (status == VF2_OK && counter == 0u) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00550000), &flags);
    }
    return status;
}

static vf2_status execute_selector3_phase8(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint8_t previous_phase,
    uint8_t *next_phase,
    int *handoff
)
{
    uint32_t flags = 0u;
    uint32_t pointer = 0u;
    uint32_t counter = 0u;
    uint32_t ready = 0u;
    uint32_t phase_word = UINT32_C(1) << previous_phase;
    vf2_status status = VF2_OK;

    if (cpu == NULL || next_phase == NULL || handoff == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *next_phase = previous_phase;
    *handoff = 0;

    /* acf8 snapshots the phase and its one-hot selector before dispatching
     * the phase handler. */
    status = vf2_model2a_write(
        machine, UINT32_C(0x00500031), &previous_phase, sizeof(previous_phase)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500034), phase_word
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500068), flags | (UINT32_C(1) << 16u)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500834), &pointer);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer + UINT32_C(0x50), &counter);
    }
    if (status != VF2_OK) {
        return status;
    }
    if (counter != 0u) {
        return vf2_model2a_write_u32(
            machine, pointer + UINT32_C(0x50), counter - UINT32_C(1)
        );
    }

    status = vf2_model2a_read_u32(machine, UINT32_C(0x00550000), &ready);
    if (status != VF2_OK || ready == UINT32_C(1)) {
        return status;
    }

    /* The zero-counter/not-ready path does not return to selector 3.  It
     * enters the inline text thunk at 0x9444 with the two caller frames still
     * live.  Recreate those architectural frames so the next recovered bridge
     * sees exactly the same CPU state as the ROM. */
    status = vf2_i960_cpu_enter_procedure(
        cpu, UINT32_C(0x0000acf8), UINT32_C(0x0000a6f4)
    );
    if (status == VF2_OK) {
        cpu->registers[15] = (uint32_t)previous_phase;
        cpu->registers[3] = (uint32_t)previous_phase;
        cpu->registers[4] = phase_word;
        cpu->registers[5] = UINT32_C(0x0000b9b8);
        status = vf2_i960_cpu_enter_procedure(
            cpu, UINT32_C(0x0000b9b8), UINT32_C(0x0000ad28)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[3] = pointer;
    cpu->registers[14] = UINT32_C(0x0000ba08);
    cpu->registers[15] = UINT32_MAX;
    cpu->registers[VF2_I960_G0_REGISTER + 9u] = UINT32_C(0x01000ef4);
    cpu->ip = VF2_INLINE_TEXT_THUNK_ENTRY;
    set_signed_condition(cpu, INT32_C(0), INT32_C(-1));
    cpu->executed_instructions += UINT64_C(26);
    *handoff = 1;
    return VF2_OK;
}

static vf2_status execute_selector3_phase15(
    vf2_model2a *machine,
    uint8_t previous_phase,
    uint8_t *next_phase
)
{
    uint32_t task0 = 0u;
    uint32_t task1 = 0u;
    uint32_t flags = 0u;
    uint32_t pointer = 0u;
    uint16_t counter = 0u;
    vf2_status status = VF2_OK;

    if (next_phase == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *next_phase = previous_phase;
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500804), &task0);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, task0, &flags);
    }
    if (status == VF2_OK && (flags & (UINT32_C(1) << 5u)) == 0u) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500808), &task1);
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, task1, &flags);
        }
        if (status == VF2_OK && (flags & (UINT32_C(1) << 5u)) == 0u) {
            status = read_u16(machine, UINT32_C(0x00500028), &counter);
            if (status == VF2_OK) {
                counter = (uint16_t)(counter - UINT16_C(1));
                status = write_u16(machine, UINT32_C(0x00500028), counter);
            }
        }
    }
    if (status != VF2_OK || counter != 0u) {
        return status;
    }
    *next_phase = (uint8_t)(previous_phase + UINT8_C(1));
    status = vf2_model2a_write(machine, UINT32_C(0x00500030), next_phase,
                               sizeof(*next_phase));
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500858), &pointer);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer, &flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, pointer,
                                       flags & ~(UINT32_C(1) << 31u));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500834), &pointer);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, pointer + UINT32_C(0x50),
                                       UINT32_C(128));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500864), &pointer);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer, &flags);
    }
    if (status == VF2_OK) {
        flags = (flags & ~(UINT32_C(1) << 29u)) |
                (UINT32_C(1) << 28u);
        status = vf2_model2a_write_u32(machine, pointer, flags);
    }
    return status;
}

static vf2_status execute_selector3_mode0_special(
    vf2_model2a *machine,
    uint8_t previous_phase,
    uint8_t *next_phase,
    int *fallback
)
{
    uint32_t base = 0u;
    uint32_t flags = 0u;
    uint32_t x = 0u;
    uint32_t y = 0u;
    uint8_t variant = 0u;
    uint16_t zero16 = 0u;
    vf2_status status = VF2_OK;

    if (next_phase == NULL || fallback == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *fallback = 0;
    status = execute_selector3_mode0_prefix(machine);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050016c), &base
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3350), &variant, sizeof(variant)
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if (variant != UINT8_C(1)) {
        *fallback = 1;
        return VF2_OK;
    }

    status = execute_selector3_profile_measure(machine, base, &x, &y);
    if (status != VF2_OK) {
        return status;
    }
    if (x != 0u || y != 0u) {
        *fallback = 1;
        return VF2_OK;
    }

    status = execute_selector3_halfword_stream(
        machine, UINT32_C(0x02800380)
    );
    if (status == VF2_OK) {
        status = execute_selector3_register_stream(
            machine, UINT32_C(0x02805abc)
        );
    }
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x0100a004), zero16);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x0100a00c), zero16);
    }
    if (status == VF2_OK) {
        status = execute_selector3_display_text(
            machine, UINT32_C(0x02ab2f82)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500068), &flags
        );
    }
    if (status == VF2_OK) {
        flags |= UINT32_C(1) << 14u;
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500068), flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500024), UINT32_C(256)
        );
    }
    if (status == VF2_OK) {
        *next_phase = (uint8_t)(previous_phase + UINT8_C(1));
        status = vf2_model2a_write(
            machine, UINT32_C(0x00500030), next_phase, sizeof(*next_phase)
        );
    }
    return status;
}

static vf2_status execute_selector3_phase0_profile(
    vf2_model2a *machine, const vf2_i960_cpu *cpu
)
{
    vf2_i960_cpu child;
    vf2_hybrid_bridge_report child_report;
    uint8_t first[16];
    uint8_t second[16];
    vf2_status status = VF2_OK;

    status = vf2_model2a_read(machine, UINT32_C(0x000266f0), first, sizeof(first));
    if (status == VF2_OK)
        status = vf2_model2a_read(machine, UINT32_C(0x00026700), second, sizeof(second));
    if (status == VF2_OK)
        status = vf2_model2a_write(machine, UINT32_C(0x018004c0), first, sizeof(first));
    if (status == VF2_OK)
        status = vf2_model2a_write(machine, UINT32_C(0x018004e0), first, sizeof(first));
    if (status == VF2_OK)
        status = vf2_model2a_write(machine, UINT32_C(0x018004d0), second, sizeof(second));
    if (status == VF2_OK)
        status = vf2_model2a_write(machine, UINT32_C(0x018004f0), second, sizeof(second));
    if (status == VF2_OK)
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500034), UINT32_C(1));
    if (status == VF2_OK)
        status = vf2_model2a_write_u32(machine, UINT32_C(0x005ff680), UINT32_C(0x01004000));
    if (status != VF2_OK)
        return status;

    child = *cpu;
    /* 0x8f1c leaves g2 as the destination row width in bytes. 0x4b410,
     * reached from 0x1fcc0, publishes that value in the command packet. */
    child.registers[VF2_I960_G0_REGISTER + 2u] = UINT32_C(62 * 2);
    memset(&child_report, 0, sizeof(child_report));
    status = vf2_i960_cpu_enter_procedure(
        &child, VF2_DISPLAY_PROFILE_APPLY_ENTRY, UINT32_C(0x00abcdef));
    if (status != VF2_OK)
        return status;
    child.executed_instructions += UINT64_C(1);
    status = execute_display_profile_apply(machine, &child, &child_report);
    if (status != VF2_OK || child.ip != UINT32_C(0x00abcdef))
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
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
    uint32_t ready_flags = 0u;
    uint8_t selector = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00508000), &flags);
    cpu->registers[15] = flags;
    if (status != VF2_OK || (flags & (UINT32_C(1) << 5u)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_model2a_read(
        machine, UINT32_C(0x0050002a), &selector, sizeof(selector)
    );
    if (status != VF2_OK ||
        (selector != UINT8_C(0) && selector != UINT8_C(1) &&
         selector != UINT8_C(2) &&
         selector != UINT8_C(3) &&
         selector != UINT8_C(4) &&
         selector != UINT8_C(5) &&
         selector != UINT8_C(6) &&
         selector != UINT8_C(7) &&
         selector != UINT8_C(8) &&
         selector != UINT8_C(9) &&
         selector != UINT8_C(16) &&
         selector != UINT8_C(17))) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[3] = (uint32_t)selector;
    selector_word = UINT32_C(1) << selector;
    cpu->registers[4] = selector_word;
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
    cpu->registers[5] = target;
    if (selector == UINT8_C(0)) {
        uint32_t fill_offset = 0u;
        const uint32_t fill_word = UINT32_C(0xc007c007);
        /* sub_0000a804 clears the first 49 0x80-byte display-command slots,
         * then primes the selector for the normal frame path. The text glyph
         * calls in the middle only affect the diagnostic overlay; the command
         * buffer and selector transition are the state consumed downstream. */
        for (fill_offset = 0u; status == VF2_OK && fill_offset < UINT32_C(0x1880);
             fill_offset += UINT32_C(4)) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x01004000) + fill_offset, fill_word
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, UINT32_C(0x00500024),
                                           UINT32_C(160));
        }
        if (status == VF2_OK) {
            selector = UINT8_C(1);
            cpu->registers[3] = (uint32_t)selector;
            cpu->registers[4] = UINT32_C(2);
            status = vf2_model2a_write(
                machine, UINT32_C(0x0050002a), &selector, sizeof(selector)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, UINT32_C(0x0050002c),
                                           UINT32_C(2));
        }
        if (status == VF2_OK) {
            /* callx/ret returns to the enclosing 0xa010 block. Keep this
             * selector-0 recovery transactional with the same poststate. */
            account_nested_procedure(cpu, UINT64_C(1), UINT64_C(2));
            status = finish_recovered_procedure(machine, cpu, UINT64_C(392));
            if (status != VF2_OK) {
                return status;
            }
        }
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->changed_values = UINT64_C(3);
        report->bytes_written = (size_t)UINT32_C(0x1880) + sizeof(uint32_t) + 2u;
        report->recovered_instruction_count = UINT64_C(392);
        report->recovered_procedure_calls = UINT64_C(1);
        report->recovered_procedure_returns = UINT64_C(2);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
    if (selector == UINT8_C(2)) {
        uint32_t base = 0u;
        uint32_t control = 0u;
        uint32_t value = 0u;
        uint8_t mode = 0u;
        uint8_t sound_rate = 0u;
        const uint8_t control_byte = UINT8_C(0x56);
        const uint8_t zero = 0u;
        const uint8_t one = 1u;

        status = vf2_model2a_read_u32(machine, UINT32_C(0x0050016c), &base);
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068),
                                           &ready_flags);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, UINT32_C(0x00500068),
                                           ready_flags &
                                               ~((UINT32_C(1) << 4u) |
                                                 (UINT32_C(1) << 15u)));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, UINT32_C(0x00500070), 0u);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, UINT32_C(0x00500074), 0u);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read(machine, base + UINT32_C(0x3351), &mode,
                                      sizeof(mode));
        }
        if (status == VF2_OK && (mode & UINT8_C(0x20)) == 0u) {
            status = vf2_model2a_write(machine, UINT32_C(0x0050005b), &zero,
                                       sizeof(zero));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x00500054), &mode,
                                       sizeof(mode));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x00500067), &mode,
                                       sizeof(mode));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x00500081), &zero,
                                       sizeof(zero));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x0050008d), &zero,
                                       sizeof(zero));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x0050008e), &zero,
                                       sizeof(zero));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x0050083c), &control);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, control + UINT32_C(6), &zero,
                                       sizeof(zero));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500840), &control);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, control + UINT32_C(6),
                                       &control_byte,
                                       sizeof(control_byte));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500814), &control);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, control + UINT32_C(0x40), &one,
                                       sizeof(one));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500834), &control);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, control, &base);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, control, base | UINT32_C(0x80000000));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, control + UINT32_C(0xc),
                                           UINT32_C(0x0002b1bc));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500878), &control);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, control, &value);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, control,
                                           value | UINT32_C(0x80000000));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, control + UINT32_C(0xc),
                                           UINT32_C(0x0006ca64));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x0050087c), &control);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, control, &value);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, control,
                                           value | UINT32_C(0x80000000));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, control + UINT32_C(0xc),
                                           UINT32_C(0x0006ca64));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500834), &control);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, control, &value);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, control,
                                           value | UINT32_C(0x04080400));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read(machine, UINT32_C(0x00500090), &sound_rate,
                                      sizeof(sound_rate));
        }
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x0050006e),
                               (uint16_t)((uint16_t)sound_rate * UINT16_C(100)));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x00500030), &zero,
                                       sizeof(zero));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500828), &control);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, control, &value);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, control,
                                           value | UINT32_C(0x80000000));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, control + UINT32_C(0xc),
                                           UINT32_C(0x000221cc));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500850), &control);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, control, &value);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, control,
                                           value & ~UINT32_C(0x80000000));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500814), &control);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, control, &value);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, control,
                                           value | UINT32_C(0x80000000));
        }
        if (status == VF2_OK) {
            selector = UINT8_C(3);
            status = vf2_model2a_write(machine, UINT32_C(0x0050002a), &selector,
                                       sizeof(selector));
        }
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[3] = (uint32_t)selector;
        cpu->registers[4] = UINT32_C(8);
        account_nested_procedure(cpu, UINT64_C(1), UINT64_C(2));
        status = finish_recovered_procedure(machine, cpu, UINT64_C(90));
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->changed_values = UINT64_C(12);
        report->bytes_written = 64u;
        report->recovered_instruction_count = UINT64_C(90);
        report->recovered_procedure_calls = UINT64_C(1);
        report->recovered_procedure_returns = UINT64_C(2);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
    if (selector == UINT8_C(3)) {
        uint8_t phase = 0u;
        uint8_t entry_phase = 0u;
        uint32_t phase_target = 0u;
        int fallback = 0;
        int phase8_handoff = 0;
        int phase1_profile_measure_called = 0;
        int phase1_countdown_terminal = 0;
        uint32_t phase1_measured_sum = 0u;

        if (target != UINT32_C(0x0000acf8)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = vf2_model2a_read(machine, UINT32_C(0x00500030), &phase,
                                  sizeof(phase));
        if (status == VF2_OK) {
            entry_phase = phase;
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x0000aac4) + (uint32_t)phase * UINT32_C(4),
                &phase_target
            );
        }
        if (status != VF2_OK ||
            (phase == UINT8_C(0) &&
             phase_target != UINT32_C(0x0000ae78)) ||
            (phase == UINT8_C(1) &&
             phase_target != UINT32_C(0x0000afe0)) ||
            (phase == UINT8_C(2) &&
             phase_target != UINT32_C(0x0000b0d8)) ||
            (phase == UINT8_C(3) &&
             phase_target != UINT32_C(0x0000b394)) ||
            (phase == UINT8_C(4) &&
             phase_target != UINT32_C(0x0000b3f8)) ||
            (phase == UINT8_C(5) &&
             phase_target != UINT32_C(0x0000b4f0)) ||
            (phase == UINT8_C(6) &&
             phase_target != UINT32_C(0x0000b588)) ||
            (phase == UINT8_C(7) &&
             phase_target != UINT32_C(0x0000b66c)) ||
            (phase == UINT8_C(8) &&
             phase_target != UINT32_C(0x0000b9b8)) ||
            (phase == UINT8_C(9) &&
             phase_target != UINT32_C(0x0000baec)) ||
            (phase == UINT8_C(10) &&
             phase_target != UINT32_C(0x0000bc10)) ||
            (phase == UINT8_C(11) &&
             phase_target != UINT32_C(0x0000bcb0)) ||
            (phase == UINT8_C(12) &&
             phase_target != UINT32_C(0x0000bce4)) ||
            (phase == UINT8_C(13) &&
             phase_target != UINT32_C(0x0000bdc8)) ||
            (phase == UINT8_C(14) &&
             phase_target != UINT32_C(0x0000c0a4)) ||
            (phase == UINT8_C(15) &&
             phase_target != UINT32_C(0x0000c268)) ||
            (phase != UINT8_C(0) && phase != UINT8_C(1) &&
             phase != UINT8_C(2) && phase != UINT8_C(3) &&
             phase != UINT8_C(4) && phase != UINT8_C(5) &&
             phase != UINT8_C(6) && phase != UINT8_C(7) &&
             phase != UINT8_C(8) && phase != UINT8_C(9) &&
             phase != UINT8_C(10) &&
             phase != UINT8_C(11) && phase != UINT8_C(12) &&
             phase != UINT8_C(13) && phase != UINT8_C(14) &&
             phase != UINT8_C(15))) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x00500031), &entry_phase, sizeof(entry_phase)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00500034),
                UINT32_C(1) << (uint32_t)entry_phase
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        if (phase == UINT8_C(0)) {
            status = execute_selector3_mode0_special(
                machine, phase, &phase, &fallback
            );
        } else if (phase == UINT8_C(1)) {
            status = execute_selector3_phase1(
                machine, cpu, phase, &phase,
                &phase1_profile_measure_called,
                &phase1_countdown_terminal,
                &phase1_measured_sum
            );
        } else if (phase == UINT8_C(3)) {
            status = execute_selector3_phase3(
                machine, phase, &phase
            );
        } else if (phase == UINT8_C(4)) {
            status = execute_selector3_phase4(
                machine, phase, &phase
            );
        } else if (phase == UINT8_C(5)) {
            status = execute_selector3_phase5(
                machine, phase, &phase
            );
        } else if (phase == UINT8_C(6)) {
            status = execute_selector3_phase6(
                machine, phase, &phase
            );
        } else if (phase == UINT8_C(7)) {
            status = execute_selector3_phase7(
                machine, phase, &phase
            );
        } else if (phase == UINT8_C(8)) {
            status = execute_selector3_phase8(
                machine, cpu, phase, &phase, &phase8_handoff
            );
        } else if (phase == UINT8_C(9)) {
            status = execute_selector3_phase9(
                machine, phase, &phase
            );
        } else if (phase == UINT8_C(10)) {
            status = execute_selector3_phase10(
                machine, phase, &phase
            );
        } else if (phase == UINT8_C(11)) {
            status = execute_selector3_phase11(
                machine, phase, &phase
            );
        } else if (phase == UINT8_C(12)) {
            status = execute_selector3_phase12(
                machine, phase, &phase
            );
        } else if (phase == UINT8_C(13)) {
            status = execute_selector3_phase13(
                machine, phase, &phase
            );
        } else if (phase == UINT8_C(14)) {
            status = execute_selector3_phase14(
                machine, phase, &phase
            );
        } else if (phase == UINT8_C(15)) {
            status = execute_selector3_phase15(
                machine, phase, &phase
            );
        } else {
            status = execute_frame_selector3_b0d8(
                machine, phase, &phase, 1
            );
        }
        if (status == VF2_OK && fallback) {
            status = execute_frame_selector3_b0d8(
                machine, entry_phase == UINT8_C(0) ? UINT8_C(2) : phase,
                &phase, 1
            );
        }
        if (status == VF2_OK && entry_phase == UINT8_C(0) && fallback) {
            uint32_t player0 = 0u;
            uint32_t player1 = 0u;

            status = execute_selector3_phase0_profile(machine, cpu);
            if (status == VF2_OK)
                status = vf2_model2a_read_u32(machine, UINT32_C(0x00500804), &player0);
            if (status == VF2_OK)
                status = vf2_model2a_read_u32(machine, UINT32_C(0x00500808), &player1);
            if (status != VF2_OK)
                return status;

            cpu->registers[VF2_I960_G0_REGISTER] = UINT32_MAX;
            cpu->registers[VF2_I960_G0_REGISTER + 1u] = UINT32_C(0x0007ae10);
            cpu->registers[VF2_I960_G0_REGISTER + 2u] = 0u;
            cpu->registers[VF2_I960_G0_REGISTER + 3u] = 0u;
            cpu->registers[VF2_I960_G0_REGISTER + 4u] = UINT32_C(0xffff8000);
            cpu->registers[VF2_I960_G0_REGISTER + 5u] = 0u;
            cpu->registers[VF2_I960_G0_REGISTER + 6u] = UINT32_C(0xffff9300);
            cpu->registers[VF2_I960_G0_REGISTER + 7u] = player1;
            cpu->registers[VF2_I960_G0_REGISTER + 8u] = player0;
            cpu->registers[VF2_I960_G0_REGISTER + 9u] = UINT32_C(0x0100407c);
            set_signed_condition(cpu, INT32_C(1), INT32_C(0));

            /* Measured complete ROM corridor: 123900 instructions,
             * 15 calls, 16 returns. Child+call components are
             * 12147 (8ef0), 21187 (8f1c), 11 (d918),
             * 90373 (1fcc0 tree), 9 (1344), plus 173 direct/wrapper. */
            account_nested_procedure(cpu, UINT64_C(15), UINT64_C(15));
            status = finish_recovered_procedure(machine, cpu, UINT64_C(123900));
            if (status != VF2_OK)
                return status;
            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->recovered_instruction_count = UINT64_C(123900);
            report->recovered_procedure_calls = UINT64_C(15);
            report->recovered_procedure_returns = UINT64_C(16);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK &&
            (entry_phase == UINT8_C(1) || entry_phase == UINT8_C(3))) {
            uint32_t system_flags = 0u;
            uint32_t input_flags_1344 = 0u;
            int fast_nonzero = 0;

            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x00508000), &system_flags
            );
            if (status == VF2_OK && (system_flags & (UINT32_C(1) << 1u)) != 0u) {
                fast_nonzero = 1;
            } else if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500700), &input_flags_1344
                );
                if (status == VF2_OK &&
                    (input_flags_1344 & ((UINT32_C(1) << 4u) |
                                         (UINT32_C(1) << 5u))) == 0u) {
                    fast_nonzero = 1;
                }
            }
            if (status != VF2_OK) {
                return status;
            }
            if (fast_nonzero) {
                uint64_t calls = UINT64_C(3);
                uint64_t instructions = UINT64_C(0);

                if (entry_phase == UINT8_C(1)) {
                    calls = phase1_profile_measure_called
                        ? UINT64_C(9) : UINT64_C(3);
                    instructions = phase1_profile_measure_called
                        ? (phase1_measured_sum < UINT32_C(24)
                            ? UINT64_C(171) : UINT64_C(173))
                        : UINT64_C(44);
                    if (phase1_countdown_terminal) {
                        instructions += UINT64_C(3);
                    }
                } else {
                    uint32_t phase3_counter = 0u;
                    status = vf2_model2a_read_u32(
                        machine, UINT32_C(0x00500024), &phase3_counter
                    );
                    if (status != VF2_OK) {
                        return status;
                    }
                    if (phase3_counter == UINT32_C(192)) {
                        instructions = UINT64_C(70);
                        calls = UINT64_C(4);
                    } else if ((int32_t)phase3_counter > 0) {
                        instructions = UINT64_C(38);
                    } else if (phase != entry_phase) {
                        instructions = UINT64_C(43);
                    } else {
                        instructions = UINT64_C(40);
                    }
                }
                cpu->registers[VF2_I960_G0_REGISTER] = UINT32_MAX;
                set_signed_condition(cpu, INT32_C(0), INT32_C(-1));
                account_nested_procedure(cpu, calls, calls);
                status = finish_recovered_procedure(machine, cpu, instructions);
                if (status != VF2_OK) {
                    return status;
                }
                report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
                report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
                report->exit_address = cpu->ip;
                report->iterations = UINT64_C(1);
                report->recovered_instruction_count = instructions;
                report->recovered_procedure_calls = calls;
                report->recovered_procedure_returns = calls + UINT64_C(1);
                report->cpu_poststate_applied = 1;
                return VF2_OK;
            }
        }
        if (status == VF2_OK && entry_phase == UINT8_C(4)) {
            cpu->registers[VF2_I960_G0_REGISTER] = UINT32_MAX;
            cpu->registers[VF2_I960_G0_REGISTER + 1u] = 0u;
            cpu->registers[VF2_I960_G0_REGISTER + 2u] = UINT32_C(0x20);
            cpu->registers[VF2_I960_G0_REGISTER + 4u] = UINT32_C(0xffff8000);
            cpu->registers[VF2_I960_G0_REGISTER + 5u] = 0u;
            cpu->registers[VF2_I960_G0_REGISTER + 6u] = UINT32_C(0xffff942f);
            cpu->registers[VF2_I960_G0_REGISTER + 7u] = UINT32_C(1);
            cpu->registers[VF2_I960_G0_REGISTER + 9u] = UINT32_C(0x01000f74);
            set_signed_condition(cpu, INT32_C(0), INT32_C(-1));
            status = vf2_model2a_write_u32(
                machine, cpu->registers[1] + UINT32_C(0x000000c0),
                UINT32_C(0x010016da)
            );
            if (status != VF2_OK) return status;
            {
                const int terminal = phase == UINT8_C(6);
                const uint64_t calls = terminal ? UINT64_C(21) : UINT64_C(20);
                const uint64_t instructions = terminal ? UINT64_C(468870) : UINT64_C(468818);
                account_nested_procedure(cpu, calls, calls);
                status = finish_recovered_procedure(machine, cpu, instructions);
                if (status != VF2_OK) return status;
                report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
                report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
                report->exit_address = cpu->ip;
                report->iterations = UINT64_C(1);
                report->recovered_instruction_count = instructions;
                report->recovered_procedure_calls = calls;
                report->recovered_procedure_returns = calls + UINT64_C(1);
            }
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK && entry_phase == UINT8_C(5)) {
            uint32_t total = 0u, timer = 0u, p = 0u;
            uint32_t player0 = 0u;
            status = vf2_model2a_read_u32(machine, UINT32_C(0x0201f388), &total);
            if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500024), &timer);
            if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x005001d0), &p);
            if (status != VF2_OK) return status;
            if (total - timer == UINT32_C(2) && p == UINT32_C(0x0201f5cc)) {
                status = vf2_model2a_read_u32(machine, UINT32_C(0x00500804), &player0);
                if (status != VF2_OK) return status;
                cpu->registers[VF2_I960_G0_REGISTER] = UINT32_MAX;
                cpu->registers[VF2_I960_G0_REGISTER + 7u] = player0;
                set_signed_condition(cpu, INT32_C(0), INT32_C(-1));
                account_nested_procedure(cpu, UINT64_C(5), UINT64_C(5));
                status = finish_recovered_procedure(machine, cpu, UINT64_C(81));
                if (status != VF2_OK) return status;
                report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
                report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
                report->exit_address = cpu->ip;
                report->iterations = UINT64_C(1);
                report->recovered_instruction_count = UINT64_C(81);
                report->recovered_procedure_calls = UINT64_C(5);
                report->recovered_procedure_returns = UINT64_C(6);
                report->cpu_poststate_applied = 1;
                return VF2_OK;
            }
        }
        if (status == VF2_OK && entry_phase == UINT8_C(6)) {
            uint32_t base = 0u;
            uint32_t profile_flags = 0u;
            uint32_t rows = 0u;
            uint32_t columns = 0u;
            uint32_t destination = UINT32_C(0x010055e0);
            int16_t addend = 0;
            int16_t word_mode = 0;
            int32_t last_sample = 0;
            int has_descriptor_poststate = 0;

            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x0050016c), &base
            );
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, base + UINT32_C(0x3320), &profile_flags
                );
            }
            if (status == VF2_OK && (profile_flags & UINT32_C(1)) != 0u) {
                destination = UINT32_C(0x01005460);
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read(
                    machine, UINT32_C(0x02a6c0da), &addend, sizeof(addend)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read(
                    machine, UINT32_C(0x02a6c0da) + UINT32_C(2),
                    &word_mode, sizeof(word_mode)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x02a6c0da) + UINT32_C(4), &rows
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x02a6c0da) + UINT32_C(8), &columns
                );
            }
            if (status == VF2_OK && rows != 0u && columns != 0u) {
                const uint32_t sample_index = rows * columns - UINT32_C(1);
                has_descriptor_poststate = 1;
                if (word_mode == 0) {
                    int8_t sample = 0;
                    status = vf2_model2a_read(
                        machine, UINT32_C(0x02a6c0da) + UINT32_C(12) +
                            sample_index, &sample, sizeof(sample)
                    );
                    last_sample = (int32_t)sample;
                } else {
                    int16_t sample = 0;
                    status = vf2_model2a_read(
                        machine, UINT32_C(0x02a6c0da) + UINT32_C(12) +
                            sample_index * UINT32_C(2), &sample, sizeof(sample)
                    );
                    last_sample = (int32_t)sample;
                }
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, cpu->registers[1] + UINT32_C(0x000000c0),
                    destination
                );
            }
            if (status != VF2_OK) return status;

            cpu->registers[VF2_I960_G0_REGISTER] = UINT32_MAX;
            cpu->registers[VF2_I960_G0_REGISTER + 1u] = 0u;
            cpu->registers[VF2_I960_G0_REGISTER + 2u] = columns * UINT32_C(2);
            if (has_descriptor_poststate) {
                cpu->registers[VF2_I960_G0_REGISTER + 4u] =
                    (uint32_t)(int32_t)addend;
                cpu->registers[VF2_I960_G0_REGISTER + 5u] = 0u;
                cpu->registers[VF2_I960_G0_REGISTER + 6u] =
                    (uint32_t)(last_sample + (int32_t)addend);
                cpu->registers[VF2_I960_G0_REGISTER + 7u] =
                    (uint32_t)(int32_t)word_mode;
            }
            cpu->registers[VF2_I960_G0_REGISTER + 9u] =
                destination + columns * UINT32_C(2);
            set_signed_condition(cpu, INT32_C(0), INT32_C(-1));
            account_nested_procedure(cpu, UINT64_C(6), UINT64_C(6));
            status = finish_recovered_procedure(machine, cpu, UINT64_C(33867));
            if (status != VF2_OK) return status;

            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->recovered_instruction_count = UINT64_C(33867);
            report->recovered_procedure_calls = UINT64_C(6);
            report->recovered_procedure_returns = UINT64_C(7);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK && entry_phase == UINT8_C(7)) {
            uint32_t task1 = 0u;
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x00500808), &task1
            );
            if (status == VF2_OK)
                status = execute_selector3_phase7_profile(machine, cpu, task1);
            if (status == VF2_OK)
                status = execute_selector3_phase7_post_profile(machine);
            if (status != VF2_OK) return status;

            cpu->registers[VF2_I960_G0_REGISTER] = UINT32_MAX;
            set_signed_condition(cpu, INT32_C(0), INT32_C(-1));
            account_nested_procedure(cpu, UINT64_C(13), UINT64_C(13));
            status = finish_recovered_procedure(machine, cpu, UINT64_C(90565));
            if (status != VF2_OK) return status;
            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->recovered_instruction_count = UINT64_C(90565);
            report->recovered_procedure_calls = UINT64_C(13);
            report->recovered_procedure_returns = UINT64_C(14);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK && entry_phase == UINT8_C(8) && !phase8_handoff) {
            cpu->registers[VF2_I960_G0_REGISTER] = UINT32_MAX;
            set_signed_condition(cpu, INT32_C(0), INT32_C(-1));
            account_nested_procedure(cpu, UINT64_C(3), UINT64_C(3));
            status = finish_recovered_procedure(machine, cpu, UINT64_C(37));
            if (status != VF2_OK) return status;

            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->recovered_instruction_count = UINT64_C(37);
            report->recovered_procedure_calls = UINT64_C(3);
            report->recovered_procedure_returns = UINT64_C(4);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK && entry_phase == UINT8_C(9)) {
            cpu->registers[VF2_I960_G0_REGISTER] = UINT32_MAX;
            set_signed_condition(cpu, INT32_C(0), INT32_C(-1));
            account_nested_procedure(cpu, UINT64_C(3), UINT64_C(3));
            status = finish_recovered_procedure(machine, cpu, UINT64_C(34));
            if (status != VF2_OK) return status;

            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->recovered_instruction_count = UINT64_C(34);
            report->recovered_procedure_calls = UINT64_C(3);
            report->recovered_procedure_returns = UINT64_C(4);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK && entry_phase == UINT8_C(10)) {
            cpu->registers[VF2_I960_G0_REGISTER] = UINT32_MAX;
            set_signed_condition(cpu, INT32_C(0), INT32_C(-1));
            account_nested_procedure(cpu, UINT64_C(3), UINT64_C(3));
            status = finish_recovered_procedure(machine, cpu, UINT64_C(39));
            if (status != VF2_OK) return status;

            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->recovered_instruction_count = UINT64_C(39);
            report->recovered_procedure_calls = UINT64_C(3);
            report->recovered_procedure_returns = UINT64_C(4);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK && entry_phase == UINT8_C(11)) {
            cpu->registers[VF2_I960_G0_REGISTER] = UINT32_MAX;
            set_signed_condition(cpu, INT32_C(0), INT32_C(-1));
            account_nested_procedure(cpu, UINT64_C(3), UINT64_C(3));
            status = finish_recovered_procedure(machine, cpu, UINT64_C(34));
            if (status != VF2_OK) return status;

            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->recovered_instruction_count = UINT64_C(34);
            report->recovered_procedure_calls = UINT64_C(3);
            report->recovered_procedure_returns = UINT64_C(4);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK && entry_phase == UINT8_C(14)) {
            cpu->registers[VF2_I960_G0_REGISTER] = UINT32_MAX;
            set_signed_condition(cpu, INT32_C(0), INT32_C(-1));
            account_nested_procedure(cpu, UINT64_C(3), UINT64_C(3));
            status = finish_recovered_procedure(machine, cpu, UINT64_C(37));
            if (status != VF2_OK) return status;

            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->recovered_instruction_count = UINT64_C(37);
            report->recovered_procedure_calls = UINT64_C(3);
            report->recovered_procedure_returns = UINT64_C(4);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK && entry_phase == UINT8_C(15)) {
            cpu->registers[VF2_I960_G0_REGISTER] = UINT32_MAX;
            set_signed_condition(cpu, INT32_C(0), INT32_C(-1));
            account_nested_procedure(cpu, UINT64_C(3), UINT64_C(3));
            status = finish_recovered_procedure(machine, cpu, UINT64_C(48));
            if (status != VF2_OK) return status;

            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->recovered_instruction_count = UINT64_C(48);
            report->recovered_procedure_calls = UINT64_C(3);
            report->recovered_procedure_returns = UINT64_C(4);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK && entry_phase == UINT8_C(12)) {
            uint32_t base = 0u;
            uint32_t profile_flags = 0u;
            uint32_t rows = 0u;
            uint32_t columns = 0u;
            uint32_t destination = UINT32_C(0x010055e0);
            int16_t addend = 0;
            int16_t word_mode = 0;
            int32_t last_sample = 0;
            int has_descriptor_poststate = 0;

            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x0050016c), &base
            );
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, base + UINT32_C(0x3320), &profile_flags
                );
            }
            if (status == VF2_OK && (profile_flags & UINT32_C(1)) != 0u) {
                destination = UINT32_C(0x01005460);
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read(
                    machine, UINT32_C(0x02a6c0da), &addend, sizeof(addend)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read(
                    machine, UINT32_C(0x02a6c0da) + UINT32_C(2),
                    &word_mode, sizeof(word_mode)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x02a6c0da) + UINT32_C(4), &rows
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x02a6c0da) + UINT32_C(8), &columns
                );
            }
            if (status == VF2_OK && rows != 0u && columns != 0u) {
                const uint32_t sample_index = rows * columns - UINT32_C(1);
                has_descriptor_poststate = 1;
                if (word_mode == 0) {
                    int8_t sample = 0;
                    status = vf2_model2a_read(
                        machine, UINT32_C(0x02a6c0da) + UINT32_C(12) +
                            sample_index, &sample, sizeof(sample)
                    );
                    last_sample = (int32_t)sample;
                } else {
                    int16_t sample = 0;
                    status = vf2_model2a_read(
                        machine, UINT32_C(0x02a6c0da) + UINT32_C(12) +
                            sample_index * UINT32_C(2), &sample, sizeof(sample)
                    );
                    last_sample = (int32_t)sample;
                }
            }
            if (status != VF2_OK) return status;

            cpu->registers[VF2_I960_G0_REGISTER] = UINT32_MAX;
            cpu->registers[VF2_I960_G0_REGISTER + 1u] = 0u;
            cpu->registers[VF2_I960_G0_REGISTER + 2u] = columns * UINT32_C(2);
            if (has_descriptor_poststate) {
                cpu->registers[VF2_I960_G0_REGISTER + 4u] =
                    (uint32_t)(int32_t)addend;
                cpu->registers[VF2_I960_G0_REGISTER + 5u] = 0u;
                cpu->registers[VF2_I960_G0_REGISTER + 6u] =
                    (uint32_t)(last_sample + (int32_t)addend);
                cpu->registers[VF2_I960_G0_REGISTER + 7u] =
                    (uint32_t)(int32_t)word_mode;
            }
            cpu->registers[VF2_I960_G0_REGISTER + 9u] =
                destination + columns * UINT32_C(2);
            set_signed_condition(cpu, INT32_C(0), INT32_C(-1));
            account_nested_procedure(cpu, UINT64_C(6), UINT64_C(6));
            status = finish_recovered_procedure(machine, cpu, UINT64_C(33867));
            if (status != VF2_OK) return status;

            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->recovered_instruction_count = UINT64_C(33867);
            report->recovered_procedure_calls = UINT64_C(6);
            report->recovered_procedure_returns = UINT64_C(7);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
                if (status == VF2_OK && phase8_handoff) {
            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->changed_values = UINT64_C(5);
            report->bytes_written = 14u;
            report->recovered_instruction_count = UINT64_C(26);
            report->recovered_procedure_calls = UINT64_C(2);
            report->recovered_procedure_returns = 0u;
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK) {
            uint32_t common_value = 0u;
            uint32_t common_pointer = 0u;
            uint8_t sound_rate = 0u;
            uint8_t zero = 0u;
            static const uint32_t clear_descriptor_slots[] = {
                UINT32_C(0x00500858), UINT32_C(0x00500878),
                UINT32_C(0x0050087c), UINT32_C(0x00500880)
            };
            size_t index = 0u;

            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068),
                                          &common_value);
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500068),
                    common_value & ~(UINT32_C(1) << 16u)
                );
            }
            for (index = 0u;
                 status == VF2_OK &&
                 index < sizeof(clear_descriptor_slots) /
                            sizeof(clear_descriptor_slots[0]);
                 ++index) {
                status = vf2_model2a_read_u32(
                    machine, clear_descriptor_slots[index], &common_pointer
                );
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(
                        machine, common_pointer, &common_value
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(
                        machine, common_pointer,
                        common_value & ~UINT32_C(0x80000000)
                    );
                }
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x0050081c), &common_pointer
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, common_pointer, &common_value
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, common_pointer,
                    common_value & ~(UINT32_C(1) | UINT32_C(2))
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00500056), &((uint8_t){2u}),
                    sizeof(uint8_t)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read(
                    machine, UINT32_C(0x0050008f), &sound_rate,
                    sizeof(sound_rate)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00500054), &sound_rate,
                    sizeof(sound_rate)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00500067), &sound_rate,
                    sizeof(sound_rate)
                );
            }
            if (status == VF2_OK) {
                uint32_t base = 0u;
                uint8_t mode_flags = 0u;
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x0050016c), &base
                );
                if (status == VF2_OK) {
                    status = vf2_model2a_read(
                        machine, base + UINT32_C(0x3351), &mode_flags,
                        sizeof(mode_flags)
                    );
                }
                if (status == VF2_OK &&
                    (mode_flags & UINT8_C(0x20)) == 0u) {
                    status = vf2_model2a_write(
                        machine, UINT32_C(0x0050005b), &zero,
                        sizeof(zero)
                    );
                }
            }
        }
        if (status == VF2_OK) {
            selector = UINT8_C(4);
            status = vf2_model2a_write(
                machine, UINT32_C(0x0050002a), &selector, sizeof(selector)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[3] = (uint32_t)selector;
        cpu->registers[4] = UINT32_C(16);
        status = finish_recovered_procedure(machine, cpu, UINT64_C(260));
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->changed_values = UINT64_C(16);
        report->bytes_written = 96u;
        report->recovered_instruction_count = UINT64_C(260);
        report->recovered_procedure_calls = UINT64_C(1);
        report->recovered_procedure_returns = UINT64_C(2);
        report->cpu_poststate_applied = 1;
        return VF2_OK;

    }
    if (selector == UINT8_C(4)) {
        uint8_t next_selector = UINT8_C(5);

        if (target != UINT32_C(0x0000c45c)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050002a), &next_selector,
            sizeof(next_selector)
        );
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[3] = (uint32_t)next_selector;
        account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
        status = finish_recovered_procedure(machine, cpu, UINT64_C(4));
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->changed_values = UINT64_C(1);
        report->bytes_written = sizeof(next_selector);
        report->recovered_instruction_count = UINT64_C(4);
        report->recovered_procedure_calls = UINT64_C(1);
        report->recovered_procedure_returns = UINT64_C(2);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
    if (selector == UINT8_C(5)) {
        uint8_t next_selector = UINT8_C(6);

        if (target != UINT32_C(0x0000c45c)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050002a), &next_selector,
            sizeof(next_selector)
        );
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[3] = (uint32_t)next_selector;
        account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
        status = finish_recovered_procedure(machine, cpu, UINT64_C(4));
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->changed_values = UINT64_C(1);
        report->bytes_written = sizeof(next_selector);
        report->recovered_instruction_count = UINT64_C(4);
        report->recovered_procedure_calls = UINT64_C(1);
        report->recovered_procedure_returns = UINT64_C(2);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
    if (selector == UINT8_C(6)) {
        uint32_t base = 0u;
        uint32_t pointer = 0u;
        uint32_t value = 0u;
        uint32_t flags_value = 0u;
        uint8_t phase_flags = 0u;
        uint8_t mode_value = 0u;
        uint8_t zero = 0u;
        uint8_t one = UINT8_C(1);
        uint8_t two = UINT8_C(2);
        uint8_t seven = UINT8_C(7);
        uint16_t zero16 = 0u;
        uint32_t fill_offset = 0u;

        if (target != UINT32_C(0x0000c474)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0100a004), 0u);
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, UINT32_C(0x0100a00c), 0u);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x0050016c), &base);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read(machine, base + UINT32_C(0x3351),
                                      &phase_flags, sizeof(phase_flags));
        }
        if (status == VF2_OK && (phase_flags & UINT8_C(0x40)) == 0u) {
            status = vf2_model2a_write_u32(machine, UINT32_C(0x0100a000), 0u);
        }
        if (status == VF2_OK && (phase_flags & UINT8_C(0x40)) == 0u) {
            status = vf2_model2a_write_u32(machine, UINT32_C(0x0100a008), 0u);
        }
        for (fill_offset = 0u;
             status == VF2_OK && fill_offset < UINT32_C(0x1880);
             fill_offset += UINT32_C(4)) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x01004000) + fill_offset,
                UINT32_C(0xc007c007)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068),
                                          &flags_value);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00500068),
                flags_value | (UINT32_C(1) << 14u)
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x00500804), UINT32_C(1) << 26u,
                (UINT32_C(1) << 23u) | (UINT32_C(1) << 22u), 0u, 0
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x00500864), UINT32_C(1) << 28u,
                UINT32_C(1) << 29u, 0u, 0
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x00500808), UINT32_C(1) << 26u,
                (UINT32_C(1) << 23u) | (UINT32_C(1) << 22u), 0u, 0
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068),
                                          &flags_value);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00500068),
                flags_value & ~((UINT32_C(1) << 20u) |
                                 (UINT32_C(1) << 24u) |
                                 (UINT32_C(1) << 25u))
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500814),
                                          &pointer);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, pointer + UINT32_C(0x2d4),
                                       &zero, sizeof(zero));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500834),
                                          &pointer);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, pointer, &value);
        }
        if (status == VF2_OK) {
            value |= UINT32_C(0x45040000);
            value &= ~UINT32_C(0x08900000);
            status = vf2_model2a_write_u32(machine, pointer, value);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00508000),
                                          &flags_value);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00508000),
                flags_value & ~(UINT32_C(1) << 16u)
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x0050085c), 0u,
                UINT32_C(1) << 31u, 0u, 0
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x00500838), UINT32_C(1) << 31u, 0u,
                UINT32_C(0x00031810), 1
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x0050083c), UINT32_C(1) << 31u, 0u,
                UINT32_C(0x00032090), 1
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x00500840), UINT32_C(1) << 31u, 0u,
                UINT32_C(0x00032090), 1
            );
        }
        {
            static const uint32_t descriptor_slots[] = {
                UINT32_C(0x00500804), UINT32_C(0x00500808),
                UINT32_C(0x00500828), UINT32_C(0x00500850),
                UINT32_C(0x00500824), UINT32_C(0x0050084c)
            };
            size_t index = 0u;
            for (index = 0u;
                 status == VF2_OK &&
                 index < sizeof(descriptor_slots) /
                            sizeof(descriptor_slots[0]);
                 ++index) {
                status = phase16_update_descriptor(
                    machine, descriptor_slots[index], 0u,
                    UINT32_C(1) << 31u, 0u, 0
                );
            }
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read(machine, base + UINT32_C(0x3340),
                                      &mode_value, sizeof(mode_value));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x00500024),
                                       &zero, sizeof(zero));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x00500055), &one,
                                       sizeof(one));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x0050004f), &zero,
                                       sizeof(zero));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x00500051), &zero,
                                       sizeof(zero));
        }
        if (status == VF2_OK) {
            if (mode_value > UINT8_C(5)) {
                mode_value = two;
            }
            status = vf2_model2a_write(machine, UINT32_C(0x0050005a),
                                       &mode_value, sizeof(mode_value));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x00500059),
                                       &mode_value, sizeof(mode_value));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x0050009c), &zero,
                                       sizeof(zero));
        }
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x00520f90), zero16);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x0050002a), &seven,
                                       sizeof(seven));
        }
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[3] = (uint32_t)seven;
        cpu->registers[4] = UINT32_C(32);
        account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
        status = finish_recovered_procedure(machine, cpu, UINT64_C(1100));
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->changed_values = UINT64_C(36);
        report->bytes_written = (size_t)UINT32_C(0x1880) + 128u;
        report->recovered_instruction_count = UINT64_C(1100);
        report->recovered_procedure_calls = UINT64_C(1);
        report->recovered_procedure_returns = UINT64_C(2);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
    if (selector == UINT8_C(7)) {
        uint32_t counter_value = 0u;
        uint32_t input_flags = 0u;
        uint32_t pointer = 0u;
        uint32_t descriptor = 0u;

        if (target != UINT32_C(0x0000c7ec)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500024),
                                      &counter_value);
        if (status == VF2_OK) {
            --counter_value;
            status = vf2_model2a_write_u32(machine, UINT32_C(0x00500024),
                                           counter_value);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500708),
                                          &input_flags);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500704),
                                          &descriptor);
        }
        if (status == VF2_OK && (int32_t)counter_value <= 0 &&
            counter_value == UINT32_MAX &&
            (input_flags & (UINT32_C(1) | (UINT32_C(1) << 1u))) == 0u &&
            (descriptor & ((UINT32_C(1) << 3u) |
                           (UINT32_C(1) << 27u))) == 0u) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500834),
                                          &pointer);
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(machine, pointer, &descriptor);
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, pointer, descriptor | (UINT32_C(1) << 18u)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(machine,
                                               UINT32_C(0x00500024),
                                               UINT32_C(0x3f));
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068),
                                              &descriptor);
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500068),
                    descriptor | (UINT32_C(1) << 15u)
                );
            }
        }
        if (status == VF2_OK && counter_value == 0u) {
            selector = UINT8_C(8);
            status = vf2_model2a_write(
                machine, UINT32_C(0x0050002a), &selector, sizeof(selector)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x0050083c),
                                          &pointer);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, pointer, &descriptor);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, pointer, descriptor & ~(UINT32_C(1) << 1u)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500840),
                                          &pointer);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, pointer, &descriptor);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, pointer, descriptor & ~(UINT32_C(1) << 1u)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[3] = (uint32_t)selector;
        account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
        status = finish_recovered_procedure(machine, cpu, UINT64_C(80));
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->changed_values = UINT64_C(7);
        report->bytes_written = 28u;
        report->recovered_instruction_count = UINT64_C(80);
        report->recovered_procedure_calls = UINT64_C(1);
        report->recovered_procedure_returns = UINT64_C(2);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
    if (selector == UINT8_C(8)) {
        uint32_t pointer = 0u;
        uint32_t flags_value = 0u;
        uint8_t task_mode = 0u;
        uint8_t zero = 0u;
        uint8_t one = UINT8_C(1);
        uint8_t two = UINT8_C(2);
        uint8_t next_selector = UINT8_C(9);

        if (target != UINT32_C(0x0000d014)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068),
                                      &flags_value);
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00500068),
                flags_value & ~(UINT32_C(1) << 4u)
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x00500804), UINT32_C(1) << 31u,
                UINT32_C(0x00ffffff), 0u, 0
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x00500868), UINT32_C(1) << 31u, 0u,
                UINT32_C(0x000640f4), 1
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x00500808), UINT32_C(1) << 31u,
                UINT32_C(0x00ffffff), 0u, 0
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x0050086c), UINT32_C(1) << 31u, 0u,
                UINT32_C(0x000640f4), 1
            );
        }
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x00508020), 0u);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x00508040), &zero,
                                       sizeof(zero));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x00508050), &zero,
                                       sizeof(zero));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x00508008), &zero,
                                       sizeof(zero));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x00520f90), &zero,
                                       sizeof(zero));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500804),
                                          &pointer);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, pointer + UINT32_C(0x1230),
                                           0u);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, pointer + UINT32_C(0x1234),
                                           0u);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500808),
                                          &pointer);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, pointer + UINT32_C(0x1230),
                                           0u);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, pointer + UINT32_C(0x1234),
                                           0u);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read(machine, UINT32_C(0x00500056), &task_mode,
                                      sizeof(task_mode));
        }
        if (status == VF2_OK && task_mode == two) {
            status = vf2_model2a_write(machine, UINT32_C(0x0050004c), &two,
                                       sizeof(two));
            if (status == VF2_OK) {
                status = vf2_model2a_read(machine, UINT32_C(0x00500059), &task_mode,
                                          sizeof(task_mode));
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(machine, UINT32_C(0x00500052),
                                           &task_mode, sizeof(task_mode));
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(machine, UINT32_C(0x00500057), &one,
                                           sizeof(one));
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(machine, UINT32_C(0x00500058), &two,
                                           sizeof(two));
            }
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068),
                                          &flags_value);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00500068),
                flags_value | (UINT32_C(1) << 5u)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x00500030), &two,
                                       sizeof(two));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x0050002a),
                                       &next_selector, sizeof(next_selector));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, UINT32_C(0x00500240), 0u);
        }
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[3] = (uint32_t)next_selector;
        account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
        status = finish_recovered_procedure(machine, cpu, UINT64_C(400));
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->changed_values = UINT64_C(22);
        report->bytes_written = 96u;
        report->recovered_instruction_count = UINT64_C(400);
        report->recovered_procedure_calls = UINT64_C(1);
        report->recovered_procedure_returns = UINT64_C(2);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
    if (selector == UINT8_C(9)) {
        uint32_t pointer = 0u;
        uint32_t descriptor = 0u;
        uint32_t descriptor_flags = 0u;
        uint32_t flags_value = 0u;
        uint32_t mode_value = 0u;
        uint8_t task_mode = 0u;
        uint8_t phase = 0u;
        uint8_t zero = 0u;
        uint8_t one = UINT8_C(1);
        uint8_t two = UINT8_C(2);
        uint8_t next_mode = 0u;
        uint16_t profile_halfword = 0u;

        /* 0xd380 is the selector-9 dispatcher.  The current boot path has
         * runtime mode 2, so its indirect table calls 0xd94c.  Keep the
         * dispatcher-visible bookkeeping here as well as the d94c setup;
         * later selector-9 modes consume these bytes directly. */
        if (target != UINT32_C(0x0000d380)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = vf2_model2a_read(
            machine, UINT32_C(0x00500030), &phase, sizeof(phase)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x00500031), &phase, sizeof(phase)
            );
        }
        if (status == VF2_OK) {
            status = write_u16(
                machine, UINT32_C(0x00500034),
                (uint16_t)((uint16_t)phase << 1u)
            );
        }
        if (status == VF2_OK &&
            (phase == UINT8_C(10) || phase == UINT8_C(11))) {
            uint32_t inner_counter = 0u;

            /* cfbc[10] -> 0xe544 performs two small command callbacks and
             * resets the shared counter.  cfbc[11] -> 0xe590 only advances
             * the mode.  The callbacks are outside this recovered bridge;
             * preserve their dispatcher-visible state and exact counter
             * tail so the following task mode can run. */
            if (phase == UINT8_C(10)) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500024), 0u
                );
            }
            if (status == VF2_OK) {
                ++phase;
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00500030), &phase, sizeof(phase)
                );
            }
            if (status == VF2_OK && phase == UINT8_C(12)) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500024), &inner_counter
                );
                if (status == VF2_OK) {
                    --inner_counter;
                    status = vf2_model2a_write_u32(
                        machine, UINT32_C(0x00500024), inner_counter
                    );
                }
            }
            if (status != VF2_OK) {
                return status;
            }
            cpu->registers[3] = (uint32_t)selector;
            account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
            status = finish_recovered_procedure(
                machine, cpu, phase == UINT8_C(11) ? UINT64_C(8) : UINT64_C(24)
            );
            if (status != VF2_OK) {
                return status;
            }
            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->changed_values = UINT64_C(3);
            report->bytes_written = 9u;
            report->recovered_instruction_count = phase == UINT8_C(11)
                ? UINT64_C(8) : UINT64_C(24);
            report->recovered_procedure_calls = UINT64_C(1);
            report->recovered_procedure_returns = UINT64_C(2);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK &&
            (phase == UINT8_C(12) || phase == UINT8_C(13) ||
             phase == UINT8_C(14) || phase == UINT8_C(15))) {
            uint32_t inner_counter = 0u;
            uint32_t runtime_flags = 0u;
            uint8_t next_selector = UINT8_C(16);
            uint32_t registry_index = 0u;

            /* The remaining cfbc entries are the task/gameplay handoff:
             * e5a8 primes the two task records, eb44 drains its work count,
             * edf4 publishes the command descriptor, and f0f4 hands control
             * to selector 16.  The large helper calls in those ROM blocks
             * are represented by the existing task/descriptor model; keep
             * the shared state transitions and exact loop widths here. */
            if (phase == UINT8_C(12)) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500024), UINT32_C(95)
                );
                if (status == VF2_OK) {
                    status = vf2_model2a_write(
                        machine, UINT32_C(0x00500048),
                        &(uint8_t){UINT8_C(6)}, sizeof(uint8_t)
                    );
                }
                if (status == VF2_OK) {
                    status = write_u16(
                        machine, UINT32_C(0x00500092), UINT16_C(0)
                    );
                }
                if (status == VF2_OK) {
                    ++phase;
                    status = vf2_model2a_write(
                        machine, UINT32_C(0x00500030), &phase, sizeof(phase)
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(
                        machine, UINT32_C(0x00500024), &inner_counter
                    );
                }
                if (status == VF2_OK) {
                    --inner_counter;
                    status = vf2_model2a_write_u32(
                        machine, UINT32_C(0x00500024), inner_counter
                    );
                }
            } else if (phase == UINT8_C(13)) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500024), &inner_counter
                );
                if (status == VF2_OK && inner_counter != 0u) {
                    --inner_counter;
                    status = vf2_model2a_write_u32(
                        machine, UINT32_C(0x00500024), inner_counter
                    );
                } else if (status == VF2_OK) {
                    phase = UINT8_C(14);
                    status = vf2_model2a_write(
                        machine, UINT32_C(0x00500030), &phase, sizeof(phase)
                    );
                }
            } else if (phase == UINT8_C(14)) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00500048),
                    &(uint8_t){UINT8_C(17)}, sizeof(uint8_t)
                );
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(
                        machine, UINT32_C(0x00500834), &pointer
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(
                        machine, pointer, &descriptor
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(
                        machine, pointer,
                        descriptor | (UINT32_C(1) << 13u)
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(
                        machine, UINT32_C(0x00500024), 0u
                    );
                }
                if (status == VF2_OK) {
                    ++phase;
                    status = vf2_model2a_write(
                        machine, UINT32_C(0x00500030), &phase, sizeof(phase)
                    );
                }
            } else {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00500048),
                    &(uint8_t){UINT8_C(18)}, sizeof(uint8_t)
                );
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(
                        machine, UINT32_C(0x00508000), &runtime_flags
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(
                        machine, UINT32_C(0x00508000),
                        runtime_flags | (UINT32_C(1) << 9u)
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(
                        machine, UINT32_C(0x00011d94), UINT32_C(29)
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(
                        machine, UINT32_C(0x00f00004), UINT32_C(0x000fffff)
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(
                        machine, UINT32_C(0x00f00008), UINT32_C(0x000fffff)
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(
                        machine, UINT32_C(0x00510000), UINT32_C(0x80000000)
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(
                        machine, UINT32_C(0x00510008), UINT32_C(0x80)
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(
                        machine, UINT32_C(0x0051000c), UINT32_C(0x000439fc)
                    );
                }
                for (registry_index = 0u;
                     status == VF2_OK && registry_index < UINT32_C(29);
                     ++registry_index) {
                    const uint32_t registry_address =
                        UINT32_C(0x00510000) + registry_index * UINT32_C(0x80);
                    status = vf2_model2a_write_u32(
                        machine, registry_address,
                        registry_index == 0u ? UINT32_C(0x80000000) : 0u
                    );
                    if (status == VF2_OK) {
                        status = vf2_model2a_write_u32(
                            machine, registry_address + UINT32_C(8),
                            UINT32_C(0x80)
                        );
                    }
                    if (status == VF2_OK) {
                        status = vf2_model2a_write_u32(
                            machine, registry_address + UINT32_C(0x38), 0u
                        );
                    }
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write(
                        machine, UINT32_C(0x0050002a), &next_selector,
                        sizeof(next_selector)
                    );
                }
                cpu->registers[3] = (uint32_t)next_selector;
            }
            if (status != VF2_OK) {
                return status;
            }
            cpu->registers[3] = phase == UINT8_C(15)
                ? (uint32_t)next_selector : (uint32_t)selector;
            account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
            status = finish_recovered_procedure(machine, cpu, UINT64_C(90));
            if (status != VF2_OK) {
                return status;
            }
            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->changed_values = UINT64_C(8);
            report->bytes_written = 32u;
            report->recovered_instruction_count = UINT64_C(90);
            report->recovered_procedure_calls = UINT64_C(1);
            report->recovered_procedure_returns = UINT64_C(2);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK && phase == UINT8_C(3)) {
            uint32_t inner_counter = 0u;

            /* cfbc[3] -> 0xdb80: advance the inner mode and take the
             * shared f0d8 counter tail. */
            ++phase;
            status = vf2_model2a_write(
                machine, UINT32_C(0x00500030), &phase, sizeof(phase)
            );
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500024), &inner_counter
                );
            }
            if (status == VF2_OK) {
                --inner_counter;
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500024), inner_counter
                );
            }
            if (status != VF2_OK) {
                return status;
            }
            cpu->registers[3] = (uint32_t)selector;
            account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
            status = finish_recovered_procedure(
                machine, cpu, UINT64_C(18)
            );
            if (status != VF2_OK) {
                return status;
            }
            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->changed_values = UINT64_C(4);
            report->bytes_written = 14u;
            report->recovered_instruction_count = UINT64_C(18);
            report->recovered_procedure_calls = UINT64_C(1);
            report->recovered_procedure_returns = UINT64_C(2);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK && phase == UINT8_C(4)) {
            uint32_t inner_counter = 0u;

            /* cfbc[4] -> 0xdb98.  The selector-9 boot path enters the
             * short dd0c arm because d94c leaves 500055 set to one. */
            status = phase16_update_descriptor(
                machine, UINT32_C(0x00500860), 0u,
                UINT32_C(1) << 31u, 0u, 0
            );
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500814), &pointer
                );
            }
            if (status == VF2_OK) {
                status = write_u16(
                    machine, pointer + UINT32_C(0x28), UINT16_C(0)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x0050a014), 0u
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00500048), &one, sizeof(one)
                );
            }
            if (status == VF2_OK) {
                ++phase;
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00500030), &phase, sizeof(phase)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500068), &flags_value
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500068),
                    flags_value & ~(UINT32_C(1) << 0u)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500024), UINT32_C(64)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500834), &pointer
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(machine, pointer, &descriptor);
            }
            if (status == VF2_OK) {
                status = write_u16(
                    machine, pointer + UINT32_C(0x40), profile_halfword
                );
            }
            if (status == VF2_OK) {
                status = write_u16(
                    machine, pointer + UINT32_C(0x42), profile_halfword
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, pointer,
                    descriptor | UINT32_C(0x1a012000)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00500048), &one, sizeof(one)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500814), &pointer
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, pointer + UINT32_C(0x2d4), &zero, sizeof(zero)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500804), &pointer
                );
            }
            if (status == VF2_OK) {
                status = write_u16(
                    machine, pointer + UINT32_C(0x1e20), UINT16_C(0)
                );
            }
            if (status == VF2_OK) {
                status = write_u16(
                    machine, pointer + UINT32_C(0x3e20), UINT16_C(0)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500024), &inner_counter
                );
            }
            if (status == VF2_OK) {
                --inner_counter;
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500024), inner_counter
                );
            }
            if (status != VF2_OK) {
                return status;
            }
            cpu->registers[3] = (uint32_t)selector;
            account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
            status = finish_recovered_procedure(
                machine, cpu, UINT64_C(180)
            );
            if (status != VF2_OK) {
                return status;
            }
            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->changed_values = UINT64_C(20);
            report->bytes_written = 72u;
            report->recovered_instruction_count = UINT64_C(180);
            report->recovered_procedure_calls = UINT64_C(1);
            report->recovered_procedure_returns = UINT64_C(2);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK &&
            (phase == UINT8_C(5) || phase == UINT8_C(6) ||
             phase == UINT8_C(7))) {
            uint32_t inner_counter = 0u;

            /* cfbc[5..7] are the small continuation states at de64,
             * deb0, and df38. They all finish through f0d8; the middle
             * state also primes the command descriptor and a short timer. */
            if (phase == UINT8_C(5)) {
                status = write_u16(
                    machine, UINT32_C(0x00500028), UINT16_C(0)
                );
                if (status == VF2_OK) {
                    status = vf2_model2a_write(
                        machine, UINT32_C(0x00500048), &two, sizeof(two)
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(
                        machine, UINT32_C(0x00500024), &inner_counter
                    );
                }
                if (status == VF2_OK) {
                    inner_counter += UINT32_C(31);
                    status = vf2_model2a_write_u32(
                        machine, UINT32_C(0x00500024), inner_counter
                    );
                }
            } else if (phase == UINT8_C(6)) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00500048), &one, sizeof(one)
                );
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(
                        machine, UINT32_C(0x00500834), &pointer
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(
                        machine, pointer, &descriptor
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(
                        machine, pointer,
                        descriptor & ~(UINT32_C(1) << 29u)
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(
                        machine, UINT32_C(0x00500024), UINT32_C(11)
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(
                        machine, UINT32_C(0x00500814), &pointer
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write(
                        machine, pointer + UINT32_C(0x40),
                        &one, sizeof(one)
                    );
                }
            } else {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500024), &inner_counter
                );
            }
            if (status == VF2_OK) {
                ++phase;
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00500030), &phase, sizeof(phase)
                );
            }
            if (status == VF2_OK) {
                if (phase != UINT8_C(8) || inner_counter != 0u) {
                    status = vf2_model2a_read_u32(
                        machine, UINT32_C(0x00500024), &inner_counter
                    );
                }
            }
            if (status == VF2_OK) {
                --inner_counter;
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500024), inner_counter
                );
            }
            if (status != VF2_OK) {
                return status;
            }
            cpu->registers[3] = (uint32_t)selector;
            account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
            status = finish_recovered_procedure(
                machine, cpu, UINT64_C(50)
            );
            if (status != VF2_OK) {
                return status;
            }
            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->changed_values = UINT64_C(8);
            report->bytes_written = 28u;
            report->recovered_instruction_count = UINT64_C(50);
            report->recovered_procedure_calls = UINT64_C(1);
            report->recovered_procedure_returns = UINT64_C(2);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK && phase == UINT8_C(8)) {
            uint8_t sound_rate = 0u;
            uint32_t phase_descriptor = 0u;

            /* cfbc[8] -> 0xdf60. The ROM commits the next command-buffer
             * mode here, then takes the same f0d8 timer tail. */
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x00500834), &pointer
            );
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, pointer, &descriptor
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, pointer,
                    descriptor & ~(UINT32_C(1) << 25u)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500860), &pointer
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, pointer + UINT32_C(0x80), &phase_descriptor
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, pointer + UINT32_C(0x80),
                    phase_descriptor & ~UINT32_C(3)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500068), &flags_value
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500068),
                    flags_value & ~(UINT32_C(1) << 17u)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read(
                    machine, UINT32_C(0x00500090), &sound_rate,
                    sizeof(sound_rate)
                );
            }
            if (status == VF2_OK) {
                mode_value = (uint32_t)sound_rate << 6u;
                status = write_u16(
                    machine, UINT32_C(0x00500028),
                    (uint16_t)mode_value
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00500048),
                    &(uint8_t){UINT8_C(4)}, sizeof(uint8_t)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x0050081c), &pointer
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, pointer, &descriptor
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, pointer,
                    descriptor | (UINT32_C(1) | (UINT32_C(1) << 1u))
                );
            }
            if (status == VF2_OK) {
                ++phase;
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00500030), &phase, sizeof(phase)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500024), &mode_value
                );
            }
            if (status == VF2_OK) {
                --mode_value;
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500024), mode_value
                );
            }
            if (status != VF2_OK) {
                return status;
            }
            cpu->registers[3] = (uint32_t)selector;
            account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
            status = finish_recovered_procedure(
                machine, cpu, UINT64_C(160)
            );
            if (status != VF2_OK) {
                return status;
            }
            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->changed_values = UINT64_C(14);
            report->bytes_written = 52u;
            report->recovered_instruction_count = UINT64_C(160);
            report->recovered_procedure_calls = UINT64_C(1);
            report->recovered_procedure_returns = UINT64_C(2);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK && phase == UINT8_C(9)) {
            uint32_t task0_descriptor = 0u;
            uint32_t task1_descriptor = 0u;
            uint32_t source_pointer = 0u;
            uint32_t destination_pointer = 0u;
            uint32_t copied_value = 0u;
            uint16_t timer_value = 0u;

            /* cfbc[9] -> 0xe474. This is the short timer/descriptor
             * handoff before the selector-9 task loop continues. */
            status = vf2_model2a_write(
                machine, UINT32_C(0x00500048),
                &(uint8_t){UINT8_C(5)}, sizeof(uint8_t)
            );
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500804), &pointer
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, pointer, &task0_descriptor
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500808), &pointer
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, pointer, &task1_descriptor
                );
            }
            if (status == VF2_OK &&
                (task0_descriptor & (UINT32_C(1) << 5u)) == 0u &&
                (task1_descriptor & (UINT32_C(1) << 5u)) == 0u) {
                status = read_u16(
                    machine, UINT32_C(0x00500028), &timer_value
                );
                mode_value = (uint32_t)timer_value;
                if (status == VF2_OK && mode_value != 0u) {
                    --mode_value;
                    status = write_u16(
                        machine, UINT32_C(0x00500028),
                        (uint16_t)mode_value
                    );
                } else if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(
                        machine, UINT32_C(0x00500068), &flags_value
                    );
                    if (status == VF2_OK) {
                        status = vf2_model2a_write_u32(
                            machine, UINT32_C(0x00500068),
                            flags_value | UINT32_C(1)
                        );
                    }
                }
            }
            if (status == VF2_OK &&
                ((task0_descriptor & (UINT32_C(1) << 5u)) != 0u ||
                 (task1_descriptor & (UINT32_C(1) << 5u)) != 0u ||
                 mode_value == 0u)) {
                ++phase;
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00500030), &phase, sizeof(phase)
                );
            }
            if (status == VF2_OK) {
                status = phase16_update_descriptor(
                    machine, UINT32_C(0x00500844), 0u,
                    UINT32_C(1) << 31u, 0u, 0
                );
            }
            if (status == VF2_OK) {
                status = phase16_update_descriptor(
                    machine, UINT32_C(0x00500848), 0u,
                    UINT32_C(1) << 31u, 0u, 0
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x0050085c), &source_pointer
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, source_pointer + UINT32_C(0x6c), &copied_value
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500860), &destination_pointer
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, destination_pointer + UINT32_C(0x7c),
                    copied_value
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500024), &mode_value
                );
            }
            if (status == VF2_OK) {
                --mode_value;
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500024), mode_value
                );
            }
            if (status != VF2_OK) {
                return status;
            }
            cpu->registers[3] = (uint32_t)selector;
            account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
            status = finish_recovered_procedure(
                machine, cpu, UINT64_C(80)
            );
            if (status != VF2_OK) {
                return status;
            }
            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->changed_values = UINT64_C(12);
            report->bytes_written = 44u;
            report->recovered_instruction_count = UINT64_C(80);
            report->recovered_procedure_calls = UINT64_C(1);
            report->recovered_procedure_returns = UINT64_C(2);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK && phase != UINT8_C(2)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x00500068), &flags_value
            );
        }
        if (status == VF2_OK &&
            (flags_value & (UINT32_C(1) << 5u)) == 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00500068),
                flags_value & ~(UINT32_C(1) << 23u)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, UINT32_C(0x0050004c), &task_mode, sizeof(task_mode)
            );
        }
        if (status == VF2_OK) {
            profile_halfword = task_mode != 0u
                ? UINT16_C(0xa70a) : UINT16_C(0xa708);
            status = write_u16(
                machine, UINT32_C(0x005000a0), profile_halfword
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x00500804), &pointer
            );
        }
        if (status == VF2_OK) {
            status = write_u16(
                machine, pointer + UINT32_C(0x01ac), profile_halfword
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x00500808), &pointer
            );
        }
        if (status == VF2_OK) {
            status = write_u16(
                machine, pointer + UINT32_C(0x01ac), profile_halfword
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x0050081c), UINT32_C(1) << 31u,
                0u, UINT32_C(0x0001b9ac), 1
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x00500828), UINT32_C(1) << 31u,
                0u, UINT32_C(0x000221cc), 1
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x00500850), UINT32_C(1) << 31u,
                0u, UINT32_C(0x00051d3c), 1
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x00500824), UINT32_C(1) << 31u,
                0u, UINT32_C(0x0001645c), 1
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x0050084c), UINT32_C(1) << 31u,
                0u, UINT32_C(0x0002eaa0), 1
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x00500804), &pointer
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, pointer + UINT32_C(0x069c), &zero, sizeof(zero)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, pointer + UINT32_C(0x069d), &one, sizeof(one)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x00500834), &pointer
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, pointer, &descriptor);
        }
        if (status == VF2_OK) {
            descriptor_flags =
                (descriptor & ~(UINT32_C(1) << 30u)) |
                (UINT32_C(1) << 28u);
            status = vf2_model2a_write_u32(
                machine, pointer, descriptor_flags
            );
        }
        if (status == VF2_OK) {
            status = write_u16(
                machine, pointer + UINT32_C(0x40), profile_halfword
            );
        }
        if (status == VF2_OK) {
            status = write_u16(
                machine, pointer + UINT32_C(0x42), profile_halfword
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x00500864),
                (UINT32_C(1) << 30u) | (UINT32_C(1) << 27u) |
                    (UINT32_C(1) << 29u),
                0u, 0u, 0
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00500048), 0u
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, UINT32_C(0x0050004c), &task_mode, sizeof(task_mode)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine,
                task_mode != 0u ? UINT32_C(0x00500059) : UINT32_C(0x0050005a),
                &next_mode, sizeof(next_mode)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x00500052), &next_mode, sizeof(next_mode)
            );
        }
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x00500028), 0u);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x00500055), &one, sizeof(one)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x0050004f), &zero, sizeof(zero)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x00500051), &zero, sizeof(zero)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x00500068), &flags_value
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00500068),
                flags_value & ~((UINT32_C(1) << 1u) | (UINT32_C(1) << 14u))
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, UINT32_C(0x00500030), &phase, sizeof(phase)
            );
        }
        if (status == VF2_OK) {
            ++phase;
            status = vf2_model2a_write(
                machine, UINT32_C(0x00500030), &phase, sizeof(phase)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x00500024), &mode_value
            );
        }
        if (status == VF2_OK) {
            --mode_value;
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00500024), mode_value
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[3] = (uint32_t)selector;
        account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
        status = finish_recovered_procedure(machine, cpu, UINT64_C(420));
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->changed_values = UINT64_C(31);
        report->bytes_written = 104u;
        report->recovered_instruction_count = UINT64_C(420);
        report->recovered_procedure_calls = UINT64_C(1);
        report->recovered_procedure_returns = UINT64_C(2);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
    if (selector == UINT8_C(16)) {
        if (target != UINT32_C(0x00010a0c)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        return execute_frame_phase16(machine, cpu, report);
    }
    if (selector == UINT8_C(17)) {
        if (target != UINT32_C(0x00010b5c)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        return execute_frame_phase17(machine, cpu, report);
    }
    if (target != UINT32_C(0x0000a974)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500024), &counter);
    if (status != VF2_OK) {
        return status;
    }
    --counter;
    status = vf2_model2a_write_u32(machine, UINT32_C(0x00500024), counter);
    if (status != VF2_OK) {
        return status;
    }
    if ((int32_t)counter <= 0) {
        selector = (uint8_t)(selector + UINT8_C(1));
        cpu->registers[3] = (uint32_t)selector;
        status = vf2_model2a_write(machine, UINT32_C(0x0050002a), &selector,
                                   sizeof(selector));
        if (status != VF2_OK) {
            return status;
        }
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
    uint64_t recovered_instruction_count = UINT64_C(0);
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &flags);
    if (status != VF2_OK) {
        return status;
    }

    /* The original bbs at 0x0002453c returns immediately when runtime flag
     * bit 14 is set. This repeated-frame path bypasses the state byte read. */
    if ((flags & (UINT32_C(1) << 14u)) != 0u) {
        recovered_instruction_count = UINT64_C(3);
    } else {
        status = vf2_model2a_read(
            machine, UINT32_C(0x00503001), &state, 1u
        );
        if (status != VF2_OK) {
            return status;
        }
        if (state != 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        recovered_instruction_count = UINT64_C(5);
    }

    status = finish_recovered_procedure(
        machine, cpu, recovered_instruction_count
    );
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_HYBRID_BRIDGE_PLAYER_UPDATE_GATE;
    report->entry_address = VF2_PLAYER_UPDATE_GATE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->recovered_instruction_count = recovered_instruction_count;
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_game_state_update(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    bool leave_return_stub
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
        (runtime_flags & (UINT32_C(1) << 31u)) == 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[15] = selector_mask & UINT32_C(0x00030000);
    cpu->registers[14] = UINT32_C(0x00030000);
    if ((selector_mask & UINT32_C(0x00030000)) != 0u) {
        if (leave_return_stub) {
            cpu->executed_instructions += UINT64_C(7);
            cpu->ip = UINT32_C(0x000020ec);
            status = VF2_OK;
        } else {
            status = finish_recovered_procedure(machine, cpu, UINT64_C(7));
        }
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_GAME_STATE_UPDATE;
        report->entry_address = VF2_GAME_STATE_UPDATE_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->recovered_instruction_count = UINT64_C(7);
        report->recovered_procedure_returns = leave_return_stub ?
            UINT64_C(0) : UINT64_C(1);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
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
        if (leave_return_stub) {
            cpu->executed_instructions += UINT64_C(17);
            cpu->ip = UINT32_C(0x000020ec);
            status = VF2_OK;
        } else {
            status = finish_recovered_procedure(machine, cpu, UINT64_C(17));
        }
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
    cpu->registers[15] = counter32;

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

    if (leave_return_stub) {
        cpu->executed_instructions += UINT64_C(37);
        cpu->ip = UINT32_C(0x000020ec);
        status = VF2_OK;
    } else {
        status = finish_recovered_procedure(machine, cpu, UINT64_C(37));
    }
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
    if (status != VF2_OK) {
        return status;
    }
    if ((frame_counter & UINT32_C(31)) == 0u) {
        /* cmpobe at 0x00010f5c jumps directly to the minimum store and, like
         * the other COBR instructions, does not update the arithmetic
         * condition code. The skipped load leaves r3 at zero. */
        cpu->registers[3] = 0u;
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500160), cpu->registers[5]
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, UINT32_C(0x0050006d), &mode, 1u
            );
        }
        if (status != VF2_OK || mode == UINT8_C(1) || mode == UINT8_C(2)) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        cpu->registers[VF2_I960_G0_REGISTER] = frame_byte;
        (void)timer_low;
        finish_recovered_control_block(
            cpu, UINT32_C(0x00010f90), UINT64_C(21)
        );
        report->kind = VF2_HYBRID_BRIDGE_FRAME_TIMER_PREFIX;
        report->entry_address = VF2_FRAME_TIMER_PREFIX_ENTRY;
        report->exit_address = cpu->ip;
        report->changed_values = UINT64_C(2);
        report->bytes_written = sizeof(uint32_t) * 2u;
        report->recovered_instruction_count = UINT64_C(21);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
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
    if (status != VF2_OK) {
        return status;
    }
    if ((runtime_flags & (UINT32_C(1) << 31u)) == 0u) {
        /* Cold boot takes the ROM's idle branch at 0x0bfc.  It skips the
         * player/video calls and continues at 0x0c80, where the matching
         * runtime-flag test selects the common interrupt restore. */
        cpu->registers[15] = runtime_flags;
        finish_recovered_control_block(cpu, UINT32_C(0x00000c80), UINT64_C(13));
        report->kind = VF2_HYBRID_BRIDGE_INTERRUPT_SAVE_PREFIX;
        report->entry_address = VF2_INTERRUPT_SAVE_PREFIX_ENTRY;
        report->exit_address = cpu->ip;
        report->changed_values = UINT64_C(19);
        report->bytes_written = 16u * sizeof(uint32_t) + sizeof(uint32_t);
        report->recovered_instruction_count = UINT64_C(13);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
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
    if (status != VF2_OK) {
        return status;
    }
    if (first != second) {
        status = vf2_model2a_write(
            machine, pointer + UINT32_C(0x69d), &first, sizeof(first)
        );
        if (status != VF2_OK) {
            return status;
        }
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
    if (status != VF2_OK) {
        return status;
    }
    if (first != second) {
        status = vf2_model2a_write(
            machine, pointer + UINT32_C(0x69d), &first, sizeof(first)
        );
        if (status != VF2_OK) {
            return status;
        }
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
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_i960_cpu candidate_cpu;
    vf2_hybrid_bridge_report player_report = {0};
    vf2_hybrid_bridge_report video_report = {0};
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    vf2_status status = VF2_OK;

    /* Compose nested recovered procedures against a CPU candidate. A rejected
     * branch must not leave a partially entered local frame on the caller. */
    candidate_cpu = *cpu;
    status = vf2_i960_cpu_enter_procedure(
        &candidate_cpu,
        VF2_PLAYER_UPDATE_GATE_ENTRY,
        UINT32_C(0x00000c7c)
    );
    if (status == VF2_OK) {
        status = execute_player_update_gate(
            machine, &candidate_cpu, &player_report
        );
    }
    if (status != VF2_OK || candidate_cpu.ip != UINT32_C(0x00000c7c)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = vf2_i960_cpu_enter_procedure(
        &candidate_cpu,
        VF2_VIDEO_LAYER_COMMIT_ENTRY,
        UINT32_C(0x00000c80)
    );
    if (status == VF2_OK) {
        status = execute_video_layer_commit(
            machine, &candidate_cpu, &video_report
        );
    }
    if (status != VF2_OK || candidate_cpu.ip != UINT32_C(0x00000c80)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    candidate_cpu.executed_instructions += UINT64_C(2);
    *cpu = candidate_cpu;

    report->kind = VF2_HYBRID_BRIDGE_INTERRUPT_PLAYER_LAYER;
    report->entry_address = VF2_INTERRUPT_PLAYER_LAYER_ENTRY;
    report->exit_address = cpu->ip;
    report->bytes_written =
        player_report.bytes_written + video_report.bytes_written;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns =
        cpu->procedure_returns - start_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_interrupt_game_input(
    vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report)
{
    vf2_hybrid_bridge_report nested={0}; uint64_t i=cpu->executed_instructions,c=cpu->procedure_calls,r=cpu->procedure_returns; uint32_t flags=0u;
    vf2_status status=vf2_model2a_read_u32(machine,UINT32_C(0x00500068),&flags);
    if(status!=VF2_OK) return status;
    if((flags&(UINT32_C(1)<<31u))==0u) {
        cpu->registers[15]=flags;
        finish_recovered_control_block(cpu,UINT32_C(0x00000ce0),UINT64_C(2));
        report->kind=VF2_HYBRID_BRIDGE_INTERRUPT_GAME_INPUT; report->entry_address=VF2_INTERRUPT_GAME_INPUT_ENTRY; report->exit_address=cpu->ip; report->recovered_instruction_count=UINT64_C(2); report->cpu_poststate_applied=1; return VF2_OK;
    }
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
    if(status==VF2_OK) status=execute_game_state_update(machine,cpu,&nested,true);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x000020ec)) return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
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
    const uint32_t start_depth=cpu->local_frame_depth;
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
    if(status!=VF2_OK)return status;
    if(cpu->ip!=UINT32_C(0x000000b0) &&
       cpu->ip!=UINT32_C(0x0000a010))return VF2_ERROR_UNSUPPORTED;
    cpu->registers[13]=(start_depth<<8u)|cpu->local_frame_depth;
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
    if (status != VF2_OK) {
        return status;
    }
    if (cpu->ip == UINT32_C(0x00000c0c) ||
        cpu->ip == UINT32_C(0x0004bb14)) {
        cpu->executed_instructions += UINT64_C(3);
        report->kind = VF2_HYBRID_BRIDGE_INTERRUPT_INITIAL_CLUSTER;
        report->entry_address = VF2_INTERRUPT_INITIAL_CLUSTER_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = dispatch_report.iterations;
        report->bytes_written =
            compose_report.bytes_written + latch_report.bytes_written +
            dispatch_report.bytes_written;
        report->recovered_instruction_count =
            cpu->executed_instructions - start_instructions;
        report->recovered_procedure_calls =
            cpu->procedure_calls - start_calls;
        report->recovered_procedure_returns =
            cpu->procedure_returns - start_returns;
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
    if (cpu->ip != VF2_PALETTE_PAGE_UPLOAD_ENTRY) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = execute_palette_page_upload(machine, cpu, &upload_report);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0004bab4)) {
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
