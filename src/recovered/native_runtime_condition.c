#define vf2_native_runtime_step vf2_native_runtime_step_condition_impl
#include "native_runtime_condition_impl.c"
#undef vf2_native_runtime_step

#define VF2_SELECTOR2_BODY_ENTRY UINT32_C(0x0000ab0c)
#define VF2_SELECTOR2_BODY_RETURN UINT32_C(0x0000a6f4)
#define VF2_SELECTOR2_DISPATCH_RETURN UINT32_C(0x0000a010)
#define VF2_SELECTOR2_POST_INIT_ENTRY UINT32_C(0x00043fd0)
#define VF2_SELECTOR2_QUEUE_ENTRY UINT32_C(0x00043888)
#define VF2_SELECTOR2_INSTRUCTIONS UINT64_C(232)

static vf2_status selector2_queue_helper(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    uint32_t selector_mask = 0u;
    uint32_t base = 0u;
    uint32_t runtime_flags = 0u;
    uint8_t mode = 0u;
    uint8_t count = 0u;
    uint8_t index = 0u;
    vf2_status status = VF2_OK;

    cpu->registers[13] = UINT32_C(0x00ae101f);
    if (cpu->registers[VF2_I960_G0_REGISTER] != cpu->registers[13]) {
        cpu->registers[3] = UINT32_C(0x0000000c);
        status = vf2_model2a_read_u32(
            machine, VF2_FRAME_SELECTOR_MASK, &selector_mask
        );
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[13] = selector_mask;
        cpu->registers[3] &= cpu->registers[13];
        if (cpu->registers[3] != 0u) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x0050016c), &base
            );
            if (status == VF2_OK) {
                status = vf2_model2a_read(
                    machine,
                    base + UINT32_C(0x3351),
                    &mode,
                    sizeof(mode)
                );
            }
            if (status != VF2_OK) {
                return status;
            }
            cpu->registers[15] = (uint32_t)mode;
            if ((mode & UINT8_C(1)) != 0u) {
                return vf2_i960_cpu_return_procedure(cpu, machine);
            }
        }

        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500068), &runtime_flags
        );
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[15] = runtime_flags;
        if ((runtime_flags & (UINT32_C(1) << 20u)) != 0u) {
            cpu->registers[13] = UINT32_C(0x00ff0000);
            cpu->registers[3] =
                cpu->registers[VF2_I960_G0_REGISTER] & cpu->registers[13];
            cpu->registers[13] = UINT32_C(0x009e0000);
            if (cpu->registers[3] == cpu->registers[13]) {
                cpu->registers[3] = UINT32_C(1) << 17u;
                cpu->registers[VF2_I960_G0_REGISTER] -= cpu->registers[3];
            }
        }
    }

    cpu->registers[6] = UINT32_C(0x00e80004);
    cpu->registers[3] = UINT32_C(33);
    status = vf2_model2a_write_u32(
        machine, cpu->registers[6], cpu->registers[3]
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, cpu->registers[6], cpu->registers[3]
        );
    }
    cpu->registers[3] = UINT32_C(16);
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x00504001), &count, sizeof(count)
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[5] = (uint32_t)count;
    if (count < UINT8_C(16)) {
        ++count;
        cpu->registers[5] = (uint32_t)count;
        status = vf2_model2a_write(
            machine, UINT32_C(0x00504001), &count, sizeof(count)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, UINT32_C(0x00504003), &index, sizeof(index)
            );
        }
        if (status == VF2_OK) {
            cpu->registers[3] = (uint32_t)index;
            status = vf2_model2a_write_u32(
                machine,
                UINT32_C(0x00504020) + (uint32_t)index * UINT32_C(4),
                cpu->registers[VF2_I960_G0_REGISTER]
            );
        }
        cpu->registers[4] = UINT32_C(15);
        cpu->registers[3] =
            ((uint32_t)index + UINT32_C(1)) & cpu->registers[4];
        index = (uint8_t)cpu->registers[3];
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x00504003), &index, sizeof(index)
            );
        }
    }
    cpu->registers[3] = UINT32_C(0x00000421);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, cpu->registers[6], cpu->registers[3]
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, cpu->registers[6], cpu->registers[3]
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    return vf2_i960_cpu_return_procedure(cpu, machine);
}

