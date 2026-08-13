#define VF2_PLAYER_4B640_ENTRY UINT32_C(0x0004b640)
#define VF2_PLAYER_4B640_RETURN UINT32_C(0x0001440c)
#define VF2_PLAYER_4B640_STOP UINT32_C(0x00014414)
#define VF2_PLAYER_4B640_RECORD_LIMIT UINT32_C(0x0055c2f0)
#define VF2_PLAYER_4B640_ACTIVE UINT32_C(0x00550000)
#define VF2_PLAYER_4B640_CLEAR0 UINT32_C(0x0055000c)
#define VF2_PLAYER_4B640_CLEAR1 UINT32_C(0x00550080)
#define VF2_PLAYER_4B640_CLEAR2 UINT32_C(0x005500f4)
#define VF2_PLAYER_4B640_MARKER_ROM UINT32_C(0x0004ad74)
#define VF2_PLAYER_4B640_MARKER_DATA UINT32_C(0x02300000)
#define VF2_PLAYER_4B640_MODE_TABLE UINT32_C(0x020062d4)
#define VF2_PLAYER_4B640_STATE_TABLE UINT32_C(0x02006308)
#define VF2_PLAYER_4B640_RECORD0 UINT32_C(0x005501c8)
#define VF2_PLAYER_4B640_RECORD0_ALT UINT32_C(0x005501e8)
#define VF2_PLAYER_4B640_RECORD1 UINT32_C(0x00550208)
#define VF2_PLAYER_4B640_RECORD1_ALT UINT32_C(0x00550228)

typedef struct vf2_player_4b604_state {
    uint32_t pointer;
    uint32_t table0;
    uint32_t table1;
    uint32_t next_state;
    int16_t countdown;
    int16_t target0;
    int16_t target1;
    uint8_t active;
} vf2_player_4b604_state;

typedef struct vf2_player_4b640_publish {
    uint32_t record;
    uint32_t selector;
    uint32_t flags;
    uint32_t metric;
    uint64_t instruction_count;
    uint8_t active;
    uint8_t publish_record;
    uint8_t clear_globals;
} vf2_player_4b640_publish;

typedef struct vf2_player_4b640_plan {
    uint32_t player;
    uint32_t final_g[5];
    uint32_t selected_value;
    uint64_t instruction_count;
    uint64_t nested_calls;
    uint64_t nested_returns;
    vf2_i960_compare_result compare_result;
    vf2_player_4b604_state initial_reload;
    vf2_player_4b604_state final_reload;
    vf2_player_4b640_publish first_publish;
    vf2_player_4b640_publish second_publish;
    int16_t current0;
    int16_t current1;
    int16_t target0;
    int16_t target1;
    int16_t next0;
    int16_t next1;
    uint16_t countdown;
    uint16_t next_countdown;
    uint8_t write_countdown;
    uint8_t write_interpolation;
    uint8_t write_selected;
    uint8_t update_compare;
} vf2_player_4b640_plan;

static void player_4b640_set_compare(
    vf2_i960_cpu *cpu,
    vf2_i960_compare_result compare
)
{
    uint32_t bits = 0u;

    if (compare == VF2_I960_COMPARE_LESS) {
        bits = UINT32_C(4);
    } else if (compare == VF2_I960_COMPARE_EQUAL) {
        bits = UINT32_C(2);
    } else if (compare == VF2_I960_COMPARE_GREATER) {
        bits = UINT32_C(1);
    }
    cpu->compare_result = compare;
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | bits;
}

