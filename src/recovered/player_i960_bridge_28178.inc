#define VF2_PLAYER_28178_STREAM_ENTRY UINT32_C(0x00028178)
#define VF2_PLAYER_28178_STREAM_FIRST_RETURN UINT32_C(0x0001abf8)
#define VF2_PLAYER_28178_STREAM_FINAL_RET UINT32_C(0x0001b530)
#define VF2_PLAYER_28178_STREAM_STOP UINT32_C(0x00014400)
#define VF2_PLAYER_28178_STREAM_OPCODE_INTERPOLATE UINT8_C(0x12)
#define VF2_PLAYER_28178_STREAM_MAX_RECORDS 64u
#define VF2_PLAYER_28178_STREAM_SLOT_COUNT 256u

typedef struct vf2_player_28178_stream_record {
    uint8_t slot;
    uint8_t target;
    uint8_t duration;
    uint8_t immediate;
    int16_t step;
} vf2_player_28178_stream_record;

typedef struct vf2_player_28178_stream_slot {
    uint8_t loaded;
    uint8_t duration;
    uint8_t target;
    int16_t current;
    int16_t step;
} vf2_player_28178_stream_slot;

typedef struct vf2_player_28178_stream_tail_slot {
    uint8_t duration_before;
    uint8_t duration_after;
    uint8_t expires;
    int16_t current_before;
    int16_t current_after;
    int16_t step;
    uint64_t instruction_count;
} vf2_player_28178_stream_tail_slot;

typedef struct vf2_player_28178_stream_plan {
    uint32_t player;
    uint32_t record_pointer;
    uint32_t next_record_pointer;
    uint32_t state_flags;
    uint16_t selector;
    uint16_t counter;
    vf2_player_28178_stream_record records[VF2_PLAYER_28178_STREAM_MAX_RECORDS];
    vf2_player_28178_stream_tail_slot tail[2];
    size_t record_count;
    uint64_t instruction_count;
} vf2_player_28178_stream_plan;

static int16_t player_28178_stream_i16(uint16_t value)
{
    return (int16_t)value;
}

