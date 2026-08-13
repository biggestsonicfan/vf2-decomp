#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vf2/i960/executor.h"
#include "vf2/model2a.h"

#define PLAYER UINT32_C(0x00510000)
#define RECORDS UINT32_C(0x00524000)
#define BRANCH_BASE UINT32_C(0x00500000)
#define ENTRY UINT32_C(0x00028178)
#define FIRST_RETURN UINT32_C(0x0001abf8)
#define STOP UINT32_C(0x00014400)

static int failures = 0;

#define CHECK(expression)                                           \
    do {                                                            \
        if (!(expression)) {                                        \
            fprintf(                                                \
                stderr,                                             \
                "FAILED %s:%d: %s\n",                              \
                __FILE__,                                           \
                __LINE__,                                           \
                #expression                                         \
            );                                                      \
            ++failures;                                             \
        }                                                           \
    } while (0)

/* Internal recovered dispatcher composed by player_i960_bridge_tail.c. */
extern vf2_status vf2_hybrid_i960_run_tail(
    vf2_i960_cpu *cpu,
    vf2_model2a *machine,
    const vf2_i960_run_options *options,
    vf2_i960_run_result *result
);

static vf2_status write_u8(vf2_model2a *machine, uint32_t address, uint8_t value)
{
    return vf2_model2a_write(machine, address, &value, sizeof(value));
}

static vf2_status write_u16(
    vf2_model2a *machine,
    uint32_t address,
    uint16_t value
)
{
    const uint8_t bytes[2] = {(uint8_t)value, (uint8_t)(value >> 8u)};
    return vf2_model2a_write(machine, address, bytes, sizeof(bytes));
}

static uint8_t read_u8(const vf2_model2a *machine, uint32_t address)
{
    uint8_t value = 0u;
    CHECK(vf2_model2a_read(machine, address, &value, sizeof(value)) == VF2_OK);
    return value;
}

static uint16_t read_u16(const vf2_model2a *machine, uint32_t address)
{
    uint8_t bytes[2] = {0u, 0u};
    CHECK(vf2_model2a_read(machine, address, bytes, sizeof(bytes)) == VF2_OK);
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8u));
}

static uint32_t read_u32(const vf2_model2a *machine, uint32_t address)
{
    uint32_t value = 0u;
    CHECK(vf2_model2a_read_u32(machine, address, &value) == VF2_OK);
    return value;
}

static void write_record(
    vf2_model2a *machine,
    uint32_t address,
    uint16_t threshold,
    uint8_t slot,
    uint8_t target,
    uint16_t target_count
)
{
    CHECK(write_u8(machine, address, UINT8_C(0x12)) == VF2_OK);
    CHECK(write_u16(machine, address + UINT32_C(1), threshold) == VF2_OK);
    CHECK(write_u8(machine, address + UINT32_C(3), slot) == VF2_OK);
    CHECK(write_u8(machine, address + UINT32_C(4), target) == VF2_OK);
    CHECK(write_u16(machine, address + UINT32_C(5), target_count) == VF2_OK);
}

static void write_slot(
    vf2_model2a *machine,
    uint32_t slot,
    uint8_t duration,
    uint8_t target,
    int16_t current,
    int16_t step
)
{
    const uint32_t entry = PLAYER + UINT32_C(0x06c4) + slot * UINT32_C(6);
    CHECK(write_u8(machine, entry, duration) == VF2_OK);
    CHECK(write_u8(machine, entry + UINT32_C(1), target) == VF2_OK);
    CHECK(write_u16(machine, entry + UINT32_C(2), (uint16_t)current) == VF2_OK);
    CHECK(write_u16(machine, entry + UINT32_C(4), (uint16_t)step) == VF2_OK);
}