static vf2_status player_4b640_read_i16(
    const vf2_model2a *machine,
    uint32_t address,
    int16_t *value
)
{
    uint16_t raw = 0u;
    vf2_status status = VF2_OK;

    if (value == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = player_tail_read_u16(machine, address, &raw);
    if (status == VF2_OK) {
        *value = (int16_t)raw;
    }
    return status;
}

static vf2_status player_4b640_read_state(
    const vf2_model2a *machine,
    uint32_t pointer,
    vf2_player_4b604_state *state
)
{
    uint16_t raw = 0u;
    vf2_status status = VF2_OK;

    if (state == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(state, 0, sizeof(*state));
    state->pointer = pointer;
    if (pointer == 0u) {
        return VF2_OK;
    }
    status = player_tail_read_u16(machine, pointer, &raw);
    if (status == VF2_OK) {
        state->countdown = (int16_t)raw;
        status = player_tail_read_u16(machine, pointer + UINT32_C(4), &raw);
    }
    if (status == VF2_OK) {
        state->target0 = (int16_t)raw;
        status = player_tail_read_u16(machine, pointer + UINT32_C(6), &raw);
    }
    if (status == VF2_OK) {
        state->target1 = (int16_t)raw;
        status = vf2_model2a_read_u32(
            machine, pointer + UINT32_C(8), &state->table0
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, pointer + UINT32_C(0x0c), &state->table1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, pointer + UINT32_C(0x10), &state->next_state
        );
    }
    if (status == VF2_OK) {
        state->active = 1u;
    }
    return status;
}

static vf2_status player_4b640_preflight_state_write(
    const vf2_model2a *machine,
    uint32_t player
)
{
    uint32_t ignored = 0u;
    uint16_t ignored16 = 0u;
    vf2_status status = player_tail_read_u16(
        machine, player + UINT32_C(0x6b0), &ignored16
    );

    if (status == VF2_OK) {
        status = player_tail_read_u16(
            machine, player + UINT32_C(0x6ac), &ignored16
        );
    }
    if (status == VF2_OK) {
        status = player_tail_read_u16(
            machine, player + UINT32_C(0x6ae), &ignored16
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player + UINT32_C(0x76c), &ignored
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player + UINT32_C(0x6a0), &ignored
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player + UINT32_C(0x6a4), &ignored
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player + UINT32_C(0x6b8), &ignored
        );
    }
    return status;
}

static vf2_status player_4b640_apply_state(
    vf2_model2a *machine,
    uint32_t player,
    const vf2_player_4b604_state *state
)
{
    vf2_status status = VF2_OK;

    if (state == NULL || state->active == 0u) {
        return VF2_OK;
    }
    status = vf2_model2a_write_u32(
        machine, player + UINT32_C(0x76c), state->pointer
    );
    if (status == VF2_OK) {
        status = player_tail_write_u16(
            machine, player + UINT32_C(0x6b0),
            (uint16_t)state->countdown
        );
    }
    if (status == VF2_OK) {
        status = player_tail_write_u16(
            machine, player + UINT32_C(0x6ac),
            (uint16_t)state->target0
        );
    }
    if (status == VF2_OK) {
        status = player_tail_write_u16(
            machine, player + UINT32_C(0x6ae),
            (uint16_t)state->target1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, player + UINT32_C(0x6a0), state->table0
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, player + UINT32_C(0x6a4), state->table1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, player + UINT32_C(0x6b8), state->next_state
        );
    }
    return status;
}

static vf2_status player_4b640_prepare_publish(
    vf2_model2a *machine,
    uint32_t record,
    uint32_t selector,
    uint32_t flags,
    uint32_t metric,
    vf2_player_4b640_publish *publish
)
{
    uint32_t marker_rom = 0u;
    uint32_t marker_data = 0u;
    uint16_t old_metric = 0u;
    uint16_t old_selector = 0u;
    uint16_t limit = 0u;
    uint32_t ignored = 0u;
    vf2_status status = VF2_OK;

    if (publish == NULL || selector > UINT32_C(0x56)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(publish, 0, sizeof(*publish));
    publish->active = 1u;
    publish->record = record;
    publish->selector = selector;
    publish->flags = flags;
    publish->metric = metric;

    status = vf2_model2a_read_u32(
        machine, VF2_PLAYER_4B640_MARKER_ROM, &marker_rom
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_PLAYER_4B640_MARKER_DATA, &marker_data
        );
    }
    if (status != VF2_OK || marker_rom != marker_data) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = player_tail_read_u16(machine, record + UINT32_C(8), &old_metric);
    if (status == VF2_OK) {
        status = player_tail_read_u16(machine, record, &old_selector);
    }
    if (status == VF2_OK) {
        status = player_tail_read_u16(
            machine, VF2_PLAYER_4B640_RECORD_LIMIT, &limit
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    /* Both 0x4b7b0/0x4b7e8 wrappers set flag bit 4 before publishing, so the
     * helper always refreshes record+8 before deciding whether a full publish
     * is required. */
    publish->publish_record =
        (uint32_t)old_metric != metric ||
        (uint32_t)old_selector != selector;
    publish->clear_globals =
        publish->publish_record != 0u && UINT32_C(2) > (uint32_t)limit;

    /* 0x4b9b8 body, including its RET but excluding the wrapper's CALL. */
    if ((uint32_t)old_metric != metric) {
        publish->instruction_count =
            publish->clear_globals != 0u ? UINT64_C(23) : UINT64_C(19);
    } else if ((uint32_t)old_selector == selector) {
        publish->instruction_count = UINT64_C(12);
    } else {
        publish->instruction_count =
            publish->clear_globals != 0u ? UINT64_C(25) : UINT64_C(21);
    }

    /* Preflight every destination touched by the wrapper/publisher. */
    status = vf2_model2a_read_u32(machine, record + UINT32_C(8), &ignored);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, record + UINT32_C(0x10), &ignored);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, VF2_PLAYER_4B640_ACTIVE, &ignored);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, VF2_PLAYER_4B640_CLEAR0, &ignored);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, VF2_PLAYER_4B640_CLEAR1, &ignored);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, VF2_PLAYER_4B640_CLEAR2, &ignored);
    }
    return status;
}