static vf2_status player_28178_stream_read_slot(
    const vf2_model2a *machine,
    uint32_t player,
    uint8_t slot,
    vf2_player_28178_stream_slot *state
)
{
    const uint32_t address =
        player + UINT32_C(0x06c4) + (uint32_t)slot * UINT32_C(6);
    uint8_t bytes[6] = {0u, 0u, 0u, 0u, 0u, 0u};
    vf2_status status = VF2_OK;

    if (machine == NULL || state == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read(machine, address, bytes, sizeof(bytes));
    if (status != VF2_OK) {
        return status;
    }
    state->loaded = 1u;
    state->duration = bytes[0];
    state->target = bytes[1];
    state->current = player_28178_stream_i16(
        (uint16_t)((uint16_t)bytes[2] | ((uint16_t)bytes[3] << 8u))
    );
    state->step = player_28178_stream_i16(
        (uint16_t)((uint16_t)bytes[4] | ((uint16_t)bytes[5] << 8u))
    );
    return VF2_OK;
}

static vf2_status player_28178_stream_preflight_word(
    const vf2_model2a *machine,
    uint32_t address
)
{
    uint32_t ignored = 0u;
    return vf2_model2a_read_u32(machine, address, &ignored);
}

static vf2_status player_28178_stream_prepare_tail_slot(
    const vf2_player_28178_stream_slot *state,
    vf2_player_28178_stream_tail_slot *tail
)
{
    if (state == NULL || tail == NULL || state->loaded == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(tail, 0, sizeof(*tail));
    tail->duration_before = state->duration;
    tail->current_before = state->current;
    tail->current_after = state->current;
    tail->step = state->step;

    if (state->duration == 0u) {
        tail->instruction_count = UINT64_C(5);
        return VF2_OK;
    }

    tail->duration_after = (uint8_t)(state->duration - UINT8_C(1));
    if (tail->duration_after == 0u) {
        tail->expires = 1u;
        /* The ROM stores only a zero byte at current+0, preserving the high
         * byte. Express the resulting signed 16-bit state for validation. */
        tail->current_after = (int16_t)((uint16_t)state->current & UINT16_C(0xff00));
        tail->instruction_count = UINT64_C(11);
    } else {
        tail->current_after = (int16_t)(
            (int32_t)state->current + (int32_t)state->step
        );
        tail->instruction_count = UINT64_C(13);
    }
    return VF2_OK;
}

static vf2_status player_prepare_28178_stream(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_player_28178_stream_plan *plan
)
{
    vf2_player_28178_stream_slot slots[VF2_PLAYER_28178_STREAM_SLOT_COUNT];
    uint32_t packed_state = 0u;
    uint32_t player_flags = 0u;
    uint32_t runtime_flags = 0u;
    uint32_t branch_base = 0u;
    uint32_t cursor = 0u;
    uint16_t threshold = 0u;
    uint8_t phase = 0u;
    uint8_t branch_byte = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || plan == NULL ||
        cpu->ip != VF2_PLAYER_28178_STREAM_ENTRY ||
        cpu->local_frame_depth < 4u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(plan, 0, sizeof(*plan));
    memset(slots, 0, sizeof(slots));
    plan->player = cpu->registers[VF2_I960_G0_REGISTER + 7u];
    if (plan->player == 0u ||
        (cpu->registers[0] & UINT32_C(7)) != 0u ||
        (cpu->local_frames[cpu->local_frame_depth - 1u].registers[0] &
         UINT32_C(7)) != 0u ||
        cpu->local_frames[cpu->local_frame_depth - 1u].registers[2] !=
            VF2_PLAYER_28178_STREAM_FIRST_RETURN ||
        cpu->local_frames[cpu->local_frame_depth - 2u].registers[2] !=
            VF2_PLAYER_28178_STREAM_STOP) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = player_tail_read_u8(
        machine, plan->player + UINT32_C(0x0a00), &phase
    );
    if (status == VF2_OK) {
        status = player_tail_read_u16(
            machine, plan->player + UINT32_C(0x01aa), &plan->counter
        );
    }
    if (status == VF2_OK) {
        status = player_tail_read_u16(
            machine, plan->player + UINT32_C(0x01a8), &plan->selector
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, plan->player + UINT32_C(0x01a4), &plan->state_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, plan->player + UINT32_C(0x0804), &packed_state
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, plan->player, &player_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500068), &runtime_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050016c), &branch_base
        );
    }
    if (status == VF2_OK && branch_base != 0u) {
        status = player_tail_read_u8(
            machine, branch_base + UINT32_C(0x3351), &branch_byte
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, plan->player + UINT32_C(0x082c), &plan->record_pointer
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    /* This parser owns the phase-0, non-mirrored stream interpreter path. The
     * record language itself is generalized; unrelated phase/rotation state
     * remains an explicit fallback boundary. */
    if (plan->record_pointer == 0u || branch_base == 0u || phase != 0u ||
        (runtime_flags & (UINT32_C(1) << 20u)) != 0u ||
        (branch_byte & (UINT8_C(1) << 6u)) != 0u ||
        (plan->state_flags & ((UINT32_C(1) << 20u) |
                              (UINT32_C(1) << 7u))) != 0u ||
        (packed_state & (UINT32_C(1) << 7u)) != 0u ||
        (player_flags & (UINT32_C(1) << 12u)) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    /* Preflight fixed writes before parsing any records. */
    status = player_tail_read_u16(
        machine, plan->player + UINT32_C(0x0c4c), &threshold
    );
    if (status == VF2_OK) {
        status = player_28178_stream_preflight_word(
            machine, plan->player + UINT32_C(0x082c)
        );
    }
    if (status == VF2_OK) {
        status = player_28178_stream_preflight_word(
            machine, plan->player + UINT32_C(0x01a4)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cursor = plan->record_pointer;
    /* 0x28178's ldos/stos/RET plus the phase-0 path through 0x1ad58. */
    plan->instruction_count = UINT64_C(11);

    while (plan->record_count < VF2_PLAYER_28178_STREAM_MAX_RECORDS) {
        uint8_t opcode = 0u;
        uint8_t slot = 0u;
        uint8_t target = 0u;
        uint16_t target_count = 0u;
        int32_t remaining = 0;
        int32_t target_value = 0;
        int32_t step = 0;
        uint32_t duration = 0u;
        vf2_player_28178_stream_record *record = NULL;
        vf2_player_28178_stream_slot *slot_state = NULL;

        status = player_tail_read_u8(machine, cursor, &opcode);
        if (status != VF2_OK) {
            return status;
        }
        if (opcode == 0u) {
            /* ldob opcode + cmpobe terminator. */
            plan->instruction_count += UINT64_C(2);
            break;
        }

        status = player_tail_read_u16(machine, cursor + UINT32_C(1), &threshold);
        if (status != VF2_OK) {
            return status;
        }
        if ((uint32_t)plan->counter < (uint32_t)threshold) {
            /* Header through cmpobl counter,threshold. */
            plan->instruction_count += UINT64_C(10);
            break;
        }
        if (opcode != VF2_PLAYER_28178_STREAM_OPCODE_INTERPOLATE) {
            /* A due opcode whose handler is not recovered must remain on the
             * architectural path; importantly, nothing has been mutated yet. */
            return VF2_ERROR_UNSUPPORTED;
        }

        status = player_tail_read_u8(machine, cursor + UINT32_C(3), &slot);
        if (status == VF2_OK) {
            status = player_tail_read_u8(machine, cursor + UINT32_C(4), &target);
        }
        if (status == VF2_OK) {
            status = player_tail_read_u16(
                machine, cursor + UINT32_C(5), &target_count
            );
        }
        if (status != VF2_OK) {
            return status;
        }

        slot_state = &slots[slot];
        if (slot_state->loaded == 0u) {
            status = player_28178_stream_read_slot(
                machine, plan->player, slot, slot_state
            );
            if (status != VF2_OK) {
                return status;
            }
        }

        record = &plan->records[plan->record_count];
        memset(record, 0, sizeof(*record));
        record->slot = slot;
        record->target = target;
        remaining = (int32_t)(uint32_t)target_count -
                    (int32_t)(uint32_t)(uint8_t)plan->counter;
        target_value = (int32_t)((uint32_t)target << 8u);

        if (remaining <= 0) {
            record->immediate = 1u;
            record->duration = UINT8_C(1);
            slot_state->duration = UINT8_C(1);
            slot_state->target = target;
            slot_state->current = (int16_t)(uint16_t)target_value;
            /* Due header (12) + immediate handler (19). */
            plan->instruction_count += UINT64_C(31);
        } else {
            duration = (uint32_t)remaining + UINT32_C(1);
            /* STOb truncates larger values on hardware, but the subsequent
             * two-slot tail would then observe a wrapped timer. Keep that
             * unmeasured edge explicit instead of silently inventing intent. */
            if (duration > UINT8_MAX) {
                return VF2_ERROR_UNSUPPORTED;
            }
            step = (target_value - (int32_t)slot_state->current) /
                   (int32_t)duration;
            record->duration = (uint8_t)duration;
            record->step = (int16_t)(uint16_t)step;
            slot_state->duration = record->duration;
            slot_state->target = target;
            slot_state->step = record->step;
            /* Due header (12) + interpolating handler (22). */
            plan->instruction_count += UINT64_C(34);
        }

        ++plan->record_count;
        if (cursor > UINT32_MAX - UINT32_C(7)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        cursor += UINT32_C(7);
    }

    if (plan->record_count == VF2_PLAYER_28178_STREAM_MAX_RECORDS) {
        /* Require a proven stopping record instead of accepting a truncated
         * parser window. */
        uint8_t opcode = 0u;
        status = player_tail_read_u8(machine, cursor, &opcode);
        if (status != VF2_OK) {
            return status;
        }
        if (opcode != 0u) {
            status = player_tail_read_u16(
                machine, cursor + UINT32_C(1), &threshold
            );
            if (status != VF2_OK ||
                (uint32_t)plan->counter >= (uint32_t)threshold) {
                return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
            }
        }
        plan->instruction_count += opcode == 0u
            ? UINT64_C(2) : UINT64_C(10);
    }
    plan->next_record_pointer = cursor;

    /* Materialize the two states that 0x1b4e8 advances every invocation. */
    for (index = 0u; index < 2u; ++index) {
        if (slots[index].loaded == 0u) {
            status = player_28178_stream_read_slot(
                machine, plan->player, (uint8_t)index, &slots[index]
            );
            if (status != VF2_OK) {
                return status;
            }
        }
        status = player_28178_stream_prepare_tail_slot(
            &slots[index], &plan->tail[index]
        );
        if (status != VF2_OK) {
            return status;
        }
    }

    /* 1b43c..1b4ec fixed path = 12, final RET = 1. The per-slot loop costs
     * 5 (inactive), 11 (expires now), or 13 (continues). */
    plan->instruction_count += UINT64_C(13) +
        plan->tail[0].instruction_count +
        plan->tail[1].instruction_count;
    return VF2_OK;
}

static vf2_status player_28178_stream_apply_record(
    vf2_model2a *machine,
    uint32_t player,
    const vf2_player_28178_stream_record *record
)
{
    const uint32_t entry =
        player + UINT32_C(0x06c4) + (uint32_t)record->slot * UINT32_C(6);
    uint8_t value = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || record == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    value = record->duration;
    status = vf2_model2a_write(machine, entry, &value, sizeof(value));
    value = record->target;
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, entry + UINT32_C(1), &value, sizeof(value)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    if (record->immediate != 0u) {
        const uint8_t zero = 0u;
        status = vf2_model2a_write(
            machine, entry + UINT32_C(2), &zero, sizeof(zero)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, entry + UINT32_C(3),
                &record->target, sizeof(record->target)
            );
        }
    } else {
        status = player_tail_write_u16(
            machine, entry + UINT32_C(4), (uint16_t)record->step
        );
    }
    return status;
}

static vf2_status player_28178_stream_apply_tail_slot(
    vf2_model2a *machine,
    uint32_t player,
    uint32_t slot,
    const vf2_player_28178_stream_tail_slot *tail
)
{
    const uint32_t entry =
        player + UINT32_C(0x06c4) + slot * UINT32_C(6);
    uint8_t value = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || tail == NULL || tail->duration_before == 0u) {
        return machine == NULL || tail == NULL
            ? VF2_ERROR_INVALID_ARGUMENT : VF2_OK;
    }
    value = tail->duration_after;
    status = vf2_model2a_write(machine, entry, &value, sizeof(value));
    if (status != VF2_OK) {
        return status;
    }
    if (tail->expires != 0u) {
        value = 0u;
        status = vf2_model2a_write(
            machine, entry + UINT32_C(1), &value, sizeof(value)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, entry + UINT32_C(2), &value, sizeof(value)
            );
        }
        return status;
    }
    return player_tail_write_u16(
        machine, entry + UINT32_C(2), (uint16_t)tail->current_after
    );
}

static vf2_status player_apply_28178_stream(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    const vf2_player_28178_stream_plan *plan
)
{
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || plan == NULL ||
        cpu->ip != VF2_PLAYER_28178_STREAM_ENTRY ||
        plan->player != cpu->registers[VF2_I960_G0_REGISTER + 7u]) {
        return VF2_ERROR_UNSUPPORTED;
    }

    /* 0x28178 ldos + 0x2817c stos, then the first architectural RET. */
    status = player_tail_write_u16(
        machine, plan->player + UINT32_C(0x0c4c), plan->selector
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions += UINT64_C(2);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }
    ++cpu->executed_instructions;
    if (cpu->ip != VF2_PLAYER_28178_STREAM_FIRST_RETURN) {
        return VF2_ERROR_UNSUPPORTED;
    }

    for (index = 0u; index < plan->record_count; ++index) {
        status = player_28178_stream_apply_record(
            machine, plan->player, &plan->records[index]
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    status = vf2_model2a_write_u32(
        machine, plan->player + UINT32_C(0x082c), plan->next_record_pointer
    );
    if (status == VF2_OK) {
        /* The accepted phase-0 path stores r8 back unchanged at 0x1b4c8. */
        status = vf2_model2a_write_u32(
            machine, plan->player + UINT32_C(0x01a4), plan->state_flags
        );
    }
    for (index = 0u; status == VF2_OK && index < 2u; ++index) {
        status = player_28178_stream_apply_tail_slot(
            machine, plan->player, (uint32_t)index, &plan->tail[index]
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[VF2_I960_G0_REGISTER + 4u] = plan->next_record_pointer;
    /* The second cmpinco compares zero with loop index one. */
    player_4b640_set_compare(cpu, VF2_I960_COMPARE_LESS);

    /* Three instructions have already executed. Add every remaining non-RET
     * instruction, then perform the real 0x1b530 RET architecturally. */
    if (plan->instruction_count < UINT64_C(4)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    cpu->ip = VF2_PLAYER_28178_STREAM_FINAL_RET;
    cpu->executed_instructions += plan->instruction_count - UINT64_C(4);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }
    ++cpu->executed_instructions;
    return cpu->ip == VF2_PLAYER_28178_STREAM_STOP
        ? VF2_OK : VF2_ERROR_UNSUPPORTED;
}

static vf2_status player_execute_28178_stream(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint64_t max_steps,
    uint64_t *instruction_count
)
{
    vf2_player_28178_stream_plan plan;
    vf2_status status = player_prepare_28178_stream(machine, cpu, &plan);

    if (status != VF2_OK) {
        return status;
    }
    if (max_steps != 0u && max_steps < plan.instruction_count) {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (instruction_count != NULL) {
        *instruction_count = plan.instruction_count;
    }
    return player_apply_28178_stream(machine, cpu, &plan);
}
