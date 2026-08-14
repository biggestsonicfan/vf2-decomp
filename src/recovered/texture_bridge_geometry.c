#include "texture_bridge_internal.h"

static void geometry_gate_set_compare(
    vf2_i960_cpu *cpu,
    vf2_i960_compare_result result
)
{
    uint32_t condition_bits = 0u;

    if (result == VF2_I960_COMPARE_LESS) {
        condition_bits = UINT32_C(4);
    } else if (result == VF2_I960_COMPARE_EQUAL) {
        condition_bits = UINT32_C(2);
    } else if (result == VF2_I960_COMPARE_GREATER) {
        condition_bits = UINT32_C(1);
    }
    cpu->compare_result = result;
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | condition_bits;
}

vf2_status execute_frame_geometry_gate(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t flags = 0u;
    uint8_t frame_state = 0u;
    uint8_t alt_byte = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500704), &flags);
    if (status != VF2_OK) {
        return status;
    }

    if ((flags & ((UINT32_C(1) << 26u) | UINT32_C(4))) == 0u) {
        /* Clear-flags path: ld, bbs(26,fail), bbs(2,fail), b 0x0000a800, ret.
         * The second BBS leaves the architectural condition code unordered. */
        status = finish_recovered_procedure(machine, cpu, UINT64_C(5));
        if (status != VF2_OK) {
            return status;
        }
        geometry_gate_set_compare(cpu, VF2_I960_COMPARE_NONE);
        report->kind = VF2_HYBRID_BRIDGE_FRAME_GEOMETRY_GATE;
        report->entry_address = VF2_FRAME_GEOMETRY_GATE_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->recovered_instruction_count = UINT64_C(5);
        report->recovered_procedure_returns = UINT64_C(1);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

    /* Busy path observed on the third scheduler sweep via 0x0000a010. The
     * reference i960 reads 0x0050002a after the bit-26/bit-2 branch. The
     * observed live values are 1 (sweep #1) and 17 (sweep #2); sweep #3 and
     * #4 fall through the gate with flags=0. Only the value 17 forwards to
     * the alt-byte check at 0x0000a778; any other value writes 16 into the
     * state byte and returns through 0x0000a800. */
    status = vf2_model2a_read(
        machine, UINT32_C(0x0050002a), &frame_state, sizeof(frame_state)
    );
    if (status != VF2_OK) {
        return status;
    }
    if (frame_state != UINT8_C(17)) {
        const uint8_t compared_frame_state = frame_state;

        /* ld, bbs(26,taken), ldob, cmpobe(fail), mov 16, stib, b 0x0000a800,
         * ret = 8 instructions. CMPobe leaves the result of 17 vs state in CC. */
        frame_state = UINT8_C(16);
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050002a), &frame_state, sizeof(frame_state)
        );
        if (status != VF2_OK) {
            return status;
        }
        status = finish_recovered_procedure(machine, cpu, UINT64_C(8));
        if (status != VF2_OK) {
            return status;
        }
        geometry_gate_set_compare(
            cpu,
            UINT8_C(17) < compared_frame_state
                ? VF2_I960_COMPARE_LESS
                : VF2_I960_COMPARE_GREATER
        );
        report->kind = VF2_HYBRID_BRIDGE_FRAME_GEOMETRY_GATE;
        report->entry_address = VF2_FRAME_GEOMETRY_GATE_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->changed_values = UINT64_C(1);
        report->bytes_written = sizeof(frame_state);
        report->recovered_instruction_count = UINT64_C(8);
        report->recovered_procedure_returns = UINT64_C(1);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

    /* frame_state == 17 forwards to 0x0000a778 which reads 0x005000a6 and
     * returns through 0x0000a800 when the alt byte is non-zero. The observed
     * third-sweep live values for 0x005000a6 are all 255, so the deep reset
     * call sequence (calls to 0x00008ef0 and 0x0006116c followed by an
     * unconditional branch to the reset entry 0x000000b0) never executes on
     * the observed path and remains unsupported. */
    status = vf2_model2a_read(
        machine, UINT32_C(0x005000a6), &alt_byte, sizeof(alt_byte)
    );
    if (status != VF2_OK) {
        return status;
    }
    if (alt_byte != 0u) {
        /* ld, bbs(26,taken), ldob, cmpobe(17,taken), ldob, cmpobne(0,taken),
         * ret = 7 instructions. CMPobne leaves 0 < alt_byte in CC. */
        status = finish_recovered_procedure(machine, cpu, UINT64_C(7));
        if (status != VF2_OK) {
            return status;
        }
        geometry_gate_set_compare(cpu, VF2_I960_COMPARE_LESS);
        report->kind = VF2_HYBRID_BRIDGE_FRAME_GEOMETRY_GATE;
        report->entry_address = VF2_FRAME_GEOMETRY_GATE_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->recovered_instruction_count = UINT64_C(7);
        report->recovered_procedure_returns = UINT64_C(1);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

    /* alt_byte == 0 forwards the unobserved deep reset sequence at 0x0000a784
     * that zeroes 0x00500700/0x00500704/0x00500708/0x0050070c, writes 0 to
     * 0x00e80004, calls 0x00008ef0 and 0x0006116c and branches (does not
     * return) to 0x000000b0. No live evidence covers this state. */
    return VF2_ERROR_UNSUPPORTED;
}

vf2_status execute_geometry_frame_commit(
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

vf2_status execute_geometry_command_setup(
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

vf2_status execute_frame_scratch_clear(
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