static vf2_status selector2_post_init_helper(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    static const uint32_t return_addresses[4] = {
        UINT32_C(0x00044028), UINT32_C(0x00044030),
        UINT32_C(0x00044038), UINT32_C(0x00044040)
    };
    uint32_t values[4];
    uint32_t runtime_flags = 0u;
    size_t index = 0u;
    vf2_status status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00500068), &runtime_flags
    );

    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[15] = runtime_flags;
    values[0] = UINT32_C(0x00cd74f8);
    values[1] = (runtime_flags & (UINT32_C(1) << 20u)) != 0u
        ? UINT32_C(0x00ca7af8) : UINT32_C(0x00ca7ef8);
    values[2] = (runtime_flags & (UINT32_C(1) << 20u)) != 0u
        ? UINT32_C(0x00cb7bf8) : UINT32_C(0x00cb7ff8);
    values[3] = (runtime_flags & (UINT32_C(1) << 20u)) != 0u
        ? UINT32_C(0x00ad2101) : UINT32_C(0x00ad2100);

    cpu->registers[3] = values[0];
    cpu->registers[4] = values[1];
    cpu->registers[5] = values[2];
    cpu->registers[6] = values[3];
    for (index = 0u; index < 4u; ++index) {
        cpu->registers[VF2_I960_G0_REGISTER] = values[index];
        status = vf2_i960_cpu_enter_procedure(
            cpu, VF2_SELECTOR2_QUEUE_ENTRY, return_addresses[index]
        );
        if (status == VF2_OK) {
            status = selector2_queue_helper(machine, cpu);
        }
        if (status != VF2_OK || cpu->ip != return_addresses[index]) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
    }
    return vf2_i960_cpu_return_procedure(cpu, machine);
}

