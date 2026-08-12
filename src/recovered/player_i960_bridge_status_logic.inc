#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define VF2_PLAYER_STATUS_ENTRY UINT32_C(0x000180bc)
#define VF2_PLAYER_STATUS_RETURN UINT32_C(0x0001441c)
#define VF2_PLAYER_STATUS_WRAPPER_ENTRY UINT32_C(0x00014418)
#define VF2_PLAYER_STATUS_WRAPPER_TAIL UINT32_C(0x0001441c)
#define VF2_PLAYER_STATUS_WRAPPER_RET UINT32_C(0x00014428)
#define VF2_PLAYER_STATUS_WRAPPER_TAIL_INSTRUCTIONS UINT64_C(4)

typedef struct vf2_player_status_plan {
    uint32_t player;
    uint32_t flags_after;
    uint16_t status_value;
    uint64_t instruction_count;
    bool clear_gate;
} vf2_player_status_plan;

static uint32_t player_status_sign_extend_u16(uint16_t value)
{
    uint32_t result = (uint32_t)value;

    if ((value & UINT16_C(0x8000)) != 0u) {
        result |= UINT32_C(0xffff0000);
    }
    return result;
}

static vf2_status player_status_read_u16(
    const vf2_model2a *machine,
    uint32_t address,
    uint16_t *value
)
{
    uint8_t bytes[2] = {0u, 0u};
    vf2_status status = VF2_OK;

    if (machine == NULL || value == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read(machine, address, bytes, sizeof(bytes));
    if (status == VF2_OK) {
        *value = (uint16_t)((uint16_t)bytes[0] |
                            ((uint16_t)bytes[1] << 8u));
    }
    return status;
}

static vf2_status player_status_write_u16(
    vf2_model2a *machine,
    uint32_t address,
    uint16_t value
)
{
    const uint8_t bytes[2] = {
        (uint8_t)value,
        (uint8_t)(value >> 8u)
    };

    return machine == NULL
        ? VF2_ERROR_INVALID_ARGUMENT
        : vf2_model2a_write(machine, address, bytes, sizeof(bytes));
}

/*
 * Recover the complete 0x180bc state-update routine before performing a write.
 * The plan is branch-generic: it covers both state-bit-3 trees and both
 * cmpoble outcomes, while preserving the executor's no-condition-code COBR
 * semantics.
 */
static vf2_status player_status_prepare(
    vf2_model2a *machine,
    const vf2_i960_cpu *cpu,
    vf2_player_status_plan *plan
)
{
    uint32_t state_flags = 0u;
    uint32_t base_flags = 0u;
    uint16_t base_status = 0u;
    uint16_t state_bias = 0u;
    uint16_t mode_word = 0u;
    uint16_t half_source = 0u;
    uint16_t threshold = 0u;
    uint32_t combined = 0u;
    uint64_t middle_count = 0u;
    uint64_t final_count = 0u;
    bool set_flag8 = false;
    const uint32_t player = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 7u] : 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || plan == NULL || player == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(plan, 0, sizeof(*plan));
    plan->player = player;

    status = player_status_read_u16(
        machine, player + UINT32_C(0x0026), &base_status
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player + UINT32_C(0x01a4), &state_flags
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    combined = player_status_sign_extend_u16(base_status);
    plan->instruction_count = UINT64_C(6);
    if ((state_flags & (UINT32_C(1) << 6u)) != 0u) {
        status = player_status_read_u16(
            machine, player + UINT32_C(0x0812), &state_bias
        );
        if (status != VF2_OK) {
            return status;
        }
        combined += player_status_sign_extend_u16(state_bias);
        plan->instruction_count += UINT64_C(2);
    }
    plan->status_value = (uint16_t)combined;

    status = player_status_read_u16(
        machine, player + UINT32_C(0x0810), &mode_word
    );
    if (status != VF2_OK) {
        return status;
    }

    /*
     * 0x180d8 splits on state bit 3.  Each side has an opposite shortcut on
     * mode bit 7 and otherwise compares unsigned player+0x1aa against
     * (player+0x800)>>1.
     */
    if ((state_flags & (UINT32_C(1) << 3u)) == 0u) {
        middle_count = UINT64_C(2);
        if ((mode_word & (UINT16_C(1) << 7u)) == 0u) {
            set_flag8 = false;
            middle_count += UINT64_C(4);
        } else {
            status = player_status_read_u16(
                machine, player + UINT32_C(0x0800), &half_source
            );
            if (status == VF2_OK) {
                status = player_status_read_u16(
                    machine, player + UINT32_C(0x01aa), &threshold
                );
            }
            if (status != VF2_OK) {
                return status;
            }
            half_source = (uint16_t)((uint32_t)half_source >> 1u);
            middle_count += UINT64_C(4);
            if ((uint32_t)threshold <= (uint32_t)half_source) {
                set_flag8 = true;
                middle_count += UINT64_C(3);
            } else {
                set_flag8 = false;
                middle_count += UINT64_C(4);
            }
        }
    } else {
        middle_count = UINT64_C(2);
        if ((mode_word & (UINT16_C(1) << 7u)) != 0u) {
            set_flag8 = true;
            middle_count += UINT64_C(3);
        } else {
            status = player_status_read_u16(
                machine, player + UINT32_C(0x0800), &half_source
            );
            if (status == VF2_OK) {
                status = player_status_read_u16(
                    machine, player + UINT32_C(0x01aa), &threshold
                );
            }
            if (status != VF2_OK) {
                return status;
            }
            half_source = (uint16_t)((uint32_t)half_source >> 1u);
            middle_count += UINT64_C(4);
            if ((uint32_t)threshold <= (uint32_t)half_source) {
                set_flag8 = false;
                middle_count += UINT64_C(4);
            } else {
                set_flag8 = true;
                middle_count += UINT64_C(3);
            }
        }
    }

    status = vf2_model2a_read_u32(machine, player, &base_flags);
    if (status != VF2_OK) {
        return status;
    }
    plan->flags_after = set_flag8
        ? base_flags | (UINT32_C(1) << 8u)
        : base_flags & ~(UINT32_C(1) << 8u);

    /*
     * 0x18128 leaves +0x6d8 alone when state bit 0 or base flag bit 4 is set;
     * otherwise it stores one zero byte before returning.
     */
    if ((state_flags & UINT32_C(1)) != 0u) {
        final_count = UINT64_C(3);
    } else if ((plan->flags_after & (UINT32_C(1) << 4u)) != 0u) {
        final_count = UINT64_C(5);
    } else {
        plan->clear_gate = true;
        final_count = UINT64_C(7);
    }

    plan->instruction_count += middle_count + final_count;
    return VF2_OK;
}

static vf2_status player_status_apply(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    const vf2_player_status_plan *plan
)
{
    uint8_t zero = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || plan == NULL ||
        cpu->ip != VF2_PLAYER_STATUS_ENTRY ||
        cpu->local_frame_depth == 0u ||
        (cpu->registers[0] & UINT32_C(7)) != 0u ||
        cpu->local_frames[cpu->local_frame_depth - 1u].registers[2] !=
            VF2_PLAYER_STATUS_RETURN) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = player_status_write_u16(
        machine, plan->player + UINT32_C(0x05b4), plan->status_value
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, plan->player, plan->flags_after
        );
    }
    if (status == VF2_OK && plan->clear_gate) {
        status = vf2_model2a_write(
            machine, plan->player + UINT32_C(0x06d8),
            &zero, sizeof(zero)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->ip = UINT32_C(0x00018140);
    cpu->executed_instructions += plan->instruction_count - UINT64_C(1);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }
    ++cpu->executed_instructions;
    return cpu->ip == VF2_PLAYER_STATUS_RETURN
        ? VF2_OK : VF2_ERROR_UNSUPPORTED;
}

