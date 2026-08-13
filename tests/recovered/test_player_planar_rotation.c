#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vf2/model2a.h"
#include "vf2/status.h"

#include "../../src/recovered/player_i960_bridge_planar_rotation.inc"
#include "../../src/recovered/player_selector_setup.inc"

#define SELECTOR_TEST_PLAYER UINT32_C(0x00512000)
#define SELECTOR_TEST_BRANCH_BASE UINT32_C(0x00518000)
#define SELECTOR_TEST_DATA UINT32_C(0x02040000)
#define SELECTOR_TEST_TABLE UINT32_C(0x02050000)

static int failures = 0;

#define CHECK(expression)                                           \
    do {                                                            \
        if (!(expression)) {                                        \
            fprintf(stderr, "FAILED %s:%d: %s\n",                \
                    __FILE__, __LINE__, #expression);              \
            ++failures;                                             \
        }                                                           \
    } while (0)

static uint32_t bits(float value)
{
    uint32_t result = 0u;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static float value(uint32_t raw)
{
    float result = 0.0f;
    memcpy(&result, &raw, sizeof(result));
    return result;
}

static void check_rotation(
    int16_t angle,
    float x,
    float z,
    float expected_x,
    float expected_z
)
{
    uint32_t output_x = 0u;
    uint32_t output_z = 0u;

    CHECK(player_planar_rotation_apply(
        angle, bits(x), bits(z), &output_x, &output_z
    ) == VF2_OK);
    CHECK(value(output_x) == expected_x);
    CHECK(value(output_z) == expected_z);
}

static void test_cardinal_orientation(void)
{
    check_rotation(0, 2.0f, 3.0f, 2.0f, 3.0f);
    check_rotation((int16_t)UINT16_C(0x4000), 2.0f, 3.0f, 3.0f, -2.0f);
    check_rotation((int16_t)UINT16_C(0xc000), 2.0f, 3.0f, -3.0f, 2.0f);
    check_rotation((int16_t)UINT16_C(0x8000), 2.0f, 3.0f, -2.0f, -3.0f);
}

static void test_atan_edge_alignment_evidence(void)
{
    uint32_t output_x = 0u;
    uint32_t output_z = 0u;
    const float one = 1.0f;

    CHECK(player_planar_rotation_apply(
        (int16_t)UINT16_C(0x6000), bits(one), bits(one),
        &output_x, &output_z
    ) == VF2_OK);
    CHECK(value(output_x) == 0.0f);
    CHECK(value(output_z) < -1.4141f && value(output_z) > -1.4143f);
}

static void test_nonfinite_rejection(void)
{
    uint32_t output_x = 0u;
    uint32_t output_z = 0u;

    CHECK(player_planar_rotation_apply(
        0, UINT32_C(0x7f800000), bits(1.0f), &output_x, &output_z
    ) == VF2_ERROR_UNSUPPORTED);
    CHECK(player_planar_rotation_apply(
        0, bits(1.0f), UINT32_C(0x7fc00000), &output_x, &output_z
    ) == VF2_ERROR_UNSUPPORTED);
}

static vf2_status selector_test_write_u8(
    vf2_model2a *machine,
    uint32_t address,
    uint8_t value8
)
{
    return vf2_model2a_write(machine, address, &value8, sizeof(value8));
}

static vf2_status selector_test_write_u16(
    vf2_model2a *machine,
    uint32_t address,
    uint16_t value16
)
{
    const uint8_t raw[2] = {(uint8_t)value16, (uint8_t)(value16 >> 8u)};
    return vf2_model2a_write(machine, address, raw, sizeof(raw));
}

static uint8_t selector_test_read_u8(
    const vf2_model2a *machine,
    uint32_t address
)
{
    uint8_t result = 0u;
    CHECK(vf2_model2a_read(machine, address, &result, sizeof(result)) == VF2_OK);
    return result;
}

static uint16_t selector_test_read_u16(
    const vf2_model2a *machine,
    uint32_t address
)
{
    uint8_t raw[2] = {0u, 0u};
    CHECK(vf2_model2a_read(machine, address, raw, sizeof(raw)) == VF2_OK);
    return (uint16_t)((uint16_t)raw[0] | ((uint16_t)raw[1] << 8u));
}

static uint32_t selector_test_read_u32(
    const vf2_model2a *machine,
    uint32_t address
)
{
    uint32_t result = 0u;
    CHECK(vf2_model2a_read_u32(machine, address, &result) == VF2_OK);
    return result;
}

static int selector_test_prepare(vf2_model2a *machine)
{
    memset(machine, 0, sizeof(*machine));
    if (!vf2_model2a_initialize(machine)) {
        return 0;
    }
    memset(machine->work_ram, 0, machine->work_ram_size);
    memset(machine->main_data, 0, machine->main_data_size);
    CHECK(vf2_model2a_write_u32(
        machine, VF2_PLAYER_SELECTOR_BRANCH_BASE, SELECTOR_TEST_BRANCH_BASE
    ) == VF2_OK);
    CHECK(selector_test_write_u8(
        machine, SELECTOR_TEST_BRANCH_BASE + UINT32_C(0x3351), 0u
    ) == VF2_OK);
    return 1;
}

static void selector_test_bind(
    vf2_model2a *machine,
    uint32_t selector,
    uint32_t data,
    uint32_t table,
    uint16_t table_value
)
{
    CHECK(vf2_model2a_write_u32(
        machine,
        VF2_PLAYER_SELECTOR_DATA_TABLE + selector * UINT32_C(4), data
    ) == VF2_OK);
    CHECK(vf2_model2a_write_u32(
        machine,
        VF2_PLAYER_SELECTOR_SCRATCH_TABLE + selector * UINT32_C(4), table
    ) == VF2_OK);
    CHECK(selector_test_write_u16(machine, table, table_value) == VF2_OK);
}

static void test_selector_opcode1_then_8_non_505(void)
{
    vf2_model2a machine;
    vf2_player_selector_setup_plan plan;
    const uint32_t selector = UINT32_C(0x0123);
    const uint8_t base[8] = {0u, 2u, 0u, 0u, 0xfeu, 2u, 0x80u, 0x7fu};
    const uint8_t stream[4] = {1u, 0xaau, 0xbbu, 8u};

    CHECK(selector_test_prepare(&machine));
    if (machine.work_ram == NULL) {
        return;
    }
    selector_test_bind(
        &machine, selector, SELECTOR_TEST_DATA, SELECTOR_TEST_TABLE,
        UINT16_C(0x3456)
    );
    CHECK(vf2_model2a_write(
        &machine, SELECTOR_TEST_DATA, base, sizeof(base)
    ) == VF2_OK);
    CHECK(vf2_model2a_write(
        &machine, SELECTOR_TEST_DATA + UINT32_C(8), stream, sizeof(stream)
    ) == VF2_OK);
    CHECK(vf2_model2a_write_u32(
        &machine, SELECTOR_TEST_PLAYER + UINT32_C(0x01a4),
        UINT32_C(0xdeadbeef)
    ) == VF2_OK);
    CHECK(vf2_model2a_write_u32(
        &machine, SELECTOR_TEST_PLAYER + UINT32_C(0x0804),
        UINT32_C(0x12345678)
    ) == VF2_OK);

    CHECK(player_selector_execute_setup(
        &machine, SELECTOR_TEST_PLAYER, selector, &plan
    ) == VF2_OK);
    CHECK(plan.selector == selector);
    CHECK(plan.opcode_count == 2u);
    CHECK(plan.final_cursor == SELECTOR_TEST_DATA + UINT32_C(12));
    CHECK(selector_test_read_u32(
        &machine, SELECTOR_TEST_PLAYER + UINT32_C(0x0bd4)
    ) == UINT32_C(0xdeadbeef));
    CHECK(selector_test_read_u32(
        &machine, SELECTOR_TEST_PLAYER + UINT32_C(0x0c30)
    ) == UINT32_C(0x12345678));
    CHECK(selector_test_read_u16(
        &machine, SELECTOR_TEST_PLAYER + UINT32_C(0x0800)
    ) == UINT16_C(0x3456));
    CHECK(selector_test_read_u8(
        &machine, SELECTOR_TEST_PLAYER + UINT32_C(0x0802)
    ) == UINT8_C(0xaa));
    CHECK(selector_test_read_u8(
        &machine, SELECTOR_TEST_PLAYER + UINT32_C(0x0803)
    ) == UINT8_C(0xbb));
    CHECK(selector_test_read_u32(
        &machine, SELECTOR_TEST_PLAYER + UINT32_C(0x082c)
    ) == SELECTOR_TEST_DATA + UINT32_C(12));
    vf2_model2a_shutdown(&machine);
}

static void test_selector_opcode16_variable_length(void)
{
    vf2_model2a machine;
    vf2_player_selector_setup_plan plan;
    const uint32_t selector = UINT32_C(0x0700);
    const uint8_t base[8] = {0u};
    const uint8_t stream[9] = {
        16u, 3u, 0x10u, 0x11u, 0x20u, 0x21u, 0x30u, 0x31u, 8u
    };

    CHECK(selector_test_prepare(&machine));
    if (machine.work_ram == NULL) {
        return;
    }
    selector_test_bind(
        &machine, selector, SELECTOR_TEST_DATA, SELECTOR_TEST_TABLE,
        UINT16_C(0x9999)
    );
    CHECK(vf2_model2a_write(
        &machine, SELECTOR_TEST_DATA, base, sizeof(base)
    ) == VF2_OK);
    CHECK(vf2_model2a_write(
        &machine, SELECTOR_TEST_DATA + UINT32_C(8), stream, sizeof(stream)
    ) == VF2_OK);

    CHECK(player_selector_execute_setup(
        &machine, SELECTOR_TEST_PLAYER, selector, &plan
    ) == VF2_OK);
    CHECK(plan.final_cursor == SELECTOR_TEST_DATA + UINT32_C(17));
    CHECK(selector_test_read_u32(
        &machine, SELECTOR_TEST_PLAYER + UINT32_C(0x0854)
    ) == SELECTOR_TEST_DATA + UINT32_C(10));
    CHECK(selector_test_read_u16(
        &machine, SELECTOR_TEST_PLAYER + UINT32_C(0x0800)
    ) == UINT16_C(0x30));
    vf2_model2a_shutdown(&machine);
}

static void test_selector_unsupported_opcode_is_transactional(void)
{
    vf2_model2a machine;
    vf2_player_selector_setup_plan plan;
    const uint32_t selector = UINT32_C(0x0222);
    const uint8_t base[8] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
    const uint8_t opcode2[7] = {2u, 1u, 2u, 3u, 4u, 5u, 6u};

    CHECK(selector_test_prepare(&machine));
    if (machine.work_ram == NULL) {
        return;
    }
    selector_test_bind(
        &machine, selector, SELECTOR_TEST_DATA, SELECTOR_TEST_TABLE,
        UINT16_C(0x1111)
    );
    CHECK(vf2_model2a_write(
        &machine, SELECTOR_TEST_DATA, base, sizeof(base)
    ) == VF2_OK);
    CHECK(vf2_model2a_write(
        &machine, SELECTOR_TEST_DATA + UINT32_C(8), opcode2, sizeof(opcode2)
    ) == VF2_OK);
    CHECK(vf2_model2a_write_u32(
        &machine, SELECTOR_TEST_PLAYER + UINT32_C(0x0804),
        UINT32_C(0x76543210)
    ) == VF2_OK);
    CHECK(vf2_model2a_write_u32(
        &machine, SELECTOR_TEST_PLAYER + UINT32_C(0x01a4),
        UINT32_C(0x12345678)
    ) == VF2_OK);

    CHECK(player_selector_execute_setup(
        &machine, SELECTOR_TEST_PLAYER, selector, &plan
    ) == VF2_ERROR_UNSUPPORTED);
    CHECK(selector_test_read_u32(
        &machine, SELECTOR_TEST_PLAYER + UINT32_C(0x0804)
    ) == UINT32_C(0x76543210));
    CHECK(selector_test_read_u32(
        &machine, SELECTOR_TEST_PLAYER + UINT32_C(0x01a4)
    ) == UINT32_C(0x12345678));
    CHECK(selector_test_read_u16(
        &machine, SELECTOR_TEST_PLAYER + UINT32_C(0x0800)
    ) == 0u);
    vf2_model2a_shutdown(&machine);
}

int main(void)
{
    test_cardinal_orientation();
    test_atan_edge_alignment_evidence();
    test_nonfinite_rejection();
    test_selector_opcode1_then_8_non_505();
    test_selector_opcode16_variable_length();
    test_selector_unsupported_opcode_is_transactional();

    if (failures != 0) {
        fprintf(stderr, "%d player recovery semantic test(s) failed\n", failures);
        return 1;
    }
    printf("player rotation and selector semantics tests passed\n");
    return 0;
}
