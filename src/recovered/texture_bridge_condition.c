#define vf2_hybrid_bridge_apply_condition_poststate \
    vf2_hybrid_bridge_apply_condition_poststate_core
#define vf2_hybrid_post_frame_bridge_execute \
    vf2_hybrid_post_frame_bridge_execute_condition_core
#include "texture_bridge_condition_impl.c"
#undef vf2_hybrid_post_frame_bridge_execute
#undef vf2_hybrid_bridge_apply_condition_poststate

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

static vf2_status selector3_render_display(vf2_model2a *machine)
{
    uint16_t raw_addend = 0u;
    uint16_t raw_mode = 0u;
    uint32_t rows = 0u;
    uint32_t columns = 0u;
    uint32_t source = VF2_SELECTOR3_TILE_SOURCE + UINT32_C(12);
    uint32_t row = 0u;
    vf2_status status = selector3_read_u16(
        machine, VF2_SELECTOR3_TILE_SOURCE, &raw_addend
    );

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
    if (status != VF2_OK || (int16_t)raw_mode != 0 ||
        rows == 0u || rows > UINT32_C(128) ||
        columns == 0u || columns > UINT32_C(64)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    for (row = 0u; row < rows; ++row) {
        uint32_t column = 0u;
        uint32_t destination =
            VF2_SELECTOR3_TILE_DESTINATION + row * UINT32_C(0x80);

        for (column = 0u; column < columns; ++column) {
            uint8_t raw = 0u;
            int32_t value = 0;

            status = vf2_model2a_read(machine, source, &raw, sizeof(raw));
            if (status != VF2_OK) {
                return status;
            }
            ++source;
            value = (int32_t)(int16_t)raw_addend + (int32_t)(int8_t)raw;
            status = selector3_write_u16(
                machine, destination, (uint16_t)value
            );
            if (status != VF2_OK) {
                return status;
            }
            destination += UINT32_C(2);
        }
    }
    return VF2_OK;
}

static vf2_status selector3_apply_mode3_profile(vf2_model2a *machine)
{
    const uint32_t table = UINT32_C(0x0006ee00) + UINT32_C(3) * UINT32_C(0x100);
    uint32_t value32 = 0u;
    uint16_t value16 = 0u;
    uint8_t value8 = 0u;
    uint32_t index = 0u;
    vf2_status status = vf2_model2a_write_u32(
        machine, UINT32_C(0x0050a000), UINT32_C(0x3b32674f)
    );

    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x0050a004), UINT32_C(0x3f800000)
        );
    }
    for (index = 0u; status == VF2_OK && index < UINT32_C(26); ++index) {
        status = vf2_model2a_write_u32(
            machine,
            UINT32_C(0x0050a0e0) + index * UINT32_C(4),
            UINT32_C(0x3f800000)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00501018), UINT32_C(0x00001388)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, table + UINT32_C(0xae), &value8, sizeof(value8)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x00500170), &value8, sizeof(value8)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, table + UINT32_C(0xa4), &value32);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00501098), value32);
    }
    if (status == VF2_OK) {
        status = selector3_read_u16(
            machine, table + UINT32_C(0xa8), &value16
        );
    }
    if (status == VF2_OK) {
        status = selector3_write_u16(
            machine, UINT32_C(0x00501020), value16
        );
    }
    if (status == VF2_OK) {
        status = selector3_read_u16(
            machine, table + UINT32_C(0xaa), &value16
        );
    }
    if (status == VF2_OK) {
        status = selector3_write_u16(
            machine, UINT32_C(0x00501022), value16
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, table + UINT32_C(0xb8), &value8, sizeof(value8)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x005000e0), &value8, sizeof(value8)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, table + UINT32_C(0xb9), &value8, sizeof(value8)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x005000e1), &value8, sizeof(value8)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, table + UINT32_C(0xba), &value8, sizeof(value8)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x005000e2), &value8, sizeof(value8)
        );
    }

    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00546000), 1u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00546004), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00550000), 1u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x005502e0), 3u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x005502e4), UINT32_C(0x21)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x005502e8), UINT32_C(0x0a)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x005502ec), UINT32_C(0x7c)
        );
    }
    return status;
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
        status = selector3_render_display(machine);
    }
    if (status == VF2_OK) {
        status = selector3_apply_mode3_profile(machine);
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

    cpu->executed_instructions += VF2_SELECTOR3_INSTRUCTION_DELTA;
    cpu->procedure_calls += VF2_SELECTOR3_CALL_DELTA;
    cpu->procedure_returns += VF2_SELECTOR3_RETURN_DELTA;
    report->recovered_instruction_count += VF2_SELECTOR3_INSTRUCTION_DELTA;
    report->recovered_procedure_calls += VF2_SELECTOR3_CALL_DELTA;
    report->recovered_procedure_returns += VF2_SELECTOR3_RETURN_DELTA;
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
        status = apply_selector3_phase0_bridge(machine, cpu, entry, report);
    }
    if (status == VF2_OK) {
        status = apply_selector2_queue_condition(machine, cpu, entry);
    }
    return status;
}