static vf2_status execute_selector2_body(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    size_t *bytes_written
)
{
    uint32_t base = 0u;
    uint32_t control = 0u;
    uint32_t value = 0u;
    uint32_t runtime_flags = 0u;
    uint8_t profile_mode = 0u;
    uint8_t sound_rate = 0u;
    uint8_t control_byte = 0u;
    uint8_t zero = 0u;
    uint8_t one = 1u;
    uint8_t selector = 0u;
    vf2_status status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00500068), &runtime_flags
    );

    cpu->registers[15] = runtime_flags;
    if (status == VF2_OK) {
        runtime_flags &= ~(UINT32_C(1) << 4u);
        cpu->registers[15] = runtime_flags;
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500068), runtime_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500068), &runtime_flags
        );
    }
    cpu->registers[15] = runtime_flags;
    if (status == VF2_OK) {
        runtime_flags &= ~(UINT32_C(1) << 15u);
        cpu->registers[15] = runtime_flags;
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500068), runtime_flags
        );
    }
    if (status == VF2_OK) {
        cpu->registers[15] = 0u;
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500070), 0u
        );
    }
    if (status == VF2_OK) {
        cpu->registers[15] = 0u;
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500074), 0u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x0050008f), &profile_mode, sizeof(profile_mode)
        );
    }
    cpu->registers[3] = (uint32_t)profile_mode;
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x00500054), &profile_mode, sizeof(profile_mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x00500067), &profile_mode, sizeof(profile_mode)
        );
    }
    if (status == VF2_OK) {
        cpu->registers[15] = 0u;
        status = vf2_model2a_write(
            machine, UINT32_C(0x00500081), &zero, sizeof(zero)
        );
    }
    if (status == VF2_OK) {
        cpu->registers[15] = 0u;
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050008d), &zero, sizeof(zero)
        );
    }
    if (status == VF2_OK) {
        cpu->registers[15] = 0u;
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050008e), &zero, sizeof(zero)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050016c), &base
        );
    }
    cpu->registers[15] = base;
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3351), &profile_mode,
            sizeof(profile_mode)
        );
    }
    cpu->registers[15] = (uint32_t)profile_mode;
    if (status == VF2_OK && (profile_mode & UINT8_C(0x20)) == 0u) {
        cpu->registers[15] = 0u;
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050005b), &zero, sizeof(zero)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050083c), &control
        );
    }
    cpu->registers[3] = control;
    if (status == VF2_OK) {
        cpu->registers[15] = 0u;
        status = vf2_model2a_write(
            machine, control + UINT32_C(6), &zero, sizeof(zero)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500840), &control
        );
    }
    cpu->registers[3] = control;
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x00033e56), &control_byte,
            sizeof(control_byte)
        );
    }
    cpu->registers[15] = (uint32_t)(int32_t)(int8_t)control_byte;
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, control + UINT32_C(6), &control_byte,
            sizeof(control_byte)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500814), &control
        );
    }
    cpu->registers[3] = control;
    if (status == VF2_OK) {
        cpu->registers[15] = 1u;
        status = vf2_model2a_write(
            machine, control + UINT32_C(0x40), &one, sizeof(one)
        );
    }
    if (status == VF2_OK) {
        cpu->registers[4] = 1u;
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500834), &control
        );
    }
    cpu->registers[3] = control;
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, control, &value);
    }
    cpu->registers[5] = value | UINT32_C(0x80000000);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, control, cpu->registers[5]);
    }
    cpu->registers[5] = UINT32_C(0x0002b1bc);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, control + UINT32_C(0x0c), cpu->registers[5]
        );
    }

    if (status == VF2_OK) {
        cpu->registers[4] = 1u;
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500878), &control
        );
    }
    cpu->registers[3] = control;
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, control, &value);
    }
    cpu->registers[5] = value | UINT32_C(0x80000000);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, control, cpu->registers[5]);
    }
    cpu->registers[5] = UINT32_C(0x0006ca64);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, control + UINT32_C(0x0c), cpu->registers[5]
        );
    }

    if (status == VF2_OK) {
        cpu->registers[4] = 1u;
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050087c), &control
        );
    }
    cpu->registers[3] = control;
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, control, &value);
    }
    cpu->registers[5] = value | UINT32_C(0x80000000);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, control, cpu->registers[5]);
    }
    cpu->registers[5] = UINT32_C(0x0006ca64);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, control + UINT32_C(0x0c), cpu->registers[5]
        );
    }

    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500834), &control
        );
    }
    cpu->registers[3] = control;
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, control, &value);
    }
    cpu->registers[4] = value | UINT32_C(0x04080400);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, control, cpu->registers[4]);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x00500090), &sound_rate, sizeof(sound_rate)
        );
    }
    cpu->registers[3] = (uint32_t)sound_rate;
    cpu->registers[13] = UINT32_C(100);
    cpu->registers[3] *= cpu->registers[13];
    if (status == VF2_OK) {
        status = write_u16(
            machine, UINT32_C(0x0050006e), (uint16_t)cpu->registers[3]
        );
    }
    if (status == VF2_OK) {
        cpu->registers[15] = 0u;
        status = vf2_model2a_write(
            machine, UINT32_C(0x00500030), &zero, sizeof(zero)
        );
    }

    if (status == VF2_OK) {
        cpu->registers[4] = 1u;
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500828), &control
        );
    }
    cpu->registers[3] = control;
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, control, &value);
    }
    cpu->registers[5] = value | UINT32_C(0x80000000);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, control, cpu->registers[5]);
    }
    cpu->registers[5] = UINT32_C(0x000221cc);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, control + UINT32_C(0x0c), cpu->registers[5]
        );
    }

    if (status == VF2_OK) {
        cpu->registers[4] = 1u;
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500850), &control
        );
    }
    cpu->registers[3] = control;
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, control, &value);
    }
    cpu->registers[5] = value & ~UINT32_C(0x80000000);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, control, cpu->registers[5]);
    }

    if (status == VF2_OK) {
        cpu->registers[4] = 1u;
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500814), &control
        );
    }
    cpu->registers[3] = control;
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, control, &value);
    }
    cpu->registers[5] = value | UINT32_C(0x80000000);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, control, cpu->registers[5]);
    }
    if (status != VF2_OK) {
        return status;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_SELECTOR2_POST_INIT_ENTRY, UINT32_C(0x0000ace0)
    );
    if (status == VF2_OK) {
        status = selector2_post_init_helper(machine, cpu);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000ace0)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = read_frame_selector(machine, &selector);
    if (status != VF2_OK || selector != UINT8_C(2)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[15] = (uint32_t)(int32_t)(int8_t)selector;
    ++cpu->registers[15];
    selector = (uint8_t)cpu->registers[15];
    status = vf2_model2a_write(
        machine, VF2_FRAME_SELECTOR, &selector, sizeof(selector)
    );
    if (status != VF2_OK) {
        return status;
    }
    if (bytes_written != NULL) {
        *bytes_written += 58u;
    }
    return vf2_i960_cpu_return_procedure(cpu, machine);
}