static int prepare_machine(vf2_model2a *machine, vf2_i960_cpu *cpu, uint16_t counter)
{
    vf2_status status = VF2_OK;

    memset(machine, 0, sizeof(*machine));
    if (!vf2_model2a_initialize(machine)) {
        return 0;
    }
    memset(machine->work_ram, 0, machine->work_ram_size);

    status = vf2_model2a_write_u32(
        machine, UINT32_C(0x0050016c), BRANCH_BASE
    );
    if (status == VF2_OK) {
        status = write_u8(machine, BRANCH_BASE + UINT32_C(0x3351), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500068), 0u);
    }
    if (status == VF2_OK) {
        status = write_u8(machine, PLAYER + UINT32_C(0x0a00), 0u);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, PLAYER + UINT32_C(0x01aa), counter);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, PLAYER + UINT32_C(0x01a8), UINT16_C(0x1234));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, PLAYER + UINT32_C(0x01a4), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, PLAYER + UINT32_C(0x0804), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, PLAYER, 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, PLAYER + UINT32_C(0x082c), RECORDS);
    }
    if (status != VF2_OK) {
        vf2_model2a_shutdown(machine);
        return 0;
    }

    vf2_i960_cpu_reset(cpu, 0u, 0u, 0u);
    cpu->registers[1] = UINT32_C(0x005f0000);
    cpu->registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ef000);
    cpu->registers[VF2_I960_G0_REGISTER + 7u] = PLAYER;

    if (vf2_i960_cpu_enter_procedure(
            cpu, UINT32_C(0x1000), UINT32_C(0x2000)) != VF2_OK ||
        vf2_i960_cpu_enter_procedure(
            cpu, UINT32_C(0x3000), UINT32_C(0x4000)) != VF2_OK ||
        vf2_i960_cpu_enter_procedure(
            cpu, UINT32_C(0x5000), STOP) != VF2_OK ||
        vf2_i960_cpu_enter_procedure(cpu, ENTRY, FIRST_RETURN) != VF2_OK) {
        vf2_model2a_shutdown(machine);
        return 0;
    }
    return 1;
}

static void run_exact(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint64_t expected_instructions
)
{
    vf2_i960_run_options options;
    vf2_i960_run_result result;
    vf2_status status = VF2_OK;

    memset(&options, 0, sizeof(options));
    memset(&result, 0, sizeof(result));
    options.stop_address = STOP;
    options.max_steps = expected_instructions;
    options.stop_on_self_branch = true;
    status = vf2_hybrid_i960_run_tail(cpu, machine, &options, &result);
    CHECK(status == VF2_OK);
    CHECK(result.halt_reason == VF2_I960_HALT_STOP_ADDRESS);
    CHECK(result.executed_instructions == expected_instructions);
    CHECK(cpu->executed_instructions == expected_instructions);
    CHECK(cpu->ip == STOP);
    CHECK(cpu->local_frame_depth == 2u);
    CHECK(cpu->procedure_calls == 4u);
    CHECK(cpu->procedure_returns == 2u);
    CHECK(cpu->compare_result == VF2_I960_COMPARE_LESS);
    CHECK(cpu->registers[VF2_I960_G0_REGISTER + 7u] == PLAYER);
    CHECK(read_u16(machine, PLAYER + UINT32_C(0x0c4c)) == UINT16_C(0x1234));
}

static void test_single_record_then_future_threshold(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    const uint32_t slot0 = PLAYER + UINT32_C(0x06c4);

    CHECK(prepare_machine(&machine, &cpu, UINT16_C(10)));
    if (machine.work_ram == NULL) {
        return;
    }
    write_slot(&machine, 0u, 0u, 0u, (int16_t)UINT16_C(0x1000), 0);
    write_slot(&machine, 1u, 0u, 0u, 0, 0);
    write_record(&machine, RECORDS, UINT16_C(5), 0u, UINT8_C(0x20), UINT16_C(20));
    write_record(
        &machine, RECORDS + UINT32_C(7), UINT16_C(11),
        1u, UINT8_C(0x30), UINT16_C(30)
    );

    run_exact(&machine, &cpu, UINT64_C(86));
    CHECK(read_u32(&machine, PLAYER + UINT32_C(0x082c)) == RECORDS + UINT32_C(7));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 4u] == RECORDS + UINT32_C(7));
    CHECK(read_u8(&machine, slot0) == UINT8_C(10));
    CHECK(read_u8(&machine, slot0 + UINT32_C(1)) == UINT8_C(0x20));
    CHECK(read_u16(&machine, slot0 + UINT32_C(2)) == UINT16_C(0x1174));
    CHECK(read_u16(&machine, slot0 + UINT32_C(4)) == UINT16_C(0x0174));
    vf2_model2a_shutdown(&machine);
}

