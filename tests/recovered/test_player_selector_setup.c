#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vf2/model2a.h"
#include "vf2/status.h"

#include "../../src/recovered/player_selector_setup.inc"

#define PLAYER UINT32_C(0x00512000)
#define BRANCH_BASE UINT32_C(0x00518000)
#define DATA0 UINT32_C(0x02040000)
#define TABLE0 UINT32_C(0x02050000)

static int failures = 0;

#define CHECK(expression)                                           \
    do {                                                            \
        if (!(expression)) {                                        \
            fprintf(stderr, "FAILED %s:%d: %s\n",                \
                    __FILE__, __LINE__, #expression);              \
            ++failures;                                             \
        }                                                           \
    } while (0)

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

static int prepare_machine(vf2_model2a *machine)
{
    memset(machine, 0, sizeof(*machine));
    if (!vf2_model2a_initialize(machine)) {
        return 0;
    }
    memset(machine->work_ram, 0, machine->work_ram_size);
    memset(machine->main_data, 0, machine->main_data_size);
    CHECK(vf2_model2a_write_u32(
        machine, VF2_PLAYER_SELECTOR_BRANCH_BASE, BRANCH_BASE
    ) == VF2_OK);
    CHECK(write_u8(machine, BRANCH_BASE + UINT32_C(0x3351), 0u) == VF2_OK);
    return 1;
}

static void bind_selector(
    vf2_model2a *machine,
    uint32_t selector,
    uint32_t data,
    uint32_t table,
    uint16_t table_value
)
{
    CHECK(vf2_model2a_write_u32(
        machine,
        VF2_PLAYER_SELECTOR_DATA_TABLE + selector * UINT32_C(4),
        data
    ) == VF2_OK);
    CHECK(vf2_model2a_write_u32(
        machine,
        VF2_PLAYER_SELECTOR_SCRATCH_TABLE + selector * UINT32_C(4),
        table
    ) == VF2_OK);
    CHECK(write_u16(machine, table, table_value) == VF2_OK);
}

static void write_base(
    vf2_model2a *machine,
    uint32_t data,
    uint32_t flags,
    const uint8_t signed_bytes[4]
)
{
    CHECK(vf2_model2a_write_u32(machine, data, flags) == VF2_OK);
    CHECK(vf2_model2a_write(
        machine, data + UINT32_C(4), signed_bytes, 4u
    ) == VF2_OK);
}

static void test_opcode1_then_8_non_505_selector(void)
{
    vf2_model2a machine;
    vf2_player_selector_setup_plan plan;
    const uint32_t selector = UINT32_C(0x0123);
    const uint8_t signed_bytes[4] = {0xfeu, 0x02u, 0x80u, 0x7fu};
    const uint8_t stream[4] = {1u, 0xaau, 0xbbu, 8u};

    CHECK(prepare_machine(&machine));
    if (machine.work_ram == NULL) {
        return;
    }
    bind_selector(&machine, selector, DATA0, TABLE0, UINT16_C(0x3456));
    write_base(&machine, DATA0, UINT32_C(0x00200200), signed_bytes);
    CHECK(vf2_model2a_write(
        &machine, DATA0 + UINT32_C(8), stream, sizeof(stream)
    ) == VF2_OK);
    CHECK(vf2_model2a_write_u32(
        &machine, PLAYER + UINT32_C(0x01a4), UINT32_C(0xdeadbeef)
    ) == VF2_OK);
    CHECK(vf2_model2a_write_u32(
        &machine, PLAYER + UINT32_C(0x0804), UINT32_C(0x12345678)
    ) == VF2_OK);

    CHECK(player_selector_execute_setup(
        &machine, PLAYER, selector, &plan
    ) == VF2_OK);
    CHECK(plan.selector == selector);
    CHECK(plan.opcode_count == 2u);
    CHECK(plan.terminator == 8u);
    CHECK(plan.final_cursor == DATA0 + UINT32_C(12));
    CHECK(read_u32(&machine, PLAYER + UINT32_C(0x0bd4)) == UINT32_C(0xdeadbeef));
    CHECK(read_u32(&machine, PLAYER + UINT32_C(0x0c30)) == UINT32_C(0x12345678));
    CHECK(read_u16(&machine, PLAYER + UINT32_C(0x0800)) == UINT16_C(0x3456));
    CHECK(read_u16(&machine, PLAYER + UINT32_C(0x080a)) == UINT16_C(0x3456));
    CHECK(read_u16(&machine, PLAYER + UINT32_C(0x080c)) == UINT16_C(0x3456));
    CHECK(read_u32(&machine, PLAYER + UINT32_C(0x0804)) == UINT32_C(0x00000200));
    CHECK(read_u32(&machine, PLAYER + UINT32_C(0x01a4)) == UINT32_C(0x00000200));
    CHECK(read_u8(&machine, PLAYER + UINT32_C(0x0810)) == UINT8_C(0xfe));
    CHECK(read_u8(&machine, PLAYER + UINT32_C(0x0811)) == UINT8_C(0x02));
    CHECK(read_u8(&machine, PLAYER + UINT32_C(0x0814)) == UINT8_C(0x80));
    CHECK(read_u8(&machine, PLAYER + UINT32_C(0x0815)) == UINT8_C(0x7f));
    CHECK(read_u8(&machine, PLAYER + UINT32_C(0x0802)) == UINT8_C(0xaa));
    CHECK(read_u8(&machine, PLAYER + UINT32_C(0x0803)) == UINT8_C(0xbb));
    CHECK(read_u32(&machine, PLAYER + UINT32_C(0x082c)) == DATA0 + UINT32_C(12));
    CHECK(read_u32(&machine, PLAYER + UINT32_C(0x06d0)) == DATA0 + UINT32_C(12));
    vf2_model2a_shutdown(&machine);
}

static void test_opcode6_and_zero_terminator(void)
{
    vf2_model2a machine;
    vf2_player_selector_setup_plan plan;
    const uint32_t selector = UINT32_C(0x0310);
    const uint8_t signed_bytes[4] = {0u, 0u, 0u, 0u};
    const uint8_t stream[8] = {
        6u, 0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0u
    };

    CHECK(prepare_machine(&machine));
    if (machine.work_ram == NULL) {
        return;
    }
    bind_selector(&machine, selector, DATA0, TABLE0, UINT16_C(0x0042));
    write_base(&machine, DATA0, 0u, signed_bytes);
    CHECK(vf2_model2a_write(
        &machine, DATA0 + UINT32_C(8), stream, sizeof(stream)
    ) == VF2_OK);

    CHECK(player_selector_execute_setup(
        &machine, PLAYER, selector, &plan
    ) == VF2_OK);
    CHECK(plan.opcode_count == 2u);
    CHECK(plan.terminator == 0u);
    CHECK(plan.final_cursor == DATA0 + UINT32_C(15));
    CHECK(read_u8(&machine, PLAYER + UINT32_C(0x0818)) == UINT8_C(0x11));
    CHECK(read_u8(&machine, PLAYER + UINT32_C(0x0819)) == UINT8_C(0x22));
    CHECK(read_u8(&machine, PLAYER + UINT32_C(0x0812)) == UINT8_C(0x33));
    CHECK(read_u8(&machine, PLAYER + UINT32_C(0x0813)) == UINT8_C(0x44));
    CHECK(read_u8(&machine, PLAYER + UINT32_C(0x085a)) == UINT8_C(0x55));
    CHECK(read_u8(&machine, PLAYER + UINT32_C(0x085b)) == UINT8_C(0x66));
    CHECK(read_u32(&machine, PLAYER + UINT32_C(0x082c)) == DATA0 + UINT32_C(15));
    vf2_model2a_shutdown(&machine);
}

static void test_opcode16_variable_length(void)
{
    vf2_model2a machine;
    vf2_player_selector_setup_plan plan;
    const uint32_t selector = UINT32_C(0x0700);
    const uint8_t signed_bytes[4] = {0u, 0u, 0u, 0u};
    const uint8_t stream[9] = {
        16u, 3u, 0x10u, 0x11u, 0x20u, 0x21u, 0x30u, 0x31u, 8u
    };

    CHECK(prepare_machine(&machine));
    if (machine.work_ram == NULL) {
        return;
    }
    bind_selector(&machine, selector, DATA0, TABLE0, UINT16_C(0x9999));
    write_base(&machine, DATA0, 0u, signed_bytes);
    CHECK(vf2_model2a_write(
        &machine, DATA0 + UINT32_C(8), stream, sizeof(stream)
    ) == VF2_OK);

    CHECK(player_selector_execute_setup(
        &machine, PLAYER, selector, &plan
    ) == VF2_OK);
    CHECK(plan.final_cursor == DATA0 + UINT32_C(17));
    CHECK(read_u32(&machine, PLAYER + UINT32_C(0x0854)) == DATA0 + UINT32_C(10));
    /* Opcode 16 selects byte at cursor + count*2. */
    CHECK(read_u16(&machine, PLAYER + UINT32_C(0x0800)) == UINT16_C(0x30));
    vf2_model2a_shutdown(&machine);
}

static void test_special_base_flag_rules(void)
{
    vf2_model2a machine;
    vf2_player_selector_setup_plan plan;
    const uint8_t signed_bytes[4] = {0u, 0u, 0u, 0u};
    const uint8_t terminator = 8u;

    CHECK(prepare_machine(&machine));
    if (machine.work_ram == NULL) {
        return;
    }
    bind_selector(&machine, UINT32_C(40), DATA0, TABLE0, 1u);
    write_base(&machine, DATA0, 0u, signed_bytes);
    CHECK(write_u8(&machine, DATA0 + UINT32_C(8), terminator) == VF2_OK);
    CHECK(player_selector_execute_setup(
        &machine, PLAYER, UINT32_C(40), &plan
    ) == VF2_OK);
    CHECK((read_u32(&machine, PLAYER + UINT32_C(0x0804)) &
           (UINT32_C(1) << 7u)) != 0u);

    bind_selector(
        &machine, UINT32_C(0x00db), DATA0 + UINT32_C(0x100),
        TABLE0 + UINT32_C(0x100), 2u
    );
    write_base(
        &machine, DATA0 + UINT32_C(0x100), UINT32_C(1) << 25u, signed_bytes
    );
    CHECK(write_u8(
        &machine, DATA0 + UINT32_C(0x108), terminator
    ) == VF2_OK);
    CHECK(player_selector_execute_setup(
        &machine, PLAYER, UINT32_C(0x00db), &plan
    ) == VF2_OK);
    CHECK((read_u32(&machine, PLAYER + UINT32_C(0x0804)) &
           (UINT32_C(1) << 25u)) == 0u);
    vf2_model2a_shutdown(&machine);
}

static void test_unsupported_opcode_is_transactional(void)
{
    vf2_model2a machine;
    vf2_player_selector_setup_plan plan;
    const uint32_t selector = UINT32_C(0x0222);
    const uint8_t signed_bytes[4] = {1u, 2u, 3u, 4u};
    const uint8_t opcode2[7] = {2u, 1u, 2u, 3u, 4u, 5u, 6u};

    CHECK(prepare_machine(&machine));
    if (machine.work_ram == NULL) {
        return;
    }
    bind_selector(&machine, selector, DATA0, TABLE0, UINT16_C(0x1111));
    write_base(&machine, DATA0, UINT32_C(0xabcdef01), signed_bytes);
    CHECK(vf2_model2a_write(
        &machine, DATA0 + UINT32_C(8), opcode2, sizeof(opcode2)
    ) == VF2_OK);
    CHECK(vf2_model2a_write_u32(
        &machine, PLAYER + UINT32_C(0x0804), UINT32_C(0x76543210)
    ) == VF2_OK);
    CHECK(vf2_model2a_write_u32(
        &machine, PLAYER + UINT32_C(0x01a4), UINT32_C(0x12345678)
    ) == VF2_OK);

    CHECK(player_selector_execute_setup(
        &machine, PLAYER, selector, &plan
    ) == VF2_ERROR_UNSUPPORTED);
    CHECK(read_u32(&machine, PLAYER + UINT32_C(0x0804)) == UINT32_C(0x76543210));
    CHECK(read_u32(&machine, PLAYER + UINT32_C(0x01a4)) == UINT32_C(0x12345678));
    CHECK(read_u16(&machine, PLAYER + UINT32_C(0x0800)) == 0u);
    vf2_model2a_shutdown(&machine);
}

int main(void)
{
    test_opcode1_then_8_non_505_selector();
    test_opcode6_and_zero_terminator();
    test_opcode16_variable_length();
    test_special_base_flag_rules();
    test_unsupported_opcode_is_transactional();

    if (failures != 0) {
        fprintf(stderr, "%d player selector setup test(s) failed\n", failures);
        return 1;
    }
    printf("player selector setup bytecode tests passed\n");
    return 0;
}
