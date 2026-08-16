from pathlib import Path

p = Path('src/recovered/hybrid.c')
s = p.read_text()

anchor = '''/* Recover the observed shared-fighter 0x18644 port/flag corridors at both
 * dispatcher call sites. The controlled bit-4/8/14/16/6 probes also take the
 * high-result branch, including its 0x5b6 update; the remaining directions
 * stay explicitly guarded below. */
'''
helper = r'''static vf2_status hybrid_resolve_game_info_record(
    const vf2_model2a *machine,
    uint16_t handle,
    uint8_t target_type,
    uint32_t *record_address,
    uint32_t *instruction_count
)
{
    uint32_t cursor = 0u;
    uint32_t instructions = 4u;
    uint8_t type = 0u;
    uint8_t stride = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || record_address == NULL || instruction_count == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read_u32(
        machine,
        UINT32_C(0x0200d34c) +
            ((uint32_t)handle & UINT32_C(0x1fff)) * UINT32_C(4),
        &cursor
    );
    if (status != VF2_OK) {
        return status;
    }
    cursor += UINT32_C(8);

    for (uint32_t iteration = 0u; iteration < UINT32_C(256); ++iteration) {
        status = hybrid_read_u8(machine, cursor, &type);
        if (status != VF2_OK) {
            return status;
        }
        instructions += UINT32_C(2);
        if (type == target_type) {
            ++instructions;
            *record_address = cursor;
            *instruction_count = instructions;
            return VF2_OK;
        }

        ++instructions;
        if (type == 0u) {
            instructions += UINT32_C(2);
            *record_address = 0u;
            *instruction_count = instructions;
            return VF2_OK;
        }

        ++instructions;
        if (type == UINT8_C(8)) {
            instructions += UINT32_C(2);
            *record_address = 0u;
            *instruction_count = instructions;
            return VF2_OK;
        }

        status = hybrid_read_u8(
            machine, UINT32_C(0x0001b7f6) + (uint32_t)type, &stride
        );
        if (status != VF2_OK) {
            return status;
        }
        instructions += UINT32_C(3);
        if (stride == 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        cursor += (uint32_t)stride;
    }
    return VF2_ERROR_UNSUPPORTED;
}

static vf2_status hybrid_execute_game_info_type22_equal(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t fighter0,
    uint32_t fighter1,
    uint32_t *instruction_count
)
{
    uint32_t record = 0u;
    uint32_t resolver_instructions = 0u;
    uint32_t fighter0_flags = 0u;
    uint32_t fighter1_flags = 0u;
    uint32_t fighter0_state = 0u;
    uint32_t value = 0u;
    uint16_t handle = 0u;
    uint16_t record_value = 0u;
    uint8_t fighter_scale_byte = 0u;
    uint8_t record_scale_byte = 0u;
    int32_t scaled = 0;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || instruction_count == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = hybrid_read_u16(
        machine, fighter1 + UINT32_C(0x0000019c), &handle
    );
    if (status == VF2_OK) {
        status = hybrid_resolve_game_info_record(
            machine, handle, UINT8_C(5), &record, &resolver_instructions
        );
    }
    if (status != VF2_OK || record == 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(machine, record + UINT32_C(1), &record_value);
    }
    if ((handle & UINT16_C(0x8000)) != 0u) {
        record_value ^= UINT16_C(0x8000);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, fighter0 + UINT32_C(0x00000198),
            UINT32_C(0x11000000) + (uint32_t)record_value
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, fighter1 + UINT32_C(0x0000001c), &value
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, fighter0 + UINT32_C(0x0000001c), value
        );
    }

    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, fighter0, &fighter0_flags);
    }
    if (status == VF2_OK &&
        (fighter0_flags & (UINT32_C(1) << 2u)) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    if (status == VF2_OK) {
        status = hybrid_read_u8(
            machine, fighter0 + UINT32_C(0x0000069c), &fighter_scale_byte
        );
    }
    if (status == VF2_OK) {
        status = hybrid_read_u8(
            machine, record + UINT32_C(3), &record_scale_byte
        );
    }
    if (status == VF2_OK) {
        const int32_t fighter_scale = (int32_t)(int8_t)fighter_scale_byte;
        const int32_t record_scale = (int32_t)(int8_t)record_scale_byte;
        scaled = ((fighter_scale * 5 + 100) * record_scale) / 100;
        status = vf2_model2a_write(
            machine, fighter0 + UINT32_C(0x00000822),
            &(uint8_t){(uint8_t)scaled}, sizeof(uint8_t)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, fighter1 + UINT32_C(0x00000198),
            UINT32_C(0x10000000) + (uint32_t)handle
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, fighter1, &fighter1_flags);
    }
    if (status == VF2_OK) {
        fighter1_flags &= ~(UINT32_C(1) << 4u);
        status = vf2_model2a_write_u32(machine, fighter1, fighter1_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, fighter0 + UINT32_C(0x000001a4), &fighter0_state
        );
    }
    if (status == VF2_OK) {
        fighter0_state &= ~(UINT32_C(1) << 21u);
        status = vf2_model2a_write_u32(
            machine, fighter0 + UINT32_C(0x000001a4), fighter0_state
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, fighter1, &fighter1_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, fighter0, &fighter0_flags);
    }
    if (status == VF2_OK) {
        fighter0_flags = (fighter1_flags & (UINT32_C(1) << 10u)) != 0u
            ? fighter0_flags | (UINT32_C(1) << 6u)
            : fighter0_flags & ~(UINT32_C(1) << 6u);
        status = vf2_model2a_write_u32(machine, fighter0, fighter0_flags);
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[VF2_I960_G0_REGISTER] = record;
    cpu->registers[VF2_I960_G0_REGISTER + 1u] = UINT32_C(5);
    cpu->procedure_calls += UINT64_C(3);
    cpu->procedure_returns += UINT64_C(3);
    if (cpu->maximum_local_frame_depth < cpu->local_frame_depth + 2u) {
        cpu->maximum_local_frame_depth = cpu->local_frame_depth + 2u;
    }

    *instruction_count = UINT32_C(38) + resolver_instructions +
        ((handle & UINT16_C(0x8000)) != 0u ? UINT32_C(1) : 0u);
    return VF2_OK;
}

'''
if anchor not in s:
    raise SystemExit('helper insertion anchor missing')