static vf2_status execute_frame_dispatch_selector2(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint32_t flags = 0u;
    uint32_t target = 0u;
    uint8_t selector = UINT8_C(2);
    const uint32_t selector_word = UINT32_C(4);
    size_t bytes_written = 0u;
    vf2_status status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00508000), &flags
    );

    cpu->registers[15] = flags;
    if (status != VF2_OK || (flags & (UINT32_C(1) << 5u)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = read_frame_selector(machine, &selector);
    if (status != VF2_OK || selector != UINT8_C(2)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[3] = UINT32_C(2);
    cpu->registers[4] = selector_word;
    status = vf2_model2a_write(
        machine, VF2_FRAME_SELECTOR_COPY, &selector, sizeof(selector)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_FRAME_SELECTOR_MASK, selector_word
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0000a700), &target
        );
    }
    cpu->registers[5] = target;
    if (status != VF2_OK || target != VF2_SELECTOR2_BODY_ENTRY) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    bytes_written += sizeof(uint8_t) + sizeof(uint32_t);

    status = vf2_i960_cpu_enter_procedure(
        cpu, target, VF2_SELECTOR2_BODY_RETURN
    );
    if (status == VF2_OK) {
        status = execute_selector2_body(machine, cpu, &bytes_written);
    }
    if (status != VF2_OK || cpu->ip != VF2_SELECTOR2_BODY_RETURN) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != VF2_SELECTOR2_DISPATCH_RETURN) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->executed_instructions = start_instructions + VF2_SELECTOR2_INSTRUCTIONS;
    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->bytes_written = bytes_written;
    report->recovered_instruction_count = VF2_SELECTOR2_INSTRUCTIONS;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_main_final_cluster_selector2(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report shadow = {0};
    vf2_hybrid_bridge_report buffer = {0};
    vf2_hybrid_bridge_report geometry = {0};
    vf2_hybrid_bridge_report scratch = {0};
    vf2_hybrid_bridge_report dispatch = {0};
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    const uint32_t start_depth = cpu->local_frame_depth;
    vf2_status status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_FRAME_SHADOW_VERIFY_ENTRY, UINT32_C(0x00009ffc)
    );

    if (status == VF2_OK) {
        status = execute_frame_shadow_verify(machine, cpu, &shadow);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00009ffc)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_i960_cpu_enter_procedure(
        cpu, UINT32_C(0x00029744), UINT32_C(0x0000a000)
    );
    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a000)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_FRAME_BUFFER_GATE_ENTRY, UINT32_C(0x0000a004)
    );
    if (status == VF2_OK) {
        status = execute_frame_buffer_gate(machine, cpu, &buffer);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a004)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GEOMETRY_COMMAND_SETUP_ENTRY, UINT32_C(0x0000a008)
    );
    if (status == VF2_OK) {
        status = execute_geometry_command_setup(machine, cpu, &geometry);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a008)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_FRAME_SCRATCH_CLEAR_ENTRY, UINT32_C(0x0000a00c)
    );
    if (status == VF2_OK) {
        status = execute_frame_scratch_clear(machine, cpu, &scratch);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a00c)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_FRAME_DISPATCH_TICK_ENTRY, VF2_SELECTOR2_DISPATCH_RETURN
    );
    if (status == VF2_OK) {
        status = execute_frame_dispatch_selector2(machine, cpu, &dispatch);
    }
    if (status != VF2_OK || cpu->ip != VF2_SELECTOR2_DISPATCH_RETURN) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    if (start_depth == 0u) {
        cpu->registers[13] = (start_depth << 8u) | cpu->local_frame_depth;
    }
    cpu->executed_instructions += UINT64_C(7);
    report->kind = VF2_HYBRID_BRIDGE_MAIN_FINAL_CLUSTER;
    report->entry_address = VF2_MAIN_FINAL_CLUSTER_ENTRY;
    report->exit_address = cpu->ip;
    report->bytes_written =
        shadow.bytes_written + buffer.bytes_written + geometry.bytes_written +
        scratch.bytes_written + dispatch.bytes_written;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status vf2_native_runtime_step(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report
)
{
    uint8_t selector = UINT8_MAX;
    const uint32_t entry = cpu != NULL ? cpu->ip : 0u;
    const uint32_t entry_r3 = cpu != NULL ? cpu->registers[3] : 0u;
    const uint32_t entry_r7 = cpu != NULL ? cpu->registers[7] : 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || state == NULL) {
        return vf2_native_runtime_step_condition_impl(
            machine, cpu, state, report
        );
    }
    if (entry == VF2_MAIN_FINAL_CLUSTER_ENTRY ||
        entry == VF2_FRAME_DISPATCH_TICK_ENTRY) {
        status = read_frame_selector(machine, &selector);
        if (status != VF2_OK) {
            return status;
        }
    }
    if (selector != UINT8_C(2)) {
        return vf2_native_runtime_step_condition_impl(
            machine, cpu, state, report
        );
    }

    {
        vf2_hybrid_bridge_report bridge = {0};
        vf2_native_runtime_step_report local_report = {0};
        vf2_native_runtime_step_report *effective_report =
            report != NULL ? report : &local_report;

        if (entry == VF2_MAIN_FINAL_CLUSTER_ENTRY) {
            status = execute_main_final_cluster_selector2(
                machine, cpu, &bridge
            );
        } else {
            status = execute_frame_dispatch_selector2(machine, cpu, &bridge);
        }
        if (status != VF2_OK) {
            return status;
        }
        accumulate_custom_bridge(state, &bridge, effective_report);
        status = vf2_hybrid_bridge_apply_condition_poststate(
            machine, cpu, entry, entry_r3, entry_r7
        );
        if (status == VF2_OK) {
            status = apply_repeated_bridge_condition(machine, cpu, entry);
        }
        return status;
    }
}