static void test_immediate_record_expires_in_tail(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    const uint32_t slot1 = PLAYER + UINT32_C(0x06ca);

    CHECK(prepare_machine(&machine, &cpu, UINT16_C(50)));
    if (machine.work_ram == NULL) {
        return;
    }
    write_slot(&machine, 0u, 0u, 0u, 0, 0);
    write_slot(
        &machine, 1u, 7u, UINT8_C(0x44),
        (int16_t)UINT16_C(0x2222), (int16_t)UINT16_C(0x7777)
    );
    write_record(&machine, RECORDS, 0u, 1u, UINT8_C(0x33), UINT16_C(40));
    CHECK(write_u8(&machine, RECORDS + UINT32_C(7), 0u) == VF2_OK);

    run_exact(&machine, &cpu, UINT64_C(73));
    CHECK(read_u32(&machine, PLAYER + UINT32_C(0x082c)) == RECORDS + UINT32_C(7));
    CHECK(read_u8(&machine, slot1) == 0u);
    CHECK(read_u8(&machine, slot1 + UINT32_C(1)) == 0u);
    CHECK(read_u16(&machine, slot1 + UINT32_C(2)) == UINT16_C(0x3300));
    /* Immediate opcode-12 does not rewrite the step field. */
    CHECK(read_u16(&machine, slot1 + UINT32_C(4)) == UINT16_C(0x7777));
    vf2_model2a_shutdown(&machine);
}

static void test_two_records_reuse_same_slot(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    const uint32_t slot0 = PLAYER + UINT32_C(0x06c4);

    CHECK(prepare_machine(&machine, &cpu, UINT16_C(10)));
    if (machine.work_ram == NULL) {
        return;
    }
    write_slot(&machine, 0u, 3u, UINT8_C(0x55), (int16_t)UINT16_C(0x0400), 0);
    write_slot(&machine, 1u, 0u, 0u, 0, 0);
    write_record(&machine, RECORDS, 0u, 0u, UINT8_C(0x10), UINT16_C(5));
    write_record(
        &machine, RECORDS + UINT32_C(7), 0u,
        0u, UINT8_C(0x20), UINT16_C(20)
    );
    CHECK(write_u8(&machine, RECORDS + UINT32_C(14), 0u) == VF2_OK);

    run_exact(&machine, &cpu, UINT64_C(109));
    CHECK(read_u32(&machine, PLAYER + UINT32_C(0x082c)) == RECORDS + UINT32_C(14));
    CHECK(read_u8(&machine, slot0) == UINT8_C(10));
    CHECK(read_u8(&machine, slot0 + UINT32_C(1)) == UINT8_C(0x20));
    CHECK(read_u16(&machine, slot0 + UINT32_C(2)) == UINT16_C(0x1174));
    CHECK(read_u16(&machine, slot0 + UINT32_C(4)) == UINT16_C(0x0174));
    vf2_model2a_shutdown(&machine);
}

static void test_threshold_stop_still_advances_existing_slots(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    const uint32_t slot0 = PLAYER + UINT32_C(0x06c4);
    const uint32_t slot1 = PLAYER + UINT32_C(0x06ca);

    CHECK(prepare_machine(&machine, &cpu, UINT16_C(5)));
    if (machine.work_ram == NULL) {
        return;
    }
    write_slot(&machine, 0u, 2u, UINT8_C(0x20), (int16_t)1000, (int16_t)100);
    write_slot(
        &machine, 1u, 1u, UINT8_C(0x30),
        (int16_t)UINT16_C(0x30aa), (int16_t)200
    );
    write_record(&machine, RECORDS, UINT16_C(6), 0u, UINT8_C(0x40), UINT16_C(20));

    run_exact(&machine, &cpu, UINT64_C(58));
    CHECK(read_u32(&machine, PLAYER + UINT32_C(0x082c)) == RECORDS);
    CHECK(read_u8(&machine, slot0) == UINT8_C(1));
    CHECK(read_u16(&machine, slot0 + UINT32_C(2)) == UINT16_C(1100));
    CHECK(read_u8(&machine, slot1) == 0u);
    CHECK(read_u8(&machine, slot1 + UINT32_C(1)) == 0u);
    CHECK(read_u16(&machine, slot1 + UINT32_C(2)) == UINT16_C(0x3000));
    vf2_model2a_shutdown(&machine);
}

int main(void)
{
    test_single_record_then_future_threshold();
    test_immediate_record_expires_in_tail();
    test_two_records_reuse_same_slot();
    test_threshold_stop_still_advances_existing_slots();

    if (failures != 0) {
        fprintf(stderr, "%d player 0x28178 stream test(s) failed\n", failures);
        return 1;
    }
    printf("player 0x28178 semantic stream tests passed\n");
    return 0;
}