static uint32_t player_status_outer_return(const vf2_i960_cpu *cpu)
{
    return cpu != NULL && cpu->local_frame_depth != 0u
        ? cpu->local_frames[cpu->local_frame_depth - 1u].registers[2]
        : 0u;
}

static vf2_status player_status_apply_wrapper_tail(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t expected_return
)
{
    const uint32_t player = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 7u] : 0u;
    uint32_t flags = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL ||
        cpu->ip != VF2_PLAYER_STATUS_WRAPPER_TAIL ||
        cpu->local_frame_depth == 0u || player == 0u ||
        (cpu->registers[0] & UINT32_C(7)) != 0u ||
        player_status_outer_return(cpu) != expected_return) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_model2a_read_u32(machine, player, &flags);
    if (status == VF2_OK) {
        flags |= UINT32_C(1) << 7u;
        status = vf2_model2a_write_u32(machine, player, flags);
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->ip = VF2_PLAYER_STATUS_WRAPPER_RET;
    cpu->executed_instructions +=
        VF2_PLAYER_STATUS_WRAPPER_TAIL_INSTRUCTIONS - UINT64_C(1);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }
    ++cpu->executed_instructions;
    return cpu->ip == expected_return
        ? VF2_OK : VF2_ERROR_UNSUPPORTED;
}

