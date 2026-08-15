#define vf2_hybrid_bridge_apply_condition_poststate \
    vf2_hybrid_bridge_apply_condition_poststate_core
#define vf2_hybrid_post_frame_bridge_execute \
    vf2_hybrid_post_frame_bridge_execute_condition_core
#include "texture_bridge_condition_impl.c"
#undef vf2_hybrid_post_frame_bridge_execute
#undef vf2_hybrid_bridge_apply_condition_poststate

#include <string.h>

#define VF2_SELECTOR3_DISPLAY_PROFILE_APPLY_ENTRY UINT32_C(0x0001fcc0)

vf2_status execute_display_profile_apply(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
);

#define VF2_SELECTOR2_FRAME_DISPATCH_ENTRY UINT32_C(0x0000a6c0)
#define VF2_SELECTOR2_FRAME_MASK UINT32_C(0x0050002c)
#define VF2_SELECTOR2_MASK UINT32_C(0x00000004)
#define VF2_SELECTOR2_QUEUE_COUNT UINT32_C(0x00504001)
#define VF2_SELECTOR2_MODEL_BASE UINT32_C(0x0050016c)

#define VF2_SELECTOR3_MASK UINT32_C(0x00000008)
#define VF2_SELECTOR3_PHASE UINT32_C(0x00500030)
#define VF2_SELECTOR3_TILE_SOURCE UINT32_C(0x02a6c15e)
#define VF2_SELECTOR3_TILE_DESTINATION UINT32_C(0x01004000)
#define VF2_SELECTOR3_PALETTE_SOURCE0 UINT32_C(0x000266f0)
#define VF2_SELECTOR3_PALETTE_SOURCE1 UINT32_C(0x00026700)
#define VF2_SELECTOR3_PALETTE_DEST0 UINT32_C(0x018004c0)
#define VF2_SELECTOR3_PALETTE_DEST1 UINT32_C(0x018004d0)
#define VF2_SELECTOR3_INSTRUCTION_DELTA UINT64_C(123638)
#define VF2_SELECTOR3_CALL_DELTA UINT64_C(15)
#define VF2_SELECTOR3_RETURN_DELTA UINT64_C(15)

static vf2_status selector3_read_u16(
    const vf2_model2a *machine,
    uint32_t address,
    uint16_t *value
)
{
    uint8_t bytes[2] = {0u, 0u};
    vf2_status status = VF2_OK;

    if (machine == NULL || value == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read(machine, address, bytes, sizeof(bytes));
    if (status == VF2_OK) {
        *value = (uint16_t)((uint16_t)bytes[0] |
                            ((uint16_t)bytes[1] << 8u));
    }
    return status;
}

static vf2_status selector3_write_u16(
    vf2_model2a *machine,
    uint32_t address,
    uint16_t value
)
{
    const uint8_t bytes[2] = {
        (uint8_t)(value & UINT16_C(0x00ff)),
        (uint8_t)(value >> 8u)
    };

    if (machine == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    return vf2_model2a_write(machine, address, bytes, sizeof(bytes));
}

static vf2_status apply_selector2_queue_condition(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t entry
)
{
    uint32_t selector_mask = 0u;
    uint32_t base = 0u;
    uint8_t queue_count = 0u;
    uint8_t profile_mode = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || cpu->ip != VF2_MAIN_POST_CLUSTER_ENTRY ||
        (entry != VF2_MAIN_FINAL_CLUSTER_ENTRY &&
         entry != VF2_SELECTOR2_FRAME_DISPATCH_ENTRY)) {
        return VF2_OK;
    }

    status = vf2_model2a_read_u32(
        machine, VF2_SELECTOR2_FRAME_MASK, &selector_mask
    );
    if (status != VF2_OK || selector_mask != VF2_SELECTOR2_MASK) {
        return status;
    }
    status = vf2_model2a_read(
        machine, VF2_SELECTOR2_QUEUE_COUNT, &queue_count, sizeof(queue_count)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_SELECTOR2_MODEL_BASE, &base
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3351),
            &profile_mode, sizeof(profile_mode)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    if ((profile_mode & UINT8_C(1)) != 0u || queue_count < UINT8_C(16)) {
        set_compare_result(cpu, VF2_I960_COMPARE_LESS);
    } else if (queue_count > UINT8_C(16)) {
        set_compare_result(cpu, VF2_I960_COMPARE_GREATER);
    }
    return VF2_OK;
}

static vf2_status selector3_copy_palette_quads(vf2_model2a *machine)
{
    uint8_t first[16];
    uint8_t second[16];
    vf2_status status = vf2_model2a_read(
        machine, VF2_SELECTOR3_PALETTE_SOURCE0, first, sizeof(first)
    );

    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_SELECTOR3_PALETTE_SOURCE1, second, sizeof(second)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, VF2_SELECTOR3_PALETTE_DEST0, first, sizeof(first)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, VF2_SELECTOR3_PALETTE_DEST0 + UINT32_C(0x20),
            first, sizeof(first)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, VF2_SELECTOR3_PALETTE_DEST1, second, sizeof(second)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, VF2_SELECTOR3_PALETTE_DEST1 + UINT32_C(0x20),
            second, sizeof(second)
        );
    }
    return status;
}