s = s.replace(anchor, helper + anchor, 1)

old = '''    if (status == VF2_OK) {
        status = hybrid_read_u8(
            machine, fighter1 + UINT32_C(0x0000019f), &byte_value
        );
        if (status == VF2_OK && byte_value == UINT8_C(22)) {
            uint16_t progress = 0u;
            uint16_t target = 0u;
            status = hybrid_read_u16(
                machine, fighter0 + UINT32_C(0x000001aa), &progress
            );
            if (status == VF2_OK) {
                status = hybrid_read_u16(
                    machine, fighter0 + UINT32_C(0x0000080a), &target
                );
            }
            if (status == VF2_OK) {
                /* 0x18a30..0x18a3c: the common mismatch path returns
                 * immediately after comparing progress with target-1. */
                tail_instruction_delta += UINT32_C(4);
                if ((uint32_t)progress == (uint32_t)target - UINT32_C(1)) {
                    /* The equal case reaches the r9 threshold and may call
                     * 0x18bd4; keep that subpath explicit until recovered. */
                    status = VF2_ERROR_UNSUPPORTED;
                }
            }
        }
    }
'''
new = '''    if (status == VF2_OK) {
        status = hybrid_read_u8(
            machine, fighter1 + UINT32_C(0x0000019f), &byte_value
        );
        if (status == VF2_OK && byte_value == UINT8_C(22)) {
            uint16_t progress = 0u;
            uint16_t target = 0u;
            status = hybrid_read_u16(
                machine, fighter0 + UINT32_C(0x000001aa), &progress
            );
            if (status == VF2_OK) {
                status = hybrid_read_u16(
                    machine, fighter0 + UINT32_C(0x0000080a), &target
                );
            }
            if (status == VF2_OK &&
                (uint32_t)progress != (uint32_t)target - UINT32_C(1)) {
                /* 0x18a30..0x18a3c: the mismatch path returns immediately. */
                tail_instruction_delta += UINT32_C(4);
            } else if (status == VF2_OK) {
                /* Equal progress executes the r9 threshold pair. */
                tail_instruction_delta += UINT32_C(6);
                if ((int32_t)r9 <= (int32_t)UINT32_C(0x3fb33333)) {
                    uint32_t helper_instructions = 0u;
                    status = hybrid_execute_game_info_type22_equal(
                        machine, cpu, fighter0, fighter1, &helper_instructions
                    );
                    if (status == VF2_OK) {
                        /* Account for the 0x18a4c CALL itself; the helper
                         * count includes 0x18bd4, 0x1ab34 and 0x18b58. */
                        tail_instruction_delta += UINT32_C(1) +
                            helper_instructions;
                    }
                }
            }
        }
    }
'''
if old not in s:
    raise SystemExit('type22 block anchor missing')
s = s.replace(old, new, 1)
p.write_text(s)
