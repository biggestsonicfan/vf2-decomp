#include "texture_bridge_internal.h"


vf2_status execute_video_register_compose(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t old_flags = 0u;
    uint32_t old_video = 0u;
    uint32_t runtime_flags = 0u;
    uint32_t callback = 0u;
    uint32_t packed_mask = 0u;
    uint32_t control = 0u;
    uint32_t inverted = 0u;
    uint32_t newly_enabled = 0u;
    uint32_t index = 0u;
    uint64_t instructions = UINT64_C(63);
    uint8_t mode = 0u;
    uint8_t raw = 0u;
    uint8_t mapped = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = write_u16(machine, UINT32_C(0x01c00040), UINT16_C(1));
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500700), &old_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0050070c), old_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x00500f00), &mode, 1u);
    }
    if (status != VF2_OK || mode == UINT8_C(1)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    for (index = 0u; index < UINT32_C(3); ++index) {
        const uint32_t source = UINT32_C(0x01c00016) + index * UINT32_C(2);
        status = vf2_model2a_read(machine, source, &raw, 1u);
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, UINT32_C(0x00050628) + (uint32_t)raw, &mapped, 1u
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        packed_mask |= (uint32_t)mapped << (index * UINT32_C(8));
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00508000), &runtime_flags);
    if (status != VF2_OK || (runtime_flags & (UINT32_C(1) << 14u)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    packed_mask = (~packed_mask) & UINT32_C(8);
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500710), &old_video);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500714), old_video);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500710), packed_mask);
    }
    if (status != VF2_OK) {
        return status;
    }

    for (index = 0u; index < UINT32_C(3); ++index) {
        const uint32_t source = UINT32_C(0x01c00010) + index * UINT32_C(2);
        status = vf2_model2a_read(machine, source, &raw, 1u);
        if (status != VF2_OK) {
            return status;
        }
        control |= (uint32_t)raw << (index * UINT32_C(8));
    }
    status = vf2_model2a_read(machine, UINT32_C(0x01c0001c), &raw, 1u);
    if (status != VF2_OK) {
        return status;
    }
    control |= ((uint32_t)(raw & UINT8_C(15)) | UINT32_C(0xf0)) << 24u;
    if ((control & (UINT32_C(1) << 11u)) == 0u) {
        control &= ~(UINT32_C(1) << 10u);
        control |= UINT32_C(1) << 11u;
    }
    if ((control & (UINT32_C(1) << 19u)) == 0u) {
        control &= ~(UINT32_C(1) << 18u);
        control |= UINT32_C(1) << 19u;
    }
    inverted = ~control;
    newly_enabled = inverted & ~old_flags;
    status = vf2_model2a_write_u32(machine, UINT32_C(0x00500700), inverted);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500704), newly_enabled
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500708), old_flags & control
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x005001dc), &callback);
    }
    if (status != VF2_OK || (inverted & UINT32_C(8)) == 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    if (callback == 0u) {
        callback = UINT32_C(0x00001284);
    } else if ((newly_enabled & UINT32_C(0x00f7f700)) == 0u) {
        /* The helper at 0x00001200 keeps an installed callback when no
         * relevant video bits changed. Its non-zero callback path executes
         * two more instructions than the initial installation path. */
        instructions += UINT64_C(2);
    } else {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_write_u32(
        machine, UINT32_C(0x005001dc), callback
    );
    if (status != VF2_OK) {
        return status;
    }

    account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
    status = finish_recovered_procedure(machine, cpu, instructions);
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_HYBRID_BRIDGE_VIDEO_REGISTER_COMPOSE;
    report->entry_address = VF2_VIDEO_REGISTER_COMPOSE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(7);
    report->bytes_written = 30u;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_calls = UINT64_C(1);
    report->recovered_procedure_returns = UINT64_C(2);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_video_input_latch_write(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint8_t value = 0u;
    uint8_t mode = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read(machine, UINT32_C(0x00500718), &value, 1u);
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x00500f00), &mode, 1u);
    }
    if (status != VF2_OK || mode == UINT8_C(1)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_model2a_write(machine, UINT32_C(0x01c0001e), &value, 1u);
    if (status != VF2_OK) {
        return status;
    }
    status = finish_recovered_procedure(machine, cpu, UINT64_C(7));
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_HYBRID_BRIDGE_VIDEO_INPUT_LATCH_WRITE;
    report->entry_address = VF2_VIDEO_INPUT_LATCH_WRITE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(1);
    report->bytes_written = 1u;
    report->recovered_instruction_count = UINT64_C(7);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_video_layer_commit(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t source = 0u;
    uint32_t lookup = 0u;
    uint32_t layer_flags = 0u;
    uint16_t palette_value0 = 0u;
    uint16_t palette_value1 = 0u;
    uint16_t source_value0 = 0u;
    uint16_t source_value1 = 0u;
    uint16_t mode16 = 0u;
    uint8_t index = 0u;
    uint8_t frame_mode = 0u;
    uint8_t auxiliary = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    /* Read and validate the complete observed input state before the first
     * write. Unsupported variants therefore leave video/palette memory intact. */
    status = read_u16(
        machine, UINT32_C(0x00503036), &palette_value0
    );
    if (status == VF2_OK) {
        status = read_u16(
            machine, UINT32_C(0x00503038), &palette_value1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500834), &source
        );
    }
    if (status == VF2_OK) {
        status = read_u16(
            machine, source + UINT32_C(0x44), &source_value0
        );
    }
    if (status == VF2_OK) {
        status = read_u16(
            machine, source + UINT32_C(0x46), &source_value1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x0050d004), &index, 1u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            UINT32_C(0x00026690) + (uint32_t)index * UINT32_C(4),
            &lookup
        );
    }
    if (status == VF2_OK) {
        status = read_u16(machine, UINT32_C(0x00503054), &mode16);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050304c), &layer_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x0050002b), &frame_mode, 1u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x00500031), &auxiliary, 1u
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if ((mode16 & UINT16_C(3)) != 0u ||
        (layer_flags & UINT32_C(15)) != 0u ||
        frame_mode == UINT8_C(3)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = write_u16(
        machine, UINT32_C(0x018000bc), palette_value0
    );
    if (status == VF2_OK) {
        status = write_u16(
            machine, UINT32_C(0x018000be), palette_value1
        );
    }
    if (status == VF2_OK) {
        status = write_u16(
            machine, UINT32_C(0x0180006c), source_value0
        );
    }
    if (status == VF2_OK) {
        status = write_u16(
            machine, UINT32_C(0x0180008c), source_value1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x01801058), lookup
        );
    }
    if (status == VF2_OK) {
        status = write_u16(
            machine, UINT32_C(0x01800466), UINT16_C(0x82df)
        );
    }
    if (status == VF2_OK) {
        status = write_u16(
            machine, UINT32_C(0x0180046a), UINT16_C(0x815b)
        );
    }
    if (status == VF2_OK) {
        status = write_u16(
            machine, UINT32_C(0x01801466), UINT16_C(0x82df)
        );
    }
    if (status == VF2_OK) {
        status = write_u16(
            machine, UINT32_C(0x0180146a), UINT16_C(0x815b)
        );
    }
    if (status == VF2_OK) {
        status = finish_recovered_procedure(machine, cpu, UINT64_C(40));
    }
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_VIDEO_LAYER_COMMIT;
    report->entry_address = VF2_VIDEO_LAYER_COMMIT_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(9);
    report->bytes_written = 20u;
    report->recovered_instruction_count = UINT64_C(40);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    (void)auxiliary;
    return VF2_OK;
}

