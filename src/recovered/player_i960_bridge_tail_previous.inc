#include <stdint.h>
#include <string.h>

#include "vf2/i960/executor.h"

#define VF2_PLAYER_4B640_ENTRY UINT32_C(0x0004b640)
#define VF2_PLAYER_4B640_RETURN UINT32_C(0x0001440c)
#define VF2_PLAYER_4B640_STOP UINT32_C(0x00014414)
#define VF2_PLAYER_4B640_INSTRUCTIONS UINT64_C(123)
#define VF2_PLAYER_4B640_NESTED_CALLS UINT64_C(5)
#define VF2_PLAYER_4B640_NESTED_RETURNS UINT64_C(5)

/* Earlier guarded player corridors live in player_i960_bridge.c. */
vf2_status vf2_hybrid_i960_run(
    vf2_i960_cpu *cpu,
    vf2_model2a *machine,
    const vf2_i960_run_options *options,
    vf2_i960_run_result *result
);

static vf2_status player_tail_read_u8(
    const vf2_model2a *machine,
    uint32_t address,
    uint8_t *value
)
{
    return value == NULL
        ? VF2_ERROR_INVALID_ARGUMENT
        : vf2_model2a_read(machine, address, value, sizeof(*value));
}

static vf2_status player_tail_read_u16(
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

static vf2_status player_tail_write_u16(
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

static vf2_status player_tail_expect_u32(
    const vf2_model2a *machine,
    uint32_t address,
    uint32_t expected
)
{
    uint32_t value = 0u;
    vf2_status status = vf2_model2a_read_u32(machine, address, &value);

    if (status != VF2_OK) {
        return status;
    }
    return value == expected ? VF2_OK : VF2_ERROR_UNSUPPORTED;
}

static vf2_status player_tail_expect_u16(
    const vf2_model2a *machine,
    uint32_t address,
    uint16_t expected
)
{
    uint16_t value = 0u;
    vf2_status status = player_tail_read_u16(machine, address, &value);

    if (status != VF2_OK) {
        return status;
    }
    return value == expected ? VF2_OK : VF2_ERROR_UNSUPPORTED;
}

static vf2_status player_tail_expect_u8(
    const vf2_model2a *machine,
    uint32_t address,
    uint8_t expected
)
{
    uint8_t value = 0u;
    vf2_status status = player_tail_read_u8(machine, address, &value);

    if (status != VF2_OK) {
        return status;
    }
    return value == expected ? VF2_OK : VF2_ERROR_UNSUPPORTED;
}

static vf2_status player_tail_validate_record(
    const vf2_model2a *machine,
    uint32_t address
)
{
    vf2_status status = player_tail_expect_u32(
        machine, address, UINT32_C(0x0000ffff)
    );
    if (status == VF2_OK) {
        status = player_tail_expect_u32(machine, address + UINT32_C(8), 0u);
    }
    if (status == VF2_OK) {
        status = player_tail_expect_u32(
            machine, address + UINT32_C(0x10), 0u
        );
    }
    if (status == VF2_OK) {
        status = player_tail_expect_u16(
            machine, address + UINT32_C(0x1c), 0u
        );
    }
    return status;
}

static vf2_status player_tail_write_record(
    vf2_model2a *machine,
    uint32_t address,
    uint16_t selector,
    uint32_t flags
)
{
    vf2_status status = player_tail_write_u16(machine, address, selector);

    if (status == VF2_OK) {
        status = player_tail_write_u16(
            machine, address + UINT32_C(2), UINT16_C(0xffff)
        );
    }
    if (status == VF2_OK) {
        status = player_tail_write_u16(
            machine, address + UINT32_C(8), 0u
        );
    }
    if (status == VF2_OK) {
        status = player_tail_write_u16(
            machine, address + UINT32_C(0x0a), 1u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, address + UINT32_C(0x10), flags
        );
    }
    if (status == VF2_OK) {
        status = player_tail_write_u16(
            machine, address + UINT32_C(0x1c), 2u
        );
    }
    return status;
}

/* Recover the sixth-entry 0x4b640 corridor observed after the two movement
 * helpers. The active countdown is one, so it reaches zero immediately. Both
 * 0x4b9b8 record publishers take their short update path; the final 0x4b604
 * receives a null table pointer and returns. All route selectors are checked
 * before the first write so a mismatch can safely fall back to the ROM. */
static vf2_status player_tail_execute_4b640(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    const uint32_t player = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 7u] : 0u;
    const uint32_t first_record = UINT32_C(0x005501c8);
    const uint32_t second_record = UINT32_C(0x00550208);
    uint32_t table0 = 0u;
    uint32_t table1 = 0u;
    uint32_t rom_marker = 0u;
    uint32_t data_marker = 0u;
    uint16_t selector0 = 0u;
    uint16_t selector1 = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL ||
        cpu->ip != VF2_PLAYER_4B640_ENTRY ||
        cpu->local_frame_depth < 3u ||
        player != UINT32_C(0x00510980) ||
        cpu->local_frames[cpu->local_frame_depth - 1u].registers[2] !=
            VF2_PLAYER_4B640_RETURN) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = player_tail_expect_u16(
        machine, player + UINT32_C(0x770), 0u
    );
    if (status == VF2_OK) {
        status = player_tail_expect_u8(
            machine, player + UINT32_C(0x1b1), 0u
        );
    }
    if (status == VF2_OK) {
        status = player_tail_expect_u8(
            machine, player + UINT32_C(4), 0u
        );
    }
    if (status == VF2_OK) {
        status = player_tail_expect_u8(
            machine, UINT32_C(0x0050002b), UINT8_C(0x11)
        );
    }
    if (status == VF2_OK) {
        status = player_tail_expect_u32(
            machine, UINT32_C(0x00500068), UINT32_C(0x80004400)
        );
    }
    if (status == VF2_OK) {
        status = player_tail_expect_u16(
            machine, player + UINT32_C(0x6b0), 1u
        );
    }
    if (status == VF2_OK) {
        status = player_tail_expect_u16(
            machine, player + UINT32_C(0x6a8), 0u
        );
    }
    if (status == VF2_OK) {
        status = player_tail_expect_u16(
            machine, player + UINT32_C(0x6aa), 0u
        );
    }
    if (status == VF2_OK) {
        status = player_tail_expect_u16(
            machine, player + UINT32_C(0x6ac), 0u
        );
    }
    if (status == VF2_OK) {
        status = player_tail_expect_u16(
            machine, player + UINT32_C(0x6ae), 0u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player + UINT32_C(0x6a0), &table0
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player + UINT32_C(0x6a4), &table1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player + UINT32_C(0x6b8), &rom_marker
        );
    }
    if (status != VF2_OK ||
        table0 != UINT32_C(0x0200732c) ||
        table1 != UINT32_C(0x02007324) || rom_marker != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = player_tail_read_u16(machine, table0, &selector0);
    if (status == VF2_OK) {
        status = player_tail_read_u16(machine, table1, &selector1);
    }
    if (status != VF2_OK || selector0 != UINT16_C(0x002b) ||
        selector1 != UINT16_C(0x002c)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = player_tail_expect_u32(
        machine, player + UINT32_C(0x48), UINT32_C(0x0000019c)
    );
    if (status == VF2_OK) {
        status = player_tail_expect_u32(machine, UINT32_C(0x00550000), 1u);
    }
    if (status == VF2_OK) {
        status = player_tail_expect_u32(machine, UINT32_C(0x0055000c), 0u);
    }
    if (status == VF2_OK) {
        status = player_tail_expect_u32(machine, UINT32_C(0x00550080), 0u);
    }
    if (status == VF2_OK) {
        status = player_tail_expect_u32(machine, UINT32_C(0x005500f4), 0u);
    }
    if (status == VF2_OK) {
        status = player_tail_expect_u16(machine, UINT32_C(0x0055c2f0), 0u);
    }
    if (status == VF2_OK) {
        status = player_tail_validate_record(machine, first_record);
    }
    if (status == VF2_OK) {
        status = player_tail_validate_record(machine, second_record);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0004ad74), &rom_marker
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x02300000), &data_marker
        );
    }
    if (status != VF2_OK || rom_marker != data_marker) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = player_tail_write_u16(
        machine, player + UINT32_C(0x6b0), 0u
    );
    if (status == VF2_OK) {
        status = player_tail_write_u16(
            machine, player + UINT32_C(0x6a8), 0u
        );
    }
    if (status == VF2_OK) {
        status = player_tail_write_u16(
            machine, player + UINT32_C(0x6aa), 0u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, player + UINT32_C(0x48), UINT32_C(0x0000019c)
        );
    }
    if (status == VF2_OK) {
        status = player_tail_write_record(
            machine, first_record, selector0, UINT32_C(0x10)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0055000c), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00550080), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x005500f4), 0u);
    }
    if (status == VF2_OK) {
        status = player_tail_write_record(
            machine, second_record, selector1, UINT32_C(0x30)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[VF2_I960_G0_REGISTER] = 0u;
    cpu->registers[VF2_I960_G0_REGISTER + 1u] = UINT32_C(0x30);
    cpu->registers[VF2_I960_G0_REGISTER + 2u] = second_record;
    cpu->registers[VF2_I960_G0_REGISTER + 3u] = UINT32_C(2);
    cpu->registers[VF2_I960_G0_REGISTER + 4u] = 0u;
    if (cpu->maximum_local_frame_depth < cpu->local_frame_depth + 2u) {
        cpu->maximum_local_frame_depth = cpu->local_frame_depth + 2u;
    }
    cpu->procedure_calls += VF2_PLAYER_4B640_NESTED_CALLS;
    cpu->procedure_returns += VF2_PLAYER_4B640_NESTED_RETURNS;

    /* 121 instructions precede the parent RET; the caller then executes the
     * unconditional 0x1440c -> 0x14414 branch that belongs to this bounded
     * interpreted-until corridor. */
    cpu->executed_instructions += UINT64_C(121);
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

vf2_status vf2_hybrid_i960_run_tail(
    vf2_i960_cpu *cpu,
    vf2_model2a *machine,
    const vf2_i960_run_options *options,
    vf2_i960_run_result *result
)
{
    const uint64_t start_count = cpu != NULL
        ? cpu->executed_instructions : 0u;
    vf2_status status = VF2_OK;

    if (cpu != NULL && machine != NULL && options != NULL &&
        cpu->ip == VF2_PLAYER_4B640_ENTRY &&
        options->stop_address == VF2_PLAYER_4B640_STOP &&
        options->trace_callback == NULL &&
        (options->max_steps == 0u ||
         options->max_steps >= VF2_PLAYER_4B640_INSTRUCTIONS)) {
        status = player_tail_execute_4b640(machine, cpu);
        if (status == VF2_OK) {
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
    return vf2_hybrid_i960_run(cpu, machine, options, result);
}