static vf2_status player_4b640_apply_publish(
    vf2_model2a *machine,
    const vf2_player_4b640_publish *publish
)
{
    vf2_status status = VF2_OK;

    if (publish == NULL || publish->active == 0u) {
        return VF2_OK;
    }
    /* Wrapper store before the nested publisher. */
    status = player_tail_write_u16(
        machine, publish->record + UINT32_C(0x0a), UINT16_C(1)
    );
    if (status == VF2_OK) {
        status = player_tail_write_u16(
            machine, publish->record + UINT32_C(8),
            (uint16_t)publish->metric
        );
    }
    if (status != VF2_OK || publish->publish_record == 0u) {
        return status;
    }

    status = vf2_model2a_write_u32(
        machine, VF2_PLAYER_4B640_ACTIVE, UINT32_C(1)
    );
    if (status == VF2_OK) {
        status = player_tail_write_u16(
            machine, publish->record, (uint16_t)publish->selector
        );
    }
    if (status == VF2_OK) {
        status = player_tail_write_u16(
            machine, publish->record + UINT32_C(2), UINT16_C(0xffff)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, publish->record + UINT32_C(0x10), publish->flags
        );
    }
    if (status == VF2_OK) {
        status = player_tail_write_u16(
            machine, publish->record + UINT32_C(0x1c), UINT16_C(2)
        );
    }
    if (status == VF2_OK && publish->clear_globals != 0u) {
        status = vf2_model2a_write_u32(
            machine, VF2_PLAYER_4B640_CLEAR0, 0u
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, VF2_PLAYER_4B640_CLEAR1, 0u
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, VF2_PLAYER_4B640_CLEAR2, 0u
            );
        }
    }
    return status;
}

