typedef struct vf2_player_17710_plan {
    uint32_t player;
    uint16_t status_value;
    uint64_t instruction_count;
    vf2_i960_compare_result compare_result;
    bool write_status;
    bool update_compare;
} vf2_player_17710_plan;

static vf2_i960_compare_result player_17710_compare_signed(
    int32_t left,
    int32_t right
)
{
    if (left < right) {
        return VF2_I960_COMPARE_LESS;
    }
    if (left > right) {
        return VF2_I960_COMPARE_GREATER;
    }
    return VF2_I960_COMPARE_EQUAL;
}

/*
 * Recover the non-coprocessor control-flow family rooted at 0x17710.
 *
 * The routine is intentionally planned before the first mutation. Early exits,
 * range-normalization paths and the status-delta tail are native; only paths
 * that actually fall into the 0x178bc TGP command sequence are rejected back
 * to the architectural interpreter.
 */
static vf2_status player_prepare_17710(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_player_17710_plan *plan
)
{
    const uint32_t player = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 7u] : 0u;
    const uint32_t opponent = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 8u] : 0u;
    uint32_t player_state = 0u;
    uint32_t opponent_state = 0u;
    uint32_t player_flags = 0u;
    uint32_t opponent_flags = 0u;
    uint32_t r9 = 0u;
    uint32_t r7 = 0u;
    uint32_t r6 = 0u;
    uint32_t r5 = 0u;
    uint16_t word = 0u;
    uint8_t gate = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || plan == NULL ||
        cpu->ip != VF2_PLAYER_17710_ENTRY ||
        cpu->local_frame_depth < 3u || player == 0u || opponent == 0u ||
        (cpu->registers[0] & UINT32_C(7)) != 0u ||
        cpu->local_frames[cpu->local_frame_depth - 1u].registers[2] !=
            VF2_PLAYER_17710_STOP) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(plan, 0, sizeof(*plan));
    plan->player = player;

    status = player_read_u16(
        machine, player + UINT32_C(0x05b6), &word
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

    r9 = (uint32_t)(int32_t)(int16_t)word;
    plan->instruction_count = UINT64_C(4);

    /* 0x1771c: state bit 23 selects the +0x61a delta source. */
    if ((player_state & (UINT32_C(1) << 23u)) != 0u) {
        status = player_read_u16(
            machine, player + UINT32_C(0x061a), &word
        );
        if (status != VF2_OK) {
            return status;
        }
        r9 = (uint32_t)word;
        plan->instruction_count += UINT64_C(2);
        goto path_177e0;
    }

    status = vf2_model2a_read_u32(machine, player, &player_flags);
    if (status != VF2_OK) {
        return status;
    }
    plan->instruction_count += UINT64_C(2);
    if ((player_flags & (UINT32_C(1) << 9u)) != 0u) {
        status = player_read_u16(
            machine, player + UINT32_C(0x061a), &word
        );
        if (status != VF2_OK) {
            return status;
        }
        r9 = (uint32_t)word;
        ++plan->instruction_count;
    }

    ++plan->instruction_count;
    if ((player_state & (UINT32_C(1) << 27u)) != 0u) {
        goto early_return;
    }

    plan->instruction_count += UINT64_C(2);
    if ((player_flags & (UINT32_C(1) << 22u)) != 0u) {
        goto early_return;
    }

    ++plan->instruction_count;
    if ((opponent_state & (UINT32_C(1) << 28u)) != 0u) {
        status = player_read_u16(
            machine, opponent + UINT32_C(0x01aa), &word
        );
        if (status != VF2_OK) {
            return status;
        }
        plan->instruction_count += UINT64_C(2);
        if (UINT32_C(10) > (uint32_t)word) {
            goto early_return;
        }
        ++plan->instruction_count;
        if ((player_state & (UINT32_C(1) << 13u)) != 0u) {
            goto path_17854;
        }
        ++plan->instruction_count;
        if ((player_state & (UINT32_C(1) << 10u)) != 0u) {
            goto path_17854;
        }
        ++plan->instruction_count;
        goto early_return;
    }

    plan->instruction_count += UINT64_C(3);
    if ((player_state & UINT32_C(0x00001010)) == UINT32_C(0x00001010)) {
        goto early_return;
    }

    ++plan->instruction_count;
    if ((player_state & (UINT32_C(1) << 14u)) != 0u) {
        goto early_return;
    }

    ++plan->instruction_count;
    if ((player_state & (UINT32_C(1) << 16u)) != 0u) {
        ++plan->instruction_count;
        if ((player_state & (UINT32_C(1) << 8u)) == 0u) {
            ++plan->instruction_count;
            if ((opponent_state & (UINT32_C(1) << 8u)) != 0u) {
                ++plan->instruction_count;
                if ((opponent_state & (UINT32_C(1) << 4u)) != 0u) {
                    goto early_return;
                }
            }
        }
    }

    ++plan->instruction_count;
    if ((player_state & (UINT32_C(1) << 13u)) != 0u) {
        goto path_17800;
    }

    ++plan->instruction_count;
    if ((player_state & (UINT32_C(1) << 18u)) != 0u) {
        ++plan->instruction_count;
        if ((player_state & (UINT32_C(1) << 1u)) != 0u) {
            goto path_177f0;
        }
        ++plan->instruction_count;
        goto early_return;
    }

    ++plan->instruction_count;
    if ((player_state & (UINT32_C(1) << 2u)) != 0u) {
        goto path_17854;
    }

    ++plan->instruction_count;
    if ((player_state & (UINT32_C(1) << 10u)) != 0u) {
        goto path_17800;
    }

    plan->instruction_count += UINT64_C(4);
    if ((player_state & UINT32_C(0x00001102)) == UINT32_C(0x00001002)) {
        goto path_17800;
    }

    plan->instruction_count += UINT64_C(3);
    if ((opponent_state & UINT32_C(0x00004010)) == UINT32_C(0x00004010)) {
        goto path_17854;
    }

    ++plan->instruction_count;
    if ((player_state & (UINT32_C(1) << 1u)) != 0u) {
        ++plan->instruction_count;
        if ((opponent_state & (UINT32_C(1) << 15u)) != 0u) {
            goto early_return;
        }
        ++plan->instruction_count;
        goto path_17818;
    }

    ++plan->instruction_count;
    if ((player_state & (UINT32_C(1) << 30u)) != 0u) {
        ++plan->instruction_count;
        goto path_17818;
    }

    ++plan->instruction_count;
    goto early_return;

path_177e0:
    status = player_read_u16(machine, player + UINT32_C(0x0026), &word);
    if (status != VF2_OK) {
        return status;
    }
    ++plan->instruction_count;
    r9 = (uint32_t)((int64_t)r9 - (int64_t)(int16_t)word);
    plan->instruction_count += UINT64_C(3);
    r6 = UINT32_C(0x038e);
    goto path_17890;

path_177f0: {
    uint16_t opponent_word = 0u;

    status = player_read_u16(machine, player + UINT32_C(0x0026), &word);
    if (status == VF2_OK) {
        status = player_read_u16(
            machine, opponent + UINT32_C(0x0026), &opponent_word
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    plan->instruction_count += UINT64_C(2);
    r9 = (uint32_t)((int64_t)(int16_t)opponent_word -
                    (int64_t)(int16_t)word);
    plan->instruction_count += UINT64_C(2);
    goto path_1785c;
}

path_17800:
    r6 = UINT32_C(45);
    ++plan->instruction_count;
    status = vf2_model2a_read_u32(machine, opponent, &opponent_flags);
    if (status != VF2_OK) {
        return status;
    }
    ++plan->instruction_count;
    ++plan->instruction_count;
    if ((opponent_flags & (UINT32_C(1) << 9u)) == 0u) {
        ++plan->instruction_count;
        if ((opponent_state & (UINT32_C(1) << 15u)) == 0u) {
            goto path_17854;
        }
        r6 = UINT32_C(0x005b);
        ++plan->instruction_count;
    }
    ++plan->instruction_count;
    goto path_1781c;

path_17818:
    r6 = UINT32_C(0x016c);
    ++plan->instruction_count;

path_1781c:
    status = player_read_u16(
        machine, player + UINT32_C(0x05b4), &word
    );
    if (status != VF2_OK) {
        return status;
    }
    ++plan->instruction_count;
    r9 = (uint32_t)((int64_t)r9 - (int64_t)(int16_t)word);
    ++plan->instruction_count;
    r7 = UINT32_C(0x2aa8);
    r5 = UINT32_C(1);
    plan->instruction_count += UINT64_C(3);
    goto path_1786c;

path_17854:
    status = player_read_u16(
        machine, player + UINT32_C(0x05b4), &word
    );
    if (status != VF2_OK) {
        return status;
    }
    ++plan->instruction_count;
    r9 = (uint32_t)((int64_t)r9 - (int64_t)(int16_t)word);
    ++plan->instruction_count;

path_1785c:
    r7 = UINT32_C(0x5ffa);
    r6 = UINT32_C(0x038e);
    r5 = 0u;
    plan->instruction_count += UINT64_C(3);

path_1786c: {
    const int32_t negative_limit = -(int32_t)r7;
    vf2_i960_compare_result compare = VF2_I960_COMPARE_NONE;

    r9 = (uint32_t)(int32_t)(int16_t)(uint16_t)r9;
    plan->instruction_count += UINT64_C(2);
    ++plan->instruction_count;
    compare = player_17710_compare_signed((int32_t)r9, negative_limit);
    ++plan->instruction_count;
    if (compare != VF2_I960_COMPARE_EQUAL) {
        compare = player_17710_compare_signed((int32_t)r9, (int32_t)r7);
    }
    ++plan->instruction_count;
    plan->compare_result = compare;
    plan->update_compare = true;
    ++plan->instruction_count;
    if (compare != VF2_I960_COMPARE_EQUAL) {
        ++plan->instruction_count;
        if (r5 != 0u) {
            goto early_return;
        }
        ++plan->instruction_count;
        r9 += UINT32_C(0x8000);
        ++plan->instruction_count;
        goto path_17890;
    }
}
    goto path_17898;

path_17890:
    r9 = (uint32_t)(int32_t)(int16_t)(uint16_t)r9;
    plan->instruction_count += UINT64_C(2);

path_17898: {
    const int32_t negative_clamp = -(int32_t)r6;

    ++plan->instruction_count;
    ++plan->instruction_count;
    if (negative_clamp > (int32_t)r9) {
        r9 = (uint32_t)negative_clamp;
        ++plan->instruction_count;
    }
    ++plan->instruction_count;
    if ((int32_t)r6 < (int32_t)r9) {
        r9 = r6;
        ++plan->instruction_count;
    }
}

    status = vf2_model2a_read_u32(machine, player, &player_flags);
    if (status != VF2_OK) {
        return status;
    }
    ++plan->instruction_count;
    ++plan->instruction_count;
    if ((player_flags & (UINT32_C(1) << 2u)) == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = player_read_u8(machine, player + UINT32_C(0x0840), &gate);
    if (status != VF2_OK) {
        return status;
    }
    ++plan->instruction_count;
    ++plan->instruction_count;
    if ((gate & UINT8_C(1)) == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = player_read_u16(machine, player + UINT32_C(0x0026), &word);
    if (status != VF2_OK) {
        return status;
    }
    ++plan->instruction_count;
    plan->status_value = (uint16_t)(
        (uint32_t)(int32_t)(int16_t)word + r9
    );
    plan->instruction_count += UINT64_C(2);
    plan->write_status = true;

early_return:
    ++plan->instruction_count;
    return VF2_OK;
}

static vf2_status player_apply_17710(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    const vf2_player_17710_plan *plan
)
{
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || plan == NULL ||
        cpu->ip != VF2_PLAYER_17710_ENTRY ||
        cpu->local_frame_depth < 3u ||
        (cpu->registers[0] & UINT32_C(7)) != 0u ||
        cpu->local_frames[cpu->local_frame_depth - 1u].registers[2] !=
            VF2_PLAYER_17710_STOP ||
        plan->player != cpu->registers[VF2_I960_G0_REGISTER + 7u] ||
        plan->instruction_count == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    if (plan->write_status) {
        status = player_write_u16(
            machine, plan->player + UINT32_C(0x0026), plan->status_value
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    if (plan->update_compare) {
        player_set_compare_result(cpu, plan->compare_result);
    }

    cpu->ip = VF2_PLAYER_17710_RET;
    cpu->executed_instructions += plan->instruction_count - UINT64_C(1);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }
    ++cpu->executed_instructions;
    return cpu->ip == VF2_PLAYER_17710_STOP
        ? VF2_OK : VF2_ERROR_UNSUPPORTED;
}

static vf2_status player_execute_17710(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint64_t max_steps
)
{
    vf2_player_17710_plan plan;
    vf2_status status = VF2_OK;

    /* Keep the earlier exact observed helper linked while remapping the
     * downstream dispatcher call to this generalized planner. */
    (void)player_execute_17710_fast_exit;
    status = player_prepare_17710(machine, cpu, &plan);

    if (status != VF2_OK) {
        return status;
    }
    if (max_steps != 0u && max_steps < plan.instruction_count) {
        return VF2_ERROR_UNSUPPORTED;
    }
    return player_apply_17710(machine, cpu, &plan);
}

/* The dispatcher below this include already owns the run-options object.
 * Remap only its legacy two-argument 0x17710 call to the generalized planner,
 * preserving the public bridge surface and the raw-i960 fallback. */
#define player_execute_17710_fast_exit(machine_, cpu_) \
    player_execute_17710((machine_), (cpu_), options->max_steps)
