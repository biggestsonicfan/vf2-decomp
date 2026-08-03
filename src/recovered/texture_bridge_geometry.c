#include "texture_bridge_internal.h"

vf2_status execute_frame_geometry_gate(
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
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500704), &flags);
    if (status != VF2_OK || (flags & ((UINT32_C(1) << 26u) | UINT32_C(4))) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = finish_recovered_procedure(machine, cpu, UINT64_C(5));
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_FRAME_GEOMETRY_GATE;
    report->entry_address = VF2_FRAME_GEOMETRY_GATE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->recovered_instruction_count = UINT64_C(5);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
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