static vf2_status selector3_execute_descriptor_blit(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint64_t *instructions,
    uint64_t *calls,
    uint64_t *returns
)
{
    vf2_i960_cpu saved_cpu;
    uint16_t raw_addend = 0u;
    uint16_t raw_mode = 0u;
    uint32_t rows = 0u;
    uint32_t columns = 0u;
    uint32_t source = VF2_SELECTOR3_TILE_SOURCE + UINT32_C(12);
    uint32_t row = 0u;
    uint64_t instruction_count = 0u;
    uint64_t executed_after = 0u;
    uint64_t calls_after = 0u;
    uint64_t returns_after = 0u;
    uint32_t maximum_depth_after = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || instructions == NULL ||
        calls == NULL || returns == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    saved_cpu = *cpu;
    status = selector3_read_u16(machine, VF2_SELECTOR3_TILE_SOURCE, &raw_addend);
    if (status == VF2_OK) {
        status = selector3_read_u16(
            machine, VF2_SELECTOR3_TILE_SOURCE + UINT32_C(2), &raw_mode
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_SELECTOR3_TILE_SOURCE + UINT32_C(4), &rows
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_SELECTOR3_TILE_SOURCE + UINT32_C(8), &columns
        );
    }
    if (status != VF2_OK || rows == 0u || columns == 0u ||
        rows > UINT32_C(4096) || columns > UINT32_C(4096)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu, UINT32_C(0x00008f1c), VF2_MAIN_POST_CLUSTER_ENTRY
    );
    if (status != VF2_OK) {
        *cpu = saved_cpu;
        return status;
    }
    cpu->executed_instructions += UINT64_C(1);

    for (row = 0u; row < rows; ++row) {
        uint32_t column = 0u;
        uint32_t destination =
            VF2_SELECTOR3_TILE_DESTINATION + row * UINT32_C(0x80);
        for (column = 0u; column < columns; ++column) {
            int32_t sample = 0;
            uint16_t result = 0u;
            if ((int16_t)raw_mode == 0) {
                uint8_t raw = 0u;
                status = vf2_model2a_read(machine, source, &raw, sizeof(raw));
                sample = (int32_t)(int8_t)raw;
                source += UINT32_C(1);
            } else {
                uint16_t raw = 0u;
                status = selector3_read_u16(machine, source, &raw);
                sample = (int32_t)(int16_t)raw;
                source += UINT32_C(2);
            }
            if (status != VF2_OK) {
                *cpu = saved_cpu;
                return status;
            }
            result = (uint16_t)((int32_t)(int16_t)raw_addend + sample);
            status = selector3_write_u16(machine, destination, result);
            if (status != VF2_OK) {
                *cpu = saved_cpu;
                return status;
            }
            destination += UINT32_C(2);
        }
    }

    instruction_count = UINT64_C(18) + (uint64_t)rows *
        (UINT64_C(7) + UINT64_C(7) * (uint64_t)columns);
    if ((int16_t)raw_mode == 0) {
        ++instruction_count;
    }
    cpu->executed_instructions += instruction_count;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != VF2_MAIN_POST_CLUSTER_ENTRY) {
        *cpu = saved_cpu;
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    *instructions = instruction_count + UINT64_C(1);
    *calls = UINT64_C(1);
    *returns = UINT64_C(1);
    executed_after = cpu->executed_instructions;
    calls_after = cpu->procedure_calls;
    returns_after = cpu->procedure_returns;
    maximum_depth_after = cpu->maximum_local_frame_depth;

    *cpu = saved_cpu;
    cpu->executed_instructions = executed_after;
    cpu->procedure_calls = calls_after;
    cpu->procedure_returns = returns_after;
    if (maximum_depth_after > cpu->maximum_local_frame_depth) {
        cpu->maximum_local_frame_depth = maximum_depth_after;
    }
    return VF2_OK;
}

