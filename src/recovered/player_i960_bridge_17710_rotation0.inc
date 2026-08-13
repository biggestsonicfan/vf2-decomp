/*
 * Evidence-backed zero-angle subset of TGP command 0x2d805b5b.
 *
 * 0x17710 can reach the planar rotation command through the state-bit-23
 * path.  When (+0x61a - signed +0x26) is zero, the clamped command angle is
 * exactly zero and any correct planar rotation must be the identity.  This
 * corridor therefore recovers the TGP-facing branch without assigning a
 * guessed sign convention to non-zero angles.
 */
static vf2_status player_execute_17710_rotation_zero(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint64_t max_steps
)
{
    const uint32_t player = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 7u] : 0u;
    const uint32_t opponent = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 8u] : 0u;
    const uint32_t port = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 11u] +
          cpu->registers[VF2_I960_G0_REGISTER + 12u]
        : 0u;
    uint32_t player_state = 0u;
    uint32_t opponent_state = 0u;
    uint32_t player_flags = 0u;
    uint32_t base_x = 0u;
    uint32_t base_z = 0u;
    uint32_t position_x = 0u;
    uint32_t position_z = 0u;
    uint32_t delta_z = 0u;
    uint16_t source_angle = 0u;
    uint16_t current_angle = 0u;
    uint8_t gate = 0u;
    uint64_t instruction_count = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL ||
        cpu->ip != VF2_PLAYER_17710_ENTRY ||
        cpu->local_frame_depth < 3u ||
        player == 0u || opponent == 0u || port == 0u ||
        (cpu->registers[0] & UINT32_C(7)) != 0u ||
        cpu->local_frames[cpu->local_frame_depth - 1u].registers[2] !=
            VF2_PLAYER_17710_STOP) {
        return VF2_ERROR_UNSUPPORTED;
    }

    /* The entry always performs these reads before selecting bit 23. */
    status = player_tail_read_u16(
        machine, player + UINT32_C(0x05b6), &current_angle
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player + UINT32_C(0x01a4), &player_state
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, opponent + UINT32_C(0x01a4), &opponent_state
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    (void)current_angle;
    (void)opponent_state;
    if ((player_state & (UINT32_C(1) << 23u)) == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = player_tail_read_u16(
        machine, player + UINT32_C(0x061a), &source_angle
    );
    if (status == VF2_OK) {
        status = player_tail_read_u16(
            machine, player + UINT32_C(0x0026), &current_angle
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if ((uint16_t)(source_angle - current_angle) != UINT16_C(0)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_model2a_read_u32(machine, player, &player_flags);
    if (status != VF2_OK) {
        return status;
    }
    /* Bit 2 clear rotates immediately. Bit 2 set consults +0x840 and rotates
     * only when its bit 0 is clear. */
    instruction_count = UINT64_C(17);
    if ((player_flags & (UINT32_C(1) << 2u)) != 0u) {
        status = player_tail_read_u8(
            machine, player + UINT32_C(0x0840), &gate
        );
        if (status != VF2_OK) {
            return status;
        }
        instruction_count += UINT64_C(2);
        if ((gate & UINT8_C(1)) != 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
    }

    status = vf2_model2a_read_u32(
        machine, player + UINT32_C(0x01f4), &base_x
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player + UINT32_C(0x0018), &position_x
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player + UINT32_C(0x01fc), &base_z
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player + UINT32_C(0x0020), &position_z
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    /* The zero-angle transform is identity. The final flat-port backing word
     * after command, angle, X and Z stores is the Z delta. */
    delta_z = position_z - base_z;
    (void)base_x;
    (void)position_x;

    /* 19 instructions execute the complete command/read/store sequence,
     * followed by the three +0x26 tail instructions and RET. */
    instruction_count += UINT64_C(23);
    if (max_steps != 0u && max_steps < instruction_count) {
        return VF2_ERROR_UNSUPPORTED;
    }

    if (machine->copro_read_callback == NULL &&
        machine->copro_write_callback == NULL) {
        status = vf2_model2a_write_u32(machine, port, delta_z);
        if (status != VF2_OK) {
            return status;
        }
    }

    cpu->ip = VF2_PLAYER_17710_RET;
    cpu->executed_instructions += instruction_count - UINT64_C(1);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }
    ++cpu->executed_instructions;
    return cpu->ip == VF2_PLAYER_17710_STOP
        ? VF2_OK : VF2_ERROR_UNSUPPORTED;
}