static vf2_status player_4b640_prepare_table(
    vf2_model2a *machine,
    uint32_t player,
    uint32_t table,
    int16_t value,
    uint32_t *selector,
    uint32_t *metric,
    uint32_t *selected
)
{
    uint16_t raw_selector = 0u;
    uint16_t scale = 0u;
    uint32_t base = 0u;
    uint32_t product = 0u;
    uint32_t quotient = 0u;
    uint32_t flags = 0u;
    uint8_t index = 0u;
    uint8_t side0 = 0u;
    uint8_t side1 = 0u;
    vf2_status status = VF2_OK;

    if (selector == NULL || metric == NULL || selected == NULL || table == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = player_tail_read_u16(machine, table, &raw_selector);
    if (status == VF2_OK) {
        status = player_tail_read_u16(machine, table + UINT32_C(2), &scale);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, table + UINT32_C(4), &base
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    *selector = (uint32_t)(int32_t)(int16_t)raw_selector;
    product = (uint32_t)value * (uint32_t)scale;
    quotient = product / UINT32_C(1001);
    status = player_tail_read_u8(
        machine, table + UINT32_C(8) + quotient, &index
    );
    if (status == VF2_OK) {
        status = player_tail_read_u8(
            machine, player + UINT32_C(0x1b1), &side1
        );
    }
    if (status == VF2_OK) {
        status = player_tail_read_u8(
            machine, player + UINT32_C(0x1b0), &side0
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player + UINT32_C(0x700), &flags
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if (side1 != side0) {
        base += UINT32_C(4);
    }
    if ((flags & (UINT32_C(1) << 4u)) != 0u) {
        base += UINT32_C(8);
    }
    status = vf2_model2a_read_u32(
        machine, base + (uint32_t)index * UINT32_C(16), selected
    );
    if (status == VF2_OK) {
        *metric = quotient;
    }
    return status;
}

static vf2_status player_prepare_4b640(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_player_4b640_plan *plan
)
{
    uint32_t state_code = 0u;
    uint32_t current_state = 0u;
    uint32_t expected_state = 0u;
    uint32_t reload_pointer = 0u;
    uint32_t runtime_flags = 0u;
    uint32_t object_pointer = 0u;
    uint32_t table0 = 0u;
    uint32_t table1 = 0u;
    uint32_t next_state = 0u;
    uint32_t selector0 = 0u;
    uint32_t selector1 = 0u;
    uint32_t metric0 = 0u;
    uint32_t metric1 = 0u;
    uint32_t selected = 0u;
    uint16_t raw = 0u;
    uint8_t side = 0u;
    uint8_t mode = 0u;
    uint8_t submode = 0u;
    uint8_t mode_gate = 0u;
    uint8_t object_mode = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || plan == NULL ||
        cpu->ip != VF2_PLAYER_4B640_ENTRY ||
        cpu->local_frame_depth == 0u ||
        cpu->local_frames[cpu->local_frame_depth - 1u].registers[2] !=
            VF2_PLAYER_4B640_RETURN) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(plan, 0, sizeof(*plan));
    plan->player = cpu->registers[VF2_I960_G0_REGISTER + 7u];
    if (plan->player == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    plan->final_g[0] = cpu->registers[VF2_I960_G0_REGISTER];
    plan->final_g[1] = cpu->registers[VF2_I960_G0_REGISTER + 1u];
    plan->final_g[2] = cpu->registers[VF2_I960_G0_REGISTER + 2u];
    plan->final_g[3] = cpu->registers[VF2_I960_G0_REGISTER + 3u];
    plan->final_g[4] = cpu->registers[VF2_I960_G0_REGISTER + 4u];

    status = player_tail_read_u16(
        machine, plan->player + UINT32_C(0x770), &raw
    );
    if (status != VF2_OK) {
        return status;
    }
    state_code = raw;
    plan->instruction_count += UINT64_C(2);
    if (state_code == UINT32_C(25)) {
        status = player_tail_read_u8(
            machine, plan->player + UINT32_C(0x1b1), &object_mode
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, plan->player + UINT32_C(0x76c), &current_state
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine,
                VF2_PLAYER_4B640_MODE_TABLE +
                    (uint32_t)object_mode * UINT32_C(4),
                &expected_state
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        plan->instruction_count += UINT64_C(4);
        if (current_state != expected_state) {
            status = vf2_model2a_read_u32(
                machine,
                VF2_PLAYER_4B640_STATE_TABLE +
                    (uint32_t)object_mode * UINT32_C(4),
                &reload_pointer
            );
            if (status == VF2_OK) {
                status = player_4b640_read_state(
                    machine, reload_pointer, &plan->initial_reload
                );
            }
            if (status == VF2_OK) {
                status = player_4b640_preflight_state_write(
                    machine, plan->player
                );
            }
            if (status != VF2_OK) {
                return status;
            }
            plan->final_g[0] = reload_pointer;
            plan->instruction_count += UINT64_C(2) +
                (reload_pointer == 0u ? UINT64_C(2) : UINT64_C(15));
            ++plan->nested_calls;
            ++plan->nested_returns;
        }
    }

    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00500068), &runtime_flags
    );
    if (status != VF2_OK) {
        return status;
    }
    plan->instruction_count += UINT64_C(2);
    if ((runtime_flags & (UINT32_C(1) << 24u)) != 0u) {
        goto finish;
    }

    status = player_tail_read_u8(machine, UINT32_C(0x0050002b), &mode);
    if (status != VF2_OK) {
        return status;
    }
    plan->instruction_count += UINT64_C(3);
    if (mode == UINT8_C(8)) {
        goto finish;
    }
    plan->instruction_count += UINT64_C(2);
    if (mode == UINT8_C(9)) {
        status = player_tail_read_u8(machine, UINT32_C(0x00500031), &submode);
        if (status != VF2_OK) {
            return status;
        }
        plan->instruction_count += UINT64_C(3);
        if (submode < UINT8_C(6)) {
            goto finish;
        }
        plan->instruction_count += UINT64_C(2);
        if (submode >= UINT8_C(16)) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x00500814), &object_pointer
            );
            if (status == VF2_OK && object_pointer != 0u) {
                status = player_tail_read_u8(
                    machine, object_pointer + UINT32_C(0x40), &mode_gate
                );
            }
            if (status != VF2_OK || object_pointer == 0u) {
                return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
            }
            plan->instruction_count += UINT64_C(3);
            if (mode_gate != UINT8_C(9)) {
                ++plan->instruction_count;
                if (mode_gate != UINT8_C(10)) {
                    goto countdown;
                }
            }
            status = player_tail_read_u8(
                machine, UINT32_C(0x00500064), &mode_gate
            );
            if (status != VF2_OK) {
                return status;
            }
            plan->instruction_count += UINT64_C(3);
            if (mode_gate == UINT8_C(6)) {
                goto finish;
            }
        }
    }

countdown:
    if (plan->initial_reload.active != 0u) {
        plan->countdown = (uint16_t)plan->initial_reload.countdown;
        plan->target0 = plan->initial_reload.target0;
        plan->target1 = plan->initial_reload.target1;
        table0 = plan->initial_reload.table0;
        table1 = plan->initial_reload.table1;
        next_state = plan->initial_reload.next_state;
        status = player_4b640_read_i16(
            machine, plan->player + UINT32_C(0x6a8), &plan->current0
        );
        if (status == VF2_OK) {
            status = player_4b640_read_i16(
                machine, plan->player + UINT32_C(0x6aa), &plan->current1
            );
        }
    } else {
        status = player_tail_read_u16(
            machine, plan->player + UINT32_C(0x6b0), &plan->countdown
        );
        if (status == VF2_OK) {
            status = player_4b640_read_i16(
                machine, plan->player + UINT32_C(0x6a8), &plan->current0
            );
        }
        if (status == VF2_OK) {
            status = player_4b640_read_i16(
                machine, plan->player + UINT32_C(0x6aa), &plan->current1
            );
        }
        if (status == VF2_OK) {
            status = player_4b640_read_i16(
                machine, plan->player + UINT32_C(0x6ac), &plan->target0
            );
        }
        if (status == VF2_OK) {
            status = player_4b640_read_i16(
                machine, plan->player + UINT32_C(0x6ae), &plan->target1
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, plan->player + UINT32_C(0x6a0), &table0
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, plan->player + UINT32_C(0x6a4), &table1
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, plan->player + UINT32_C(0x6b8), &next_state
            );
        }
    }
    if (status != VF2_OK) {
        return status;
    }

    plan->instruction_count += UINT64_C(3);
    plan->update_compare = 1u;
    plan->compare_result = plan->countdown == 0u
        ? VF2_I960_COMPARE_EQUAL : VF2_I960_COMPARE_LESS;
    if (plan->countdown == 0u) {
        goto finish;
    }

    plan->write_countdown = 1u;
    plan->next_countdown = (uint16_t)(plan->countdown - UINT16_C(1));
    plan->instruction_count += UINT64_C(2);
    if (plan->next_countdown == 0u) {
        plan->next0 = plan->target0;
        plan->next1 = plan->target1;
        plan->instruction_count += UINT64_C(2);
    } else {
        const int32_t delta0 = (int32_t)plan->target0 - (int32_t)plan->current0;
        const int32_t delta1 = (int32_t)plan->target1 - (int32_t)plan->current1;

        plan->next0 = (int16_t)(
            (int32_t)plan->current0 +
            delta0 / (int32_t)plan->next_countdown
        );
        plan->next1 = (int16_t)(
            (int32_t)plan->current1 +
            delta1 / (int32_t)plan->next_countdown
        );
        plan->instruction_count += UINT64_C(11);
    }
    plan->write_interpolation = 1u;
    plan->instruction_count += UINT64_C(3);

    status = player_4b640_prepare_table(
        machine, plan->player, table0, plan->next0,
        &selector0, &metric0, &selected
    );
    if (status != VF2_OK) {
        return status;
    }
    plan->selected_value = selected;
    plan->write_selected = 1u;
    plan->instruction_count += UINT64_C(16);
    {
        uint8_t side0 = 0u;
        uint8_t side1 = 0u;
        uint32_t flags = 0u;

        status = player_tail_read_u8(
            machine, plan->player + UINT32_C(0x1b1), &side1
        );
        if (status == VF2_OK) {
            status = player_tail_read_u8(
                machine, plan->player + UINT32_C(0x1b0), &side0
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, plan->player + UINT32_C(0x700), &flags
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        if (side1 != side0) {
            ++plan->instruction_count;
        }
        if ((flags & (UINT32_C(1) << 4u)) != 0u) {
            ++plan->instruction_count;
        }
    }
    plan->final_g[2] = metric0;
    plan->final_g[0] = selector0;
    if (selector0 == UINT32_MAX) {
        goto after_tables;
    }

    status = player_tail_read_u8(
        machine, plan->player + UINT32_C(4), &side
    );
    if (status != VF2_OK) {
        return status;
    }
    plan->instruction_count += UINT64_C(2);
    plan->final_g[1] = ((uint32_t)side | UINT32_C(0x10)) & ~UINT32_C(0x20);
    plan->final_g[2] = side == 0u
        ? VF2_PLAYER_4B640_RECORD0 : VF2_PLAYER_4B640_RECORD0_ALT;
    plan->final_g[3] = UINT32_C(2);
    plan->final_g[4] = metric0;
    status = player_4b640_prepare_publish(
        machine, plan->final_g[2], selector0,
        plan->final_g[1], metric0, &plan->first_publish
    );
    if (status != VF2_OK) {
        return status;
    }
    plan->instruction_count += UINT64_C(1) +
        (side == 0u ? UINT64_C(11) : UINT64_C(10)) +
        plan->first_publish.instruction_count;
    plan->nested_calls += UINT64_C(2);
    plan->nested_returns += UINT64_C(2);
    if (plan->first_publish.clear_globals != 0u) {
        plan->final_g[0] = 0u;
    }

    status = player_4b640_prepare_table(
        machine, plan->player, table1, plan->next1,
        &selector1, &metric1, &selected
    );
    if (status != VF2_OK) {
        return status;
    }
    plan->instruction_count += UINT64_C(7);
    plan->final_g[0] = selector1;
    plan->final_g[1] = (uint32_t)side | UINT32_C(0x30);
    plan->final_g[2] = side == 0u
        ? VF2_PLAYER_4B640_RECORD1 : VF2_PLAYER_4B640_RECORD1_ALT;
    plan->final_g[3] = UINT32_C(2);
    plan->final_g[4] = metric1;
    status = player_4b640_prepare_publish(
        machine, plan->final_g[2], selector1,
        plan->final_g[1], metric1, &plan->second_publish
    );
    if (status != VF2_OK) {
        return status;
    }
    plan->instruction_count +=
        (side == 0u ? UINT64_C(11) : UINT64_C(10)) +
        plan->second_publish.instruction_count;
    plan->nested_calls += UINT64_C(2);
    plan->nested_returns += UINT64_C(2);
    if (plan->second_publish.clear_globals != 0u) {
        plan->final_g[0] = 0u;
    }

after_tables:
    ++plan->instruction_count;
    if (plan->next_countdown == 0u) {
        status = player_4b640_read_state(
            machine, next_state, &plan->final_reload
        );
        if (status == VF2_OK) {
            status = player_4b640_preflight_state_write(
                machine, plan->player
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        plan->final_g[0] = next_state;
        plan->instruction_count += UINT64_C(2) +
            (next_state == 0u ? UINT64_C(2) : UINT64_C(15));
        ++plan->nested_calls;
        ++plan->nested_returns;
    }

finish:
    /* Parent RET followed by the caller's unconditional 0x1440c -> 0x14414. */
    plan->instruction_count += UINT64_C(2);
    return VF2_OK;
}

static vf2_status player_apply_4b640(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    const vf2_player_4b640_plan *plan
)
{
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || plan == NULL ||
        cpu->ip != VF2_PLAYER_4B640_ENTRY ||
        plan->player != cpu->registers[VF2_I960_G0_REGISTER + 7u] ||
        cpu->local_frame_depth == 0u ||
        cpu->local_frames[cpu->local_frame_depth - 1u].registers[2] !=
            VF2_PLAYER_4B640_RETURN || plan->instruction_count < UINT64_C(2)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = player_4b640_apply_state(
        machine, plan->player, &plan->initial_reload
    );
    if (status == VF2_OK && plan->write_countdown != 0u) {
        status = player_tail_write_u16(
            machine, plan->player + UINT32_C(0x6b0), plan->next_countdown
        );
    }
    if (status == VF2_OK && plan->write_interpolation != 0u) {
        status = player_tail_write_u16(
            machine, plan->player + UINT32_C(0x6a8),
            (uint16_t)plan->next0
        );
        if (status == VF2_OK) {
            status = player_tail_write_u16(
                machine, plan->player + UINT32_C(0x6aa),
                (uint16_t)plan->next1
            );
        }
    }
    if (status == VF2_OK && plan->write_selected != 0u) {
        status = vf2_model2a_write_u32(
            machine, plan->player + UINT32_C(0x48), plan->selected_value
        );
    }
    if (status == VF2_OK) {
        status = player_4b640_apply_publish(machine, &plan->first_publish);
    }
    if (status == VF2_OK) {
        status = player_4b640_apply_publish(machine, &plan->second_publish);
    }
    if (status == VF2_OK) {
        status = player_4b640_apply_state(
            machine, plan->player, &plan->final_reload
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[VF2_I960_G0_REGISTER] = plan->final_g[0];
    cpu->registers[VF2_I960_G0_REGISTER + 1u] = plan->final_g[1];
    cpu->registers[VF2_I960_G0_REGISTER + 2u] = plan->final_g[2];
    cpu->registers[VF2_I960_G0_REGISTER + 3u] = plan->final_g[3];
    cpu->registers[VF2_I960_G0_REGISTER + 4u] = plan->final_g[4];
    if (plan->update_compare != 0u) {
        player_4b640_set_compare(cpu, plan->compare_result);
    }
    cpu->procedure_calls += plan->nested_calls;
    cpu->procedure_returns += plan->nested_returns;
    if (cpu->maximum_local_frame_depth <
        cpu->local_frame_depth + (plan->nested_calls >= UINT64_C(2) ? 2u : 1u)) {
        cpu->maximum_local_frame_depth =
            cpu->local_frame_depth +
            (plan->nested_calls >= UINT64_C(2) ? 2u : 1u);
    }

    cpu->executed_instructions += plan->instruction_count - UINT64_C(2);
    cpu->ip = UINT32_C(0x0004b7ac);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != VF2_PLAYER_4B640_RETURN) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    ++cpu->executed_instructions;
    cpu->ip = VF2_PLAYER_4B640_STOP;
    ++cpu->executed_instructions;
    return VF2_OK;
}

static vf2_status player_execute_4b640_general(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint64_t max_steps,
    uint64_t *instruction_count
)
{
    vf2_player_4b640_plan plan;
    vf2_status status = player_prepare_4b640(machine, cpu, &plan);

    if (status != VF2_OK) {
        return status;
    }
    if (max_steps != 0u && max_steps < plan.instruction_count) {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (instruction_count != NULL) {
        *instruction_count = plan.instruction_count;
    }
    return player_apply_4b640(machine, cpu, &plan);
}