vf2_status execute_tile_controller_update(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report glyph_report;
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint32_t runtime_flags = 0u;
    uint32_t controller = 0u;
    uint32_t controller_flags = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(&glyph_report, 0, sizeof(glyph_report));
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00508000), &runtime_flags);
    if (status != VF2_OK || (runtime_flags & (UINT32_C(1) << 2u)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[10] = runtime_flags;

    status = vf2_model2a_read_u32(machine, UINT32_C(0x0059e000), &controller);
    if (status != VF2_OK || controller != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[VF2_I960_G0_REGISTER + 3u] = controller;

    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0059e008), &controller_flags
    );
    if (status != VF2_OK ||
        (controller_flags & ((UINT32_C(1) << 2u) |
                             (UINT32_C(1) << 4u) |
                             UINT32_C(1))) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[9] = controller_flags;

    if ((controller_flags & (UINT32_C(1) << 1u)) == 0u) {
        status = finish_recovered_procedure(machine, cpu, UINT64_C(12));
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_TILE_CONTROLLER_UPDATE;
        report->entry_address = VF2_TILE_CONTROLLER_UPDATE_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(0);
        report->changed_values = UINT64_C(0);
        report->bytes_written = 0u;
        report->recovered_instruction_count = UINT64_C(12);
        report->recovered_procedure_calls = UINT64_C(0);
        report->recovered_procedure_returns = UINT64_C(1);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

    controller_flags &= ~(UINT32_C(1) << 1u);
    cpu->registers[9] = controller_flags;
    status = vf2_model2a_write_u32(
        machine, UINT32_C(0x0059e008), controller_flags
    );
    if (status != VF2_OK) {
        return status;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_TILE_GLYPH_EXPAND_ENTRY, UINT32_C(0x0004ebfc)
    );
    if (status == VF2_OK) {
        status = execute_tile_glyph_expand(machine, cpu, &glyph_report);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0004ebfc)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = finish_recovered_procedure(machine, cpu, UINT64_C(15));
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_HYBRID_BRIDGE_TILE_CONTROLLER_UPDATE;
    report->entry_address = VF2_TILE_CONTROLLER_UPDATE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(1) + glyph_report.changed_values;
    report->bytes_written = sizeof(uint32_t) + glyph_report.bytes_written;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_video_status_latch(
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

vf2_status execute_palette_page_upload(
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
        /* cmpobe at 0x00002dec branches on equality without updating the
         * i960 arithmetic condition code. Preserve the incoming condition. */
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
