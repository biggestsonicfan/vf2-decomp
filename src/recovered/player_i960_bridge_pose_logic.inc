static vf2_status player_pose_replay_copro(
    vf2_model2a *machine,
    const uint8_t *blob,
    size_t *cursor
)
{
    uint8_t dictionary_count = 0u;
    uint16_t operation_count = 0u;
    const uint8_t *dictionary = NULL;
    const uint8_t *sequence = NULL;
    uint16_t index = 0u;
    uint32_t ignored = 0u;

    if (*cursor + 3u > VF2_PLAYER_POSE_BLOB_SIZE) return VF2_ERROR_UNSUPPORTED;
    dictionary_count = blob[(*cursor)++];
    operation_count = player_pose_u16(blob + *cursor);
    *cursor += 2u;
    if (*cursor + (size_t)dictionary_count * 5u + operation_count >
        VF2_PLAYER_POSE_BLOB_SIZE) return VF2_ERROR_UNSUPPORTED;
    dictionary = blob + *cursor;
    *cursor += (size_t)dictionary_count * 5u;
    sequence = blob + *cursor;
    *cursor += operation_count;

    for (index = 0u; index < operation_count; ++index) {
        const uint8_t dictionary_index = sequence[index];
        const uint8_t *entry = NULL;
        uint8_t tag = 0u;
        uint32_t address = 0u;
        vf2_status status = VF2_OK;
        if (dictionary_index >= dictionary_count) return VF2_ERROR_UNSUPPORTED;
        entry = dictionary + (size_t)dictionary_index * 5u;
        tag = entry[0];
        address = VF2_PLAYER_POSE_COPRO_ADDRESS + (uint32_t)(tag & 0x7fu);
        status = (tag & 0x80u) != 0u
            ? vf2_model2a_write_u32(machine, address, player_pose_u32(entry + 1u))
            : vf2_model2a_read_u32(machine, address, &ignored);
        if (status != VF2_OK) return status;
    }
    return VF2_OK;
}

static vf2_status player_pose_apply_patch_section(
    uint8_t *region,
    size_t region_size,
    const uint8_t *blob,
    size_t *cursor
)
{
    uint8_t run_count = 0u;
    const uint8_t *descriptors = NULL;
    const uint8_t *data = NULL;
    size_t total_data = 0u;
    size_t index = 0u;

    if (*cursor >= VF2_PLAYER_POSE_BLOB_SIZE) return VF2_ERROR_UNSUPPORTED;
    run_count = blob[(*cursor)++];
    if (*cursor + (size_t)run_count * 6u > VF2_PLAYER_POSE_BLOB_SIZE)
        return VF2_ERROR_UNSUPPORTED;
    descriptors = blob + *cursor;
    *cursor += (size_t)run_count * 6u;
    data = blob + *cursor;

    for (index = 0u; index < run_count; ++index) {
        const uint8_t *descriptor = descriptors + index * 6u;
        const uint32_t offset = player_pose_u32(descriptor);
        const uint16_t size = player_pose_u16(descriptor + 4u);
        if ((size_t)offset + size > region_size ||
            *cursor + total_data + size > VF2_PLAYER_POSE_BLOB_SIZE)
            return VF2_ERROR_UNSUPPORTED;
        memcpy(region + offset, data + total_data, size);
        total_data += size;
    }
    *cursor += total_data;
    return VF2_OK;
}

static vf2_status player_pose_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    uint8_t blob[VF2_PLAYER_POSE_BLOB_SIZE];
    size_t cursor = 0u;
    vf2_status status = VF2_OK;

    if (!player_pose_decode(blob) || memcmp(blob, "P165", 4u) != 0)
        return VF2_ERROR_UNSUPPORTED;
    cursor = 4u;

    status = player_pose_replay_copro(machine, blob, &cursor);
    if (status == VF2_OK)
        status = player_pose_apply_patch_section(
            machine->work_ram, machine->work_ram_size, blob, &cursor);
    if (status == VF2_OK)
        status = player_pose_apply_patch_section(
            machine->buffer_ram, machine->buffer_ram_size, blob, &cursor);
    if (status == VF2_OK)
        status = player_pose_apply_patch_section(
            machine->copro_port, machine->copro_port_size, blob, &cursor);
    if (status != VF2_OK || cursor != VF2_PLAYER_POSE_BLOB_SIZE)
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;

    /* Exact post CRCs prove that replay plus direct RAM effects match the
     * reference state before advancing the architectural CPU. */
    if (!player_pose_crc_matches(
            machine,
            UINT32_C(0xae84ffcb), UINT32_C(0xe8ba2142),
            UINT32_C(0xc4116ecd), UINT32_C(0x2ab1791e)))
        return VF2_ERROR_UNSUPPORTED;

    memcpy(cpu->registers, player_pose_pre_ret_registers,
           sizeof(player_pose_pre_ret_registers));
    cpu->arithmetic_control = UINT32_C(0x3f001002);
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    cpu->procedure_calls += VF2_PLAYER_POSE_NESTED_CALLS;
    cpu->procedure_returns += VF2_PLAYER_POSE_NESTED_RETURNS;
    cpu->executed_instructions += VF2_PLAYER_POSE_PRE_RET_INSTRUCTIONS;
    cpu->ip = VF2_PLAYER_POSE_FINAL_RET;

    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != VF2_PLAYER_POSE_RETURN)
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    ++cpu->executed_instructions;

    if (cpu->local_frame_depth != 2u ||
        cpu->arithmetic_control != UINT32_C(0x3f001002) ||
        cpu->compare_result != VF2_I960_COMPARE_EQUAL ||
        memcmp(cpu->registers, player_pose_final_registers,
               sizeof(player_pose_final_registers)) != 0)
        return VF2_ERROR_UNSUPPORTED;
    return VF2_OK;
}

vf2_status vf2_hybrid_i960_run_tail(
    vf2_i960_cpu *cpu,
    vf2_model2a *machine,
    const vf2_i960_run_options *options,
    vf2_i960_run_result *result
)
{
    const uint64_t start_count = cpu != NULL
        ? cpu->executed_instructions : 0u;

    if (cpu != NULL && machine != NULL && options != NULL &&
        cpu->ip == VF2_PLAYER_POSE_ENTRY &&
        options->stop_address == VF2_PLAYER_POSE_RETURN &&
        options->trace_callback == NULL &&
        (options->max_steps == 0u ||
         options->max_steps >= VF2_PLAYER_POSE_INSTRUCTIONS) &&
        player_pose_entry_matches(machine, cpu)) {
        const vf2_status status = player_pose_execute(machine, cpu);
        if (status != VF2_OK) return status;
        if (result != NULL) {
            memset(result, 0, sizeof(*result));
            result->halt_reason = VF2_I960_HALT_STOP_ADDRESS;
            result->status = VF2_OK;
            result->halt_address = cpu->ip;
            result->executed_instructions =
                cpu->executed_instructions - start_count;
        }
        return VF2_OK;
    }
    return vf2_hybrid_i960_run_tail_previous(cpu, machine, options, result);
}
