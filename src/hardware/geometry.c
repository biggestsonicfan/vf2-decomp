#include "vf2/geometry.h"

uint32_t vf2_geometry_pack_command(int16_t source_value, uint8_t command_class)
{
    uint32_t command = ((uint32_t)(int32_t)source_value << 8u) &
                       UINT32_C(0x807fffff);
    command |= (uint32_t)command_class << 23u;
    return command;
}

vf2_status vf2_geometry_setup_command(
    vf2_model2a *machine,
    uint32_t geometry_base,
    uint32_t destination_offset,
    int16_t source_value,
    uint8_t command_class,
    uint32_t *command
)
{
    uint32_t packed = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || command == NULL ||
        geometry_base != VF2_GEOMETRY_BASE ||
        destination_offset > VF2_GEOMETRY_SIZE - sizeof(uint32_t)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    packed = vf2_geometry_pack_command(source_value, command_class);
    status = vf2_model2a_write_u32(
        machine, geometry_base + VF2_GEOMETRY_STATUS_OFFSET, 0u
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_GEOMETRY_COMMAND_RESULT, packed
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, geometry_base + destination_offset, packed
        );
    }
    if (status == VF2_OK) {
        *command = packed;
    }
    return status;
}

vf2_status vf2_geometry_commit_frame(
    vf2_model2a *machine,
    uint32_t geometry_base,
    vf2_geometry_commit_report *report
)
{
    uint32_t previous_command = 0u;
    uint32_t read_pointer = 0u;
    uint32_t maximum_distance = 0u;
    uint32_t next_command = 0u;
    uint8_t ring_index = 0u;
    int32_t distance = 0;
    int maximum_updated = 0;
    vf2_status status = VF2_OK;

    if (machine == NULL || report == NULL ||
        geometry_base != VF2_GEOMETRY_BASE) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read_u32(
        machine, VF2_GEOMETRY_COMMAND_STATE, &previous_command
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, geometry_base + VF2_GEOMETRY_FRAME_STATUS_OFFSET, 0u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, geometry_base + VF2_GEOMETRY_PREVIOUS_OFFSET,
            previous_command
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, geometry_base + VF2_GEOMETRY_READ_OFFSET,
            &read_pointer
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_GEOMETRY_MAX_DISTANCE, &maximum_distance
        );
    }
    distance = (int32_t)((read_pointer & VF2_GEOMETRY_RING_MASK) -
                         (previous_command & VF2_GEOMETRY_RING_MASK));
    if (status == VF2_OK && distance > (int32_t)maximum_distance) {
        maximum_updated = 1;
        status = vf2_model2a_write_u32(
            machine, VF2_GEOMETRY_MAX_DISTANCE, (uint32_t)distance
        );
        if (status == VF2_OK) {
            maximum_distance = (uint32_t)distance;
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_GEOMETRY_RING_INDEX, &ring_index, sizeof(ring_index)
        );
    }
    ring_index = (uint8_t)((ring_index + 1u) % VF2_GEOMETRY_COMMAND_RING_COUNT);
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, VF2_GEOMETRY_RING_INDEX, &ring_index, sizeof(ring_index)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, (uint32_t)(VF2_GEOMETRY_COMMAND_TABLE +
                (uint32_t)ring_index * sizeof(uint32_t)), &next_command
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_GEOMETRY_COMMAND_STATE, next_command
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, geometry_base + VF2_GEOMETRY_WRITE_OFFSET, next_command
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    report->distance = distance;
    report->maximum_distance = maximum_distance;
    report->next_command = next_command;
    report->ring_index = ring_index;
    report->maximum_updated = maximum_updated;
    return VF2_OK;
}
