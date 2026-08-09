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
            sample_sum += (uint32_t)(int32_t)(int16_t)sample;
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

static vf2_status clear_tile_plane_64x48(
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
    if (status != VF2_OK || phase_state == 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
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
    vf2_model2a *machine,
    uint32_t source_base,
    uint32_t destination_base
)
{
    int16_t addend = 0;
    int16_t word_mode = 0;
    uint16_t raw_addend = 0u;
    uint16_t raw_word_mode = 0u;
    uint32_t columns = 0u;
    uint32_t rows = 0u;
    uint32_t row = 0u;
    uint32_t column = 0u;
    vf2_status status = VF2_OK;

    status = read_u16(machine, source_base, &raw_addend);
    addend = (int16_t)raw_addend;
    if (status == VF2_OK) {
        status = read_u16(machine, source_base + UINT32_C(2),
                          &raw_word_mode);
        word_mode = (int16_t)raw_word_mode;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, source_base + UINT32_C(4),
                                      &columns);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, source_base + UINT32_C(8),
                                      &rows);
    }
    if (status != VF2_OK || columns > UINT32_C(0x1000) ||
        rows > UINT32_C(0x1000)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    for (row = 0u; status == VF2_OK && row < rows; ++row) {
        uint32_t source = source_base + UINT32_C(12);
        uint32_t destination = destination_base + row * UINT32_C(0x100);

        for (column = 0u; status == VF2_OK && column < columns; ++column) {
            uint16_t sample = 0u;
            uint16_t output = 0u;

            if (word_mode == 0) {
                uint8_t byte = 0u;

                status = vf2_model2a_read(machine, source, &byte, sizeof(byte));
                if (status != VF2_OK) {
                    break;
                }
                output = (uint16_t)((int32_t)addend + (int32_t)byte);
                source += UINT32_C(1);
            } else {
                status = read_u16(machine, source, &sample);
                if (status != VF2_OK) {
                    break;
                }
                output = (uint16_t)((int32_t)addend +
                                    (int32_t)(int16_t)sample);
                source += UINT32_C(2);
            }
            status = write_u16(machine, destination, output);
            destination += UINT32_C(2);
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
    if (status == VF2_OK && (flags & (UINT32_C(1) << 1u)) != 0u) {
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
            machine,
            base + UINT32_C(0x3325) +
                (profile_class == 0u ? UINT32_C(2) : UINT32_C(0)),
            &byte_value, sizeof(byte_value)
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
                machine,
                base + UINT32_C(0x3327) +
                    (profile_class == 0u ? UINT32_C(1) : UINT32_C(0)),
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
        status = clear_tile_plane_64x48(
            machine, UINT32_C(0x01000000)
        );
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
    if (color0 == 0u || color1 == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (profile_class == UINT32_C(3)) {
        *x_result = 0u;
        *y_result = 0u;
    } else if (profile_class == UINT32_C(2) &&
               (flags & (UINT32_C(1) << 1u)) == 0u) {
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
    uint8_t *next_phase
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

    if (cpu == NULL || next_phase == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *next_phase = previous_phase;
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
        status = execute_selector3_profile_measure(
            machine, base, &x, &y
        );
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[0] = x;
        cpu->registers[1] = y;
        if ((profile_flags & UINT32_C(1)) != 0u ||
            (runtime_flags & (UINT32_C(1) << 1u)) == 0u) {
            sum = x + y;
        } else {
            sum = x;
        }
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
        *next_phase = (uint8_t)(previous_phase + UINT8_C(1));
        status = vf2_model2a_write(
            machine, UINT32_C(0x00500030), next_phase, sizeof(*next_phase)
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
    /* The ROM emits 0xad1001 when the countdown reaches 192. The sound
     * queue is deliberately left to the existing audio boundary; this worker
     * still preserves the selector-visible timer and phase transitions. */
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
        status = execute_selector3_display_text(
            machine, UINT32_C(0x02a6d8aa)
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
    vf2_status status = VF2_OK;

    if (next_phase == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *next_phase = previous_phase;
    status = vf2_model2a_read_u32(
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
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x0050a00c), source + UINT32_C(4)
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
        uint32_t phase_target = 0u;
        int fallback = 0;

        if (target != UINT32_C(0x0000acf8)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = vf2_model2a_read(machine, UINT32_C(0x00500030), &phase,
                                  sizeof(phase));
        if (status == VF2_OK) {
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
            (phase == UINT8_C(9) &&
             phase_target != UINT32_C(0x0000baec)) ||
            (phase == UINT8_C(10) &&
             phase_target != UINT32_C(0x0000bc10)) ||
            (phase == UINT8_C(11) &&
             phase_target != UINT32_C(0x0000bcb0)) ||
            (phase == UINT8_C(12) &&
             phase_target != UINT32_C(0x0000bce4)) ||
            (phase != UINT8_C(0) && phase != UINT8_C(1) &&
             phase != UINT8_C(2) && phase != UINT8_C(3) &&
             phase != UINT8_C(4) && phase != UINT8_C(5) &&
             phase != UINT8_C(6) && phase != UINT8_C(7) &&
             phase != UINT8_C(9) && phase != UINT8_C(10) &&
             phase != UINT8_C(11) && phase != UINT8_C(12))) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        if (phase == UINT8_C(0)) {
            status = execute_selector3_mode0_special(
                machine, phase, &phase, &fallback
            );
        } else if (phase == UINT8_C(1)) {
            status = execute_selector3_phase1(
                machine, cpu, phase, &phase
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
        } else {
            status = execute_frame_selector3_b0d8(
                machine, phase, &phase, 1
            );
        }
        if (status == VF2_OK && fallback) {
            status = execute_frame_selector3_b0d8(
                machine, phase, &phase, 1
            );
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
        (runtime_flags & (UINT32_C(1) << 31u)) == 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[15] = selector_mask & UINT32_C(0x00030000);
    cpu->registers[14] = UINT32_C(0x00030000);
    if ((selector_mask & UINT32_C(0x00030000)) != 0u) {
        status = finish_recovered_procedure(machine, cpu, UINT64_C(7));
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_GAME_STATE_UPDATE;
        report->entry_address = VF2_GAME_STATE_UPDATE_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->recovered_instruction_count = UINT64_C(7);
        report->recovered_procedure_returns = UINT64_C(1);
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
    const uint32_t start_depth=cpu->local_frame_depth;
    uint8_t debug_selector=0u;
    (void)vf2_model2a_read(machine,UINT32_C(0x0050002a),&debug_selector,sizeof(debug_selector));
    cpu->registers[15]=(uint32_t)debug_selector;
    cpu->registers[14]=UINT32_C(0x9ff801);
    vf2_status status=vf2_i960_cpu_enter_procedure(cpu,VF2_FRAME_SHADOW_VERIFY_ENTRY,UINT32_C(0x00009ffc));
    if(status==VF2_OK)status=execute_frame_shadow_verify(machine,cpu,&a);
    cpu->registers[14]=UINT32_C(0x9ff802);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x00009ffc))return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    status=vf2_i960_cpu_enter_procedure(cpu,UINT32_C(0x00029744),UINT32_C(0x0000a000));
    if(status==VF2_OK)status=vf2_i960_cpu_return_procedure(cpu,machine);
    cpu->registers[14]=UINT32_C(0x9ff803);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x0000a000))return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    status=vf2_i960_cpu_enter_procedure(cpu,VF2_FRAME_BUFFER_GATE_ENTRY,UINT32_C(0x0000a004)); if(status==VF2_OK)status=execute_frame_buffer_gate(machine,cpu,&b);
    cpu->registers[14]=UINT32_C(0x9ff804);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x0000a004))return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    status=vf2_i960_cpu_enter_procedure(cpu,VF2_GEOMETRY_COMMAND_SETUP_ENTRY,UINT32_C(0x0000a008)); if(status==VF2_OK)status=execute_geometry_command_setup(machine,cpu,&d);
    cpu->registers[14]=UINT32_C(0x9ff805);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x0000a008))return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    status=vf2_i960_cpu_enter_procedure(cpu,VF2_FRAME_SCRATCH_CLEAR_ENTRY,UINT32_C(0x0000a00c)); if(status==VF2_OK)status=execute_frame_scratch_clear(machine,cpu,&e);
    cpu->registers[14]=UINT32_C(0x9ff806);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x0000a00c))return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    status=vf2_i960_cpu_enter_procedure(cpu,VF2_FRAME_DISPATCH_TICK_ENTRY,UINT32_C(0x0000a010)); if(status==VF2_OK)status=execute_frame_dispatch_tick(machine,cpu,&f);
    cpu->registers[14]=UINT32_C(0x9ff807);
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
    if (cpu->ip == UINT32_C(0x00000c0c)) {
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
