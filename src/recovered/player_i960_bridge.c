#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "vf2/i960/executor.h"

#define VF2_PLAYER_28178_ENTRY UINT32_C(0x00028178)
#define VF2_PLAYER_28178_FIRST_RETURN UINT32_C(0x0001abf8)
#define VF2_PLAYER_28178_FINAL_RET UINT32_C(0x0001b530)
#define VF2_PLAYER_28178_STOP UINT32_C(0x00014400)
#define VF2_PLAYER_28178_INSTRUCTIONS UINT64_C(128)

typedef struct vf2_player_28178_plan {
    uint32_t player;
    uint32_t state_flags;
    uint32_t record_pointer;
    uint16_t selector;
    int16_t step[2];
    int16_t next_value[2];
    uint8_t duration[2];
    uint8_t target_byte[2];
} vf2_player_28178_plan;

static vf2_status player_read_u8(
    const vf2_model2a *machine,
    uint32_t address,
    uint8_t *value
)
{
    return value == NULL
        ? VF2_ERROR_INVALID_ARGUMENT
        : vf2_model2a_read(machine, address, value, sizeof(*value));
}

static vf2_status player_read_u16(
    const vf2_model2a *machine,
    uint32_t address,
    uint16_t *value
)
{
    uint8_t bytes[2] = {0u, 0u};
    vf2_status status = VF2_OK;

    if (value == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read(machine, address, bytes, sizeof(bytes));
    if (status == VF2_OK) {
        *value = (uint16_t)((uint16_t)bytes[0] |
                            ((uint16_t)bytes[1] << 8u));
    }
    return status;
}

static vf2_status player_write_u16(
    vf2_model2a *machine,
    uint32_t address,
    uint16_t value
)
{
    const uint8_t bytes[2] = {
        (uint8_t)value,
        (uint8_t)(value >> 8u)
    };
    return vf2_model2a_write(machine, address, bytes, sizeof(bytes));
}

static void player_set_compare_result(
    vf2_i960_cpu *cpu,
    vf2_i960_compare_result result
)
{
    uint32_t condition_bits = 0u;

    if (result == VF2_I960_COMPARE_LESS) {
        condition_bits = UINT32_C(4);
    } else if (result == VF2_I960_COMPARE_EQUAL) {
        condition_bits = UINT32_C(2);
    } else if (result == VF2_I960_COMPARE_GREATER) {
        condition_bits = UINT32_C(1);
    }
    cpu->compare_result = result;
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | condition_bits;
}

/* Validate every observed branch before the first write.  A rejected plan can
 * therefore fall back to the ROM interpreter without rolling state back. */
static vf2_status player_prepare_28178(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_player_28178_plan *plan
)
{
    uint32_t packed_state = 0u;
    uint32_t player_flags = 0u;
    uint32_t runtime_flags = 0u;
    uint32_t branch_base = 0u;
    uint16_t counter = 0u;
    uint8_t phase = 0u;
    uint8_t branch_byte = 0u;
    uint8_t records[21] = {0u};
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || plan == NULL ||
        cpu->ip != VF2_PLAYER_28178_ENTRY ||
        cpu->local_frame_depth < 4u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(plan, 0, sizeof(*plan));
    plan->player = cpu->registers[VF2_I960_G0_REGISTER + 7u];
    if (plan->player == 0u ||
        (cpu->registers[0] & UINT32_C(7)) != 0u ||
        (cpu->local_frames[cpu->local_frame_depth - 1u].registers[0] &
         UINT32_C(7)) != 0u ||
        cpu->local_frames[cpu->local_frame_depth - 1u].registers[2] !=
            VF2_PLAYER_28178_FIRST_RETURN ||
        cpu->local_frames[cpu->local_frame_depth - 2u].registers[2] !=
            VF2_PLAYER_28178_STOP) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = player_read_u8(
        machine, plan->player + UINT32_C(0x0a00), &phase
    );
    if (status == VF2_OK) {
        status = player_read_u16(
            machine, plan->player + UINT32_C(0x01aa), &counter
        );
    }
    if (status == VF2_OK) {
        status = player_read_u16(
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
    if (status == VF2_OK) {
        status = player_read_u8(
            machine, branch_base + UINT32_C(0x3351), &branch_byte
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, plan->player + UINT32_C(0x082c),
            &plan->record_pointer
        );
    }
    if (status == VF2_OK && plan->record_pointer != 0u) {
        status = vf2_model2a_read(
            machine, plan->record_pointer, records, sizeof(records)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    if (plan->record_pointer == 0u || phase != 0u ||
        counter != UINT16_C(1) ||
        (runtime_flags & (UINT32_C(1) << 20u)) != 0u ||
        (branch_byte & (UINT8_C(1) << 6u)) != 0u ||
        (plan->state_flags & ((UINT32_C(1) << 20u) |
                              (UINT32_C(1) << 7u))) != 0u ||
        (packed_state & (UINT32_C(1) << 7u)) != 0u ||
        (player_flags & (UINT32_C(1) << 12u)) != 0u ||
        records[0] != UINT8_C(0x12) ||
        records[1] != UINT8_C(0x01) || records[2] != 0u ||
        records[3] != 0u || records[4] != UINT8_C(0x1f) ||
        records[5] != UINT8_C(0x28) || records[6] != 0u ||
        records[7] != UINT8_C(0x12) ||
        records[8] != UINT8_C(0x01) || records[9] != 0u ||
        records[10] != UINT8_C(0x01) || records[11] != UINT8_C(0x1f) ||
        records[12] != UINT8_C(0x28) || records[13] != 0u ||
        records[14] != UINT8_C(0x12) ||
        records[15] != UINT8_C(0x50) || records[16] != 0u ||
        records[17] != 0u || records[18] != UINT8_C(0x0f) ||
        records[19] != UINT8_C(0x78) || records[20] != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    for (index = 0u; index < 2u; ++index) {
        const size_t offset = index * 7u;
        const uint32_t threshold =
            (uint32_t)records[offset + 1u] |
            ((uint32_t)records[offset + 2u] << 8u);
        const uint32_t target_count =
            (uint32_t)records[offset + 5u] |
            ((uint32_t)records[offset + 6u] << 8u);
        uint32_t duration = 0u;
        uint16_t current_bits = 0u;
        int32_t current = 0;
        int32_t target = 0;
        int32_t step = 0;

        if (threshold > (uint32_t)counter ||
            target_count <= (uint32_t)(uint8_t)counter) {
            return VF2_ERROR_UNSUPPORTED;
        }
        duration = target_count - (uint32_t)(uint8_t)counter + UINT32_C(1);
        if (duration == 0u || duration > UINT8_MAX) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = player_read_u16(
            machine,
            plan->player + UINT32_C(0x06c4) +
                (uint32_t)index * UINT32_C(6) + UINT32_C(2),
            &current_bits
        );
        if (status != VF2_OK) {
            return status;
        }
        current = (int32_t)(int16_t)current_bits;
        target = (int32_t)((uint32_t)records[offset + 4u] << 8u);
        step = (target - current) / (int32_t)duration;

        plan->duration[index] = (uint8_t)duration;
        plan->target_byte[index] = records[offset + 4u];
        plan->step[index] = (int16_t)(uint16_t)step;
        plan->next_value[index] = (int16_t)(uint16_t)(
            current + (int32_t)plan->step[index]
        );
    }
    return VF2_OK;
}

static vf2_status player_apply_28178(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    const vf2_player_28178_plan *plan
)
{
    size_t index = 0u;
    vf2_status status = VF2_OK;

    /* 0x28178 ldos + 0x2817c stos, followed by the real RET at 0x28180. */
    status = player_write_u16(
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
    if (cpu->ip != VF2_PLAYER_28178_FIRST_RETURN) {
        return VF2_ERROR_UNSUPPORTED;
    }

    /* Two observed opcode-0x12 records arm the two six-byte interpolation
     * slots at player+0x6c4 and player+0x6ca. */
    for (index = 0u; status == VF2_OK && index < 2u; ++index) {
        const uint32_t entry =
            plan->player + UINT32_C(0x06c4) +
            (uint32_t)index * UINT32_C(6);
        uint8_t value = plan->duration[index];

        status = vf2_model2a_write(machine, entry, &value, sizeof(value));
        value = plan->target_byte[index];
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, entry + UINT32_C(1), &value, sizeof(value)
            );
        }
        if (status == VF2_OK) {
            status = player_write_u16(
                machine, entry + UINT32_C(4),
                (uint16_t)plan->step[index]
            );
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, plan->player + UINT32_C(0x082c),
            plan->record_pointer + UINT32_C(14)
        );
    }
    if (status == VF2_OK) {
        /* 0x1b4c8 stores r8 back even though the observed value is unchanged. */
        status = vf2_model2a_write_u32(
            machine, plan->player + UINT32_C(0x01a4), plan->state_flags
        );
    }

    /* The tail advances both interpolation slots once: 40 -> 39 and
     * current += signed step. */
    for (index = 0u; status == VF2_OK && index < 2u; ++index) {
        const uint32_t entry =
            plan->player + UINT32_C(0x06c4) +
            (uint32_t)index * UINT32_C(6);
        uint8_t value = (uint8_t)(plan->duration[index] - UINT8_C(1));

        status = vf2_model2a_write(machine, entry, &value, sizeof(value));
        if (status == VF2_OK) {
            status = player_write_u16(
                machine, entry + UINT32_C(2),
                (uint16_t)plan->next_value[index]
            );
        }
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[VF2_I960_G0_REGISTER + 4u] =
        plan->record_pointer + UINT32_C(14);
    player_set_compare_result(cpu, VF2_I960_COMPARE_LESS);

    /* The recovered corridor has 124 non-RET instructions after 0x28180;
     * 0x1b530 is its second architectural RET. */
    cpu->ip = VF2_PLAYER_28178_FINAL_RET;
    cpu->executed_instructions += UINT64_C(124);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }
    ++cpu->executed_instructions;
    return cpu->ip == VF2_PLAYER_28178_STOP
        ? VF2_OK : VF2_ERROR_UNSUPPORTED;
}

/* hybrid.c is compiled with vf2_i960_run renamed to this recovered dispatcher.
 * Every non-matching run is delegated unchanged to the architectural i960
 * executor; only the fully guarded 0x28178 -> 0x14400 corridor is native. */
vf2_status vf2_hybrid_i960_run(
    vf2_i960_cpu *cpu,
    vf2_model2a *machine,
    const vf2_i960_run_options *options,
    vf2_i960_run_result *result
)
{
    const uint64_t start_count = cpu != NULL
        ? cpu->executed_instructions : 0u;
    vf2_player_28178_plan plan;
    vf2_status status = VF2_OK;

    if (cpu != NULL && machine != NULL && options != NULL &&
        cpu->ip == VF2_PLAYER_28178_ENTRY &&
        options->stop_address == VF2_PLAYER_28178_STOP &&
        options->trace_callback == NULL &&
        (options->max_steps == 0u ||
         options->max_steps >= VF2_PLAYER_28178_INSTRUCTIONS)) {
        status = player_prepare_28178(machine, cpu, &plan);
        if (status == VF2_OK) {
            status = player_apply_28178(machine, cpu, &plan);
            if (status != VF2_OK) {
                return status;
            }
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
        if (status != VF2_ERROR_UNSUPPORTED) {
            return status;
        }
    }
    return vf2_i960_run(cpu, machine, options, result);
}