static void player_status_result(
    vf2_i960_run_result *result,
    const vf2_i960_cpu *cpu,
    uint64_t start_count
)
{
    if (result != NULL && cpu != NULL) {
        memset(result, 0, sizeof(*result));
        result->halt_reason = VF2_I960_HALT_STOP_ADDRESS;
        result->status = VF2_OK;
        result->halt_address = cpu->ip;
        result->executed_instructions =
            cpu->executed_instructions - start_count;
    }
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
    vf2_player_status_plan plan;
    vf2_status status = VF2_OK;

    /*
     * Callee-only bridge: this is the exact CALL site at 0x14418, so the
     * architectural return must be 0x1441c.
     */
    if (cpu != NULL && machine != NULL && options != NULL &&
        cpu->ip == VF2_PLAYER_STATUS_ENTRY &&
        options->stop_address == VF2_PLAYER_STATUS_RETURN &&
        options->trace_callback == NULL &&
        cpu->local_frame_depth != 0u &&
        cpu->local_frames[cpu->local_frame_depth - 1u].registers[2] ==
            VF2_PLAYER_STATUS_RETURN) {
        status = player_status_prepare(machine, cpu, &plan);
        if (status == VF2_OK &&
            (options->max_steps == 0u ||
             options->max_steps >= plan.instruction_count)) {
            status = player_status_apply(machine, cpu, &plan);
            if (status != VF2_OK) {
                return status;
            }
            player_status_result(result, cpu, start_count);
            return VF2_OK;
        }
        if (status != VF2_OK && status != VF2_ERROR_UNSUPPORTED) {
            return status;
        }
    }

    /*
     * CALL + recovered callee.  This lets a run starting at 0x14418 cross the
     * complete 0x180bc routine and halt at the first wrapper instruction.
     */
    if (cpu != NULL && machine != NULL && options != NULL &&
        cpu->ip == VF2_PLAYER_STATUS_WRAPPER_ENTRY &&
        options->stop_address == VF2_PLAYER_STATUS_RETURN &&
        options->trace_callback == NULL) {
        status = player_status_prepare(machine, cpu, &plan);
        if (status == VF2_OK &&
            (options->max_steps == 0u ||
             options->max_steps >= plan.instruction_count + UINT64_C(1))) {
            status = vf2_i960_cpu_enter_procedure(
                cpu, VF2_PLAYER_STATUS_ENTRY, VF2_PLAYER_STATUS_RETURN
            );
            if (status != VF2_OK) {
                return status;
            }
            ++cpu->executed_instructions;
            status = player_status_apply(machine, cpu, &plan);
            if (status != VF2_OK) {
                return status;
            }
            player_status_result(result, cpu, start_count);
            return VF2_OK;
        }
        if (status != VF2_OK && status != VF2_ERROR_UNSUPPORTED) {
            return status;
        }
    }

    /*
     * The four-instruction wrapper tail is also generic: set base flag bit 7,
     * store it, then RET to the already-saved outer caller.
     */
    if (cpu != NULL && machine != NULL && options != NULL &&
        cpu->ip == VF2_PLAYER_STATUS_WRAPPER_TAIL &&
        options->trace_callback == NULL &&
        cpu->local_frame_depth != 0u &&
        options->stop_address == player_status_outer_return(cpu) &&
        (options->max_steps == 0u ||
         options->max_steps >= VF2_PLAYER_STATUS_WRAPPER_TAIL_INSTRUCTIONS)) {
        status = player_status_apply_wrapper_tail(
            machine, cpu, options->stop_address
        );
        if (status == VF2_OK) {
            player_status_result(result, cpu, start_count);
            return VF2_OK;
        }
        if (status != VF2_ERROR_UNSUPPORTED) {
            return status;
        }
    }

    /*
     * Full wrapper bridge: CALL 0x180bc, execute the recovered state update,
     * set base flag bit 7, and return to the outer caller in one native run.
     */
    if (cpu != NULL && machine != NULL && options != NULL &&
        cpu->ip == VF2_PLAYER_STATUS_WRAPPER_ENTRY &&
        options->trace_callback == NULL &&
        cpu->local_frame_depth != 0u &&
        options->stop_address == player_status_outer_return(cpu)) {
        const uint32_t outer_return = options->stop_address;
        status = player_status_prepare(machine, cpu, &plan);
        if (status == VF2_OK &&
            (options->max_steps == 0u ||
             options->max_steps >=
                 plan.instruction_count + UINT64_C(5))) {
            status = vf2_i960_cpu_enter_procedure(
                cpu, VF2_PLAYER_STATUS_ENTRY, VF2_PLAYER_STATUS_RETURN
            );
            if (status != VF2_OK) {
                return status;
            }
            ++cpu->executed_instructions;
            status = player_status_apply(machine, cpu, &plan);
            if (status == VF2_OK) {
                status = player_status_apply_wrapper_tail(
                    machine, cpu, outer_return
                );
            }
            if (status != VF2_OK) {
                return status;
            }
            player_status_result(result, cpu, start_count);
            return VF2_OK;
        }
        if (status != VF2_OK && status != VF2_ERROR_UNSUPPORTED) {
            return status;
        }
    }

    return vf2_hybrid_i960_run_tail_pose(cpu, machine, options, result);
}