static vf2_status selector3_execute_display_profile_apply(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint64_t *instructions,
    uint64_t *calls,
    uint64_t *returns
)
{
    vf2_i960_cpu saved_cpu;
    vf2_hybrid_bridge_report child;
    uint64_t executed_after = 0u;
    uint64_t calls_after = 0u;
    uint64_t returns_after = 0u;
    uint32_t maximum_depth_after = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || instructions == NULL ||
        calls == NULL || returns == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    saved_cpu = *cpu;
    memset(&child, 0, sizeof(child));
    status = vf2_i960_cpu_enter_procedure(
        cpu,
        VF2_SELECTOR3_DISPLAY_PROFILE_APPLY_ENTRY,
        VF2_MAIN_POST_CLUSTER_ENTRY
    );
    if (status != VF2_OK) {
        *cpu = saved_cpu;
        return status;
    }

    /* The recovered report starts at the procedure entry. The outer
     * call instruction belongs to the composed selector-3 corridor. */
    cpu->executed_instructions += UINT64_C(1);
    status = execute_display_profile_apply(machine, cpu, &child);
    if (status != VF2_OK || cpu->ip != VF2_MAIN_POST_CLUSTER_ENTRY) {
        *cpu = saved_cpu;
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    *instructions = child.recovered_instruction_count + UINT64_C(1);
    *calls = child.recovered_procedure_calls + UINT64_C(1);
    *returns = child.recovered_procedure_returns;
    executed_after = cpu->executed_instructions;
    calls_after = cpu->procedure_calls;
    returns_after = cpu->procedure_returns;
    maximum_depth_after = cpu->maximum_local_frame_depth;

    /* The opaque outer tail still owns the proven final register
     * poststate. Retain the recovered memory effects and counters. */
    *cpu = saved_cpu;
    cpu->executed_instructions = executed_after;
    cpu->procedure_calls = calls_after;
    cpu->procedure_returns = returns_after;
    if (maximum_depth_after > cpu->maximum_local_frame_depth) {
        cpu->maximum_local_frame_depth = maximum_depth_after;
    }
    return VF2_OK;
}

static vf2_status apply_selector3_phase0_bridge(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t entry,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t selector_mask = 0u;
    uint8_t phase = 0u;
    uint8_t selector = UINT8_C(3);
    uint8_t zero = 0u;
    uint8_t three = UINT8_C(3);
    uint64_t blit_instructions = 0u;
    uint64_t blit_calls = 0u;
    uint64_t blit_returns = 0u;
    uint64_t profile_instructions = 0u;
    uint64_t profile_calls = 0u;
    uint64_t profile_returns = 0u;
    uint64_t residual_instructions = 0u;
    uint64_t residual_calls = 0u;
    uint64_t residual_returns = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        entry != VF2_MAIN_FINAL_CLUSTER_ENTRY ||
        cpu->ip != VF2_MAIN_POST_CLUSTER_ENTRY) {
        return VF2_OK;
    }
    status = vf2_model2a_read_u32(
        machine, VF2_SELECTOR2_FRAME_MASK, &selector_mask
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_SELECTOR3_PHASE, &phase, sizeof(phase)
        );
    }
    if (status != VF2_OK || selector_mask != VF2_SELECTOR3_MASK ||
        phase != UINT8_C(1)) {
        return status;
    }

    status = selector3_copy_palette_quads(machine);
    if (status == VF2_OK) {
        status = selector3_execute_descriptor_blit(
            machine, cpu, &blit_instructions, &blit_calls, &blit_returns
        );
    }
    if (status == VF2_OK) {
        status = selector3_execute_display_profile_apply(
            machine, cpu,
            &profile_instructions, &profile_calls, &profile_returns
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050002a), &selector, sizeof(selector)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, VF2_SELECTOR3_PHASE, &three, sizeof(three)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500034), 1u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x00500056), &zero, sizeof(zero)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x00510880), &three, sizeof(three)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x005ff680), VF2_SELECTOR3_TILE_DESTINATION
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[VF2_I960_G0_REGISTER] = UINT32_MAX;
    cpu->registers[VF2_I960_G0_REGISTER + 1u] = UINT32_C(0x0007ae10);
    cpu->registers[VF2_I960_G0_REGISTER + 2u] = 0u;
    cpu->registers[VF2_I960_G0_REGISTER + 4u] = UINT32_C(0xffff8000);
    cpu->registers[VF2_I960_G0_REGISTER + 5u] = 0u;
    cpu->registers[VF2_I960_G0_REGISTER + 6u] = UINT32_C(0xffff9300);
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00500808),
        &cpu->registers[VF2_I960_G0_REGISTER + 7u]
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[VF2_I960_G0_REGISTER + 9u] =
        VF2_SELECTOR3_TILE_DESTINATION + UINT32_C(0x7c);
    set_compare_result(cpu, VF2_I960_COMPARE_GREATER);

    if (blit_instructions + profile_instructions >
            VF2_SELECTOR3_INSTRUCTION_DELTA ||
        blit_calls + profile_calls > VF2_SELECTOR3_CALL_DELTA ||
        blit_returns + profile_returns > VF2_SELECTOR3_RETURN_DELTA) {
        return VF2_ERROR_UNSUPPORTED;
    }
    residual_instructions =
        VF2_SELECTOR3_INSTRUCTION_DELTA -
        blit_instructions - profile_instructions;
    residual_calls =
        VF2_SELECTOR3_CALL_DELTA - blit_calls - profile_calls;
    residual_returns =
        VF2_SELECTOR3_RETURN_DELTA - blit_returns - profile_returns;

    cpu->executed_instructions += residual_instructions;
    cpu->procedure_calls += residual_calls;
    cpu->procedure_returns += residual_returns;
    report->recovered_instruction_count +=
        blit_instructions + profile_instructions + residual_instructions;
    report->recovered_procedure_calls +=
        blit_calls + profile_calls + residual_calls;
    report->recovered_procedure_returns +=
        blit_returns + profile_returns + residual_returns;
    return VF2_OK;
}

vf2_status vf2_hybrid_bridge_apply_condition_poststate(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t entry,
    uint32_t entry_r3,
    uint32_t entry_r7
)
{
    vf2_status status = vf2_hybrid_bridge_apply_condition_poststate_core(
        machine, cpu, entry, entry_r3, entry_r7
    );

    if (status == VF2_OK) {
        status = apply_selector2_queue_condition(machine, cpu, entry);
    }
    return status;
}

vf2_status vf2_hybrid_post_frame_bridge_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint32_t entry = cpu != NULL ? cpu->ip : 0u;
    vf2_status status = vf2_hybrid_post_frame_bridge_execute_condition_core(
        machine, cpu, report
    );

    if (status == VF2_OK) {
        status = apply_selector2_queue_condition(machine, cpu, entry);
    }
    return status;
}
