#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/i960/executor.h"
#include "vf2/i960/snapshot.h"
#include "vf2/model2a.h"
#include "vf2/rom.h"

static int failures = 0;

#define CHECK(expression)                                           \
    do {                                                            \
        if (!(expression)) {                                        \
            fprintf(stderr, "FAILED %s:%d: %s\n",                 \
                    __FILE__, __LINE__, #expression);               \
            ++failures;                                             \
        }                                                           \
    } while (0)

typedef struct phase17_zero_case {
    const char *name;
    uint8_t menu_index;
    uint32_t runtime_flags;
    uint32_t input_flags;
    uint32_t navigation_flags;
    uint32_t previous_flags;
    uint8_t menu_state;
    uint32_t seed_player0_offset;
    uint16_t seed_player0_value;
    uint64_t expected_instructions;
    uint64_t expected_calls;
    uint64_t expected_returns;
    uint32_t expected_depth;
} phase17_zero_case;

typedef struct phase17_scalar_copro {
    uint32_t words[3];
    size_t count;
    uint32_t result;
    int ready;
} phase17_scalar_copro;

static float phase17_float_from_bits(uint32_t bits)
{
    float value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t phase17_float_to_bits(float value)
{
    uint32_t bits = 0u;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static vf2_status phase17_copro_write(
    void *context,
    uint32_t address,
    const void *source,
    size_t size
)
{
    phase17_scalar_copro *copro = context;
    uint32_t value = 0u;
    float left = 0.0f;
    float right = 0.0f;

    if (copro == NULL || source == NULL || size != sizeof(value) ||
        address < UINT32_C(0x00884000) ||
        address >= UINT32_C(0x00888000) || copro->count >= 3u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memcpy(&value, source, sizeof(value));
    copro->words[copro->count++] = value;
    if (copro->count != 3u) {
        return VF2_OK;
    }

    left = phase17_float_from_bits(copro->words[1]);
    right = phase17_float_from_bits(copro->words[2]);
    switch (copro->words[0]) {
    case UINT32_C(0x09801313):
        copro->result = phase17_float_to_bits(left + right);
        break;
    case UINT32_C(0x0a001414):
        copro->result = phase17_float_to_bits(left - right);
        break;
    default:
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    copro->ready = 1;
    return VF2_OK;
}

static vf2_status phase17_copro_read(
    void *context,
    uint32_t address,
    void *destination,
    size_t size
)
{
    phase17_scalar_copro *copro = context;

    if (copro == NULL || destination == NULL ||
        size != sizeof(copro->result) ||
        address < UINT32_C(0x00884000) ||
        address >= UINT32_C(0x00888000) || !copro->ready) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memcpy(destination, &copro->result, sizeof(copro->result));
    copro->count = 0u;
    copro->ready = 0;
    return VF2_OK;
}

static int check_status(vf2_status status)
{
    return status == VF2_OK;
}

static vf2_status write_u8(vf2_model2a *machine, uint32_t address, uint8_t value)
{
    return vf2_model2a_write(machine, address, &value, sizeof(value));
}

static vf2_status write_u16(vf2_model2a *machine, uint32_t address, uint16_t value)
{
    return vf2_model2a_write(machine, address, &value, sizeof(value));
}

static vf2_status initialize_phase17_zero_state(
    vf2_model2a *machine,
    const phase17_zero_case *test_case
)
{
    const uint32_t player0 = UINT32_C(0x00510000);
    const uint32_t player1 = UINT32_C(0x00512000);
    const uint32_t control = UINT32_C(0x00514000);
    const uint32_t descriptor = UINT32_C(0x00516000);
    const uint32_t associated0 = UINT32_C(0x00518200);
    const uint32_t associated1 = UINT32_C(0x00518300);
    uint16_t fighter_control = UINT16_C(0x1234);
    vf2_status status = VF2_OK;

    status = vf2_model2a_write_u32(
        machine, UINT32_C(0x00508000), test_case->runtime_flags);
    if (status == VF2_OK) {
        status = write_u8(machine, UINT32_C(0x0050002a), UINT8_C(17));
    }
    if (status == VF2_OK) {
        status = write_u8(machine, UINT32_C(0x005000a6), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500804), player0);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500808), player1);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500814), control);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0050081c), descriptor);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, descriptor + UINT32_C(0x0c), UINT32_C(0x0001b9ac));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500700), test_case->input_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500704), test_case->navigation_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x0050070c), test_case->previous_flags);
    }
    if (status == VF2_OK) {
        status = write_u8(
            machine, UINT32_C(0x00508008), test_case->menu_index);
    }
    if (status == VF2_OK) {
        status = write_u8(
            machine, UINT32_C(0x00500085), test_case->menu_state);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x005000a0), &fighter_control,
            sizeof(fighter_control));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, player0, UINT32_MAX);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, player1, UINT32_MAX);
    }
    if (status == VF2_OK) {
        status = write_u8(machine, player0 + UINT32_C(0x1200), UINT8_C(0x5a));
    }
    if (status == VF2_OK) {
        status = write_u8(machine, player1 + UINT32_C(0x1200), UINT8_C(0xa5));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500860), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x0050083c), UINT32_C(0x00518000));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00518000), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500840), UINT32_C(0x00518100));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00518100), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500868), associated0);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x0050086c), associated1);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, associated0, 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, associated1, 0u);
    }
    if (status == VF2_OK) {
        status = write_u8(machine, UINT32_C(0x0050a0b8), 0u);
    }
    if (status == VF2_OK) {
        status = write_u8(machine, UINT32_C(0x0050a0b9), 0u);
    }
    if (status == VF2_OK && test_case->seed_player0_offset != 0u) {
        status = write_u16(
            machine, player0 + test_case->seed_player0_offset,
            test_case->seed_player0_value);
    }
    return status;
}

static vf2_status enter_frame_dispatch(vf2_i960_cpu *cpu)
{
    vf2_i960_cpu_reset(cpu, 0u, 0u, UINT32_C(0x00001000));
    cpu->registers[1] = UINT32_C(0x00503000);
    return vf2_i960_cpu_enter_procedure(
        cpu, UINT32_C(0x0000a6c0), UINT32_C(0x00001004));
}

static void run_case(
    const uint8_t *main_rom,
    size_t main_rom_size,
    const uint8_t *main_data,
    size_t main_data_size,
    const phase17_zero_case *test_case
)
{
    vf2_model2a reference_machine;
    vf2_model2a native_machine;
    vf2_i960_cpu reference_cpu;
    vf2_i960_cpu native_cpu;
    vf2_i960_run_options options;
    vf2_i960_run_result run_result;
    vf2_hybrid_bridge_report bridge_report;
    vf2_i960_snapshot_diff diff;
    phase17_scalar_copro reference_copro;
    phase17_scalar_copro native_copro;
    vf2_status reference_status = VF2_OK;
    vf2_status native_status = VF2_OK;
    vf2_status compare_status = VF2_OK;
    const int use_copro_oracle =
        test_case->runtime_flags == 0u &&
        test_case->navigation_flags == 0u &&
        test_case->previous_flags == test_case->input_flags &&
        test_case->menu_state == UINT8_C(0x40) &&
        (test_case->menu_index == UINT8_C(3) ||
         test_case->menu_index == UINT8_C(5) ||
         test_case->menu_index == UINT8_C(12));

    memset(&reference_machine, 0, sizeof(reference_machine));
    memset(&native_machine, 0, sizeof(native_machine));
    memset(&reference_copro, 0, sizeof(reference_copro));
    memset(&native_copro, 0, sizeof(native_copro));
    memset(&options, 0, sizeof(options));
    memset(&run_result, 0, sizeof(run_result));
    memset(&bridge_report, 0, sizeof(bridge_report));
    memset(&diff, 0, sizeof(diff));

    CHECK(vf2_model2a_initialize(&reference_machine));
    CHECK(vf2_model2a_initialize(&native_machine));
    if (reference_machine.work_ram == NULL || native_machine.work_ram == NULL) {
        vf2_model2a_shutdown(&reference_machine);
        vf2_model2a_shutdown(&native_machine);
        return;
    }
    CHECK(vf2_model2a_attach_main_rom(
              &reference_machine, main_rom, main_rom_size) == VF2_OK);
    CHECK(vf2_model2a_attach_main_rom(
              &native_machine, main_rom, main_rom_size) == VF2_OK);
    CHECK(vf2_model2a_attach_main_data(
              &reference_machine, main_data, main_data_size) == VF2_OK);
    CHECK(vf2_model2a_attach_main_data(
              &native_machine, main_data, main_data_size) == VF2_OK);
    if (use_copro_oracle) {
        CHECK(vf2_model2a_set_copro_callbacks(
                  &reference_machine, phase17_copro_read,
                  phase17_copro_write, &reference_copro) == VF2_OK);
        CHECK(vf2_model2a_set_copro_callbacks(
                  &native_machine, phase17_copro_read,
                  phase17_copro_write, &native_copro) == VF2_OK);
    }
    CHECK(check_status(initialize_phase17_zero_state(
              &reference_machine, test_case)));
    CHECK(check_status(initialize_phase17_zero_state(
              &native_machine, test_case)));
    CHECK(enter_frame_dispatch(&reference_cpu) == VF2_OK);
    CHECK(enter_frame_dispatch(&native_cpu) == VF2_OK);
    if (use_copro_oracle) {
        reference_cpu.registers[VF2_I960_G0_REGISTER + 11u] =
            UINT32_C(0x00884000);
        reference_cpu.registers[VF2_I960_G0_REGISTER + 12u] = 0u;
        native_cpu.registers[VF2_I960_G0_REGISTER + 11u] =
            UINT32_C(0x00884000);
        native_cpu.registers[VF2_I960_G0_REGISTER + 12u] = 0u;
    }

    options.stop_address = UINT32_C(0x00001004);
    options.max_steps = UINT64_C(200000);
    reference_status = vf2_i960_run(
        &reference_cpu, &reference_machine, &options, &run_result);
    native_status = vf2_hybrid_post_frame_bridge_execute(
        &native_machine, &native_cpu, &bridge_report);
    compare_status = vf2_i960_compare_live_state(
        &reference_cpu, &reference_machine,
        &native_cpu, &native_machine, &diff);

    if (reference_status != VF2_OK || native_status != VF2_OK ||
        compare_status != VF2_OK || !diff.equal) {
        fprintf(
            stderr,
            "phase17-zero %s ref=%d native=%d compare=%d "
            "component=%s offset=%zu expected=0x%08x actual=0x%08x\n",
            test_case->name, (int)reference_status, (int)native_status,
            (int)compare_status, diff.component, diff.first_offset,
            (unsigned)diff.expected_value, (unsigned)diff.actual_value);
    }
    CHECK(reference_status == VF2_OK);
    CHECK(run_result.halt_reason == VF2_I960_HALT_STOP_ADDRESS);
    CHECK(reference_cpu.executed_instructions == test_case->expected_instructions);
    CHECK(native_status == VF2_OK);
    CHECK(bridge_report.recovered_instruction_count ==
          test_case->expected_instructions);
    CHECK(bridge_report.recovered_procedure_calls == test_case->expected_calls);
    CHECK(bridge_report.recovered_procedure_returns == test_case->expected_returns);
    CHECK(native_cpu.maximum_local_frame_depth == test_case->expected_depth);
    CHECK(compare_status == VF2_OK);
    CHECK(diff.equal);

    vf2_model2a_shutdown(&reference_machine);
    vf2_model2a_shutdown(&native_machine);
}

#define IDLE_CASE(label, index, input, navigation, seed_offset, seed_value, instructions, calls, returns, depth) \
    { label, UINT8_C(index), 0u, (uint32_t)(input), (uint32_t)(navigation), \
      (uint32_t)(input), UINT8_C(0x40), (uint32_t)(seed_offset), \
      (uint16_t)(seed_value), UINT64_C(instructions), UINT64_C(calls), \
      UINT64_C(returns), UINT32_C(depth) }

#define INPUT_EDGE_CASE(label, index, input, previous, state, instructions, calls, returns, depth) \
    { label, UINT8_C(index), 0u, (uint32_t)(input), 0u, \
      (uint32_t)(previous), UINT8_C(state), 0u, 0u, UINT64_C(instructions), \
      UINT64_C(calls), UINT64_C(returns), UINT32_C(depth) }

#define TRANSITION_CASE(label, index, runtime, navigation, instructions, calls, returns, depth) \
    { label, UINT8_C(index), (uint32_t)(runtime), UINT32_C(1) << 5u, \
      (uint32_t)(navigation), UINT32_C(1) << 5u, UINT8_C(0x40), 0u, 0u, \
      UINT64_C(instructions), UINT64_C(calls), UINT64_C(returns), UINT32_C(depth) }

int main(int argc, char **argv)
{
    static const phase17_zero_case cases[] = {
        IDLE_CASE("index0-control-test", 0, 0, 0, 0, 0, 267, 6, 7, 5),
        {"index0-control-test-blank", 0u, UINT32_C(1) << 9u,
         0u, 0u, 0u, UINT8_C(0x40), 0u, 0u, UINT64_C(266), UINT64_C(6), UINT64_C(7),
         UINT32_C(5)},
        IDLE_CASE("index1-motion", 1, 0, 0, 0, 0, 534, 9, 10, 4),
        IDLE_CASE("index2-command", 2, 0, 0, 0, 0, 318, 8, 9, 4),
        IDLE_CASE("index3-robot-position", 3, 0, 0, 0, 0, 695, 12, 13, 4),
        IDLE_CASE("index3-angle-mode", 3, (1u << 9u), 0, 0, 0,
                  688, 12, 13, 4),
        IDLE_CASE("index3-second-float-dec", 3, (1u << 12u), 0, 0, 0,
                  700, 12, 13, 4),
        IDLE_CASE("index3-second-float-inc", 3, (1u << 13u), 0, 0, 0,
                  700, 12, 13, 4),
        IDLE_CASE("index3-first-float-inc", 3, (1u << 14u), 0, 0, 0,
                  700, 12, 13, 4),
        IDLE_CASE("index3-first-float-dec", 3, (1u << 15u), 0, 0, 0,
                  700, 12, 13, 4),
        IDLE_CASE("index3-angle-ignore-dec", 3,
                  (1u << 9u) | (1u << 12u), 0, 0, 0,
                  688, 12, 13, 4),
        IDLE_CASE("index3-angle-ignore-inc", 3,
                  (1u << 9u) | (1u << 13u), 0, 0, 0,
                  688, 12, 13, 4),
        IDLE_CASE("index3-angle-inc", 3,
                  (1u << 9u) | (1u << 14u), 0, 0, 0,
                  699, 12, 13, 4),
        IDLE_CASE("index3-angle-dec", 3,
                  (1u << 9u) | (1u << 15u), 0, 0, 0,
                  704, 12, 13, 4),
        IDLE_CASE("index4-camera-mode", 4, 0, 0, 0, 0, 37, 2, 3, 3),
        IDLE_CASE("index5-camera-position", 5, 0, 0, 0, 0, 695, 13, 14, 4),
        IDLE_CASE("index5-select-y", 5, (1u << 8u), 0, 0, 0,
                  694, 13, 14, 4),
        IDLE_CASE("index5-step-tenth", 5, (1u << 9u), 0, 0, 0,
                  697, 13, 14, 4),
        IDLE_CASE("index5-second-dec", 5, (1u << 12u), 0, 0, 0,
                  700, 13, 14, 4),
        IDLE_CASE("index5-second-inc", 5, (1u << 13u), 0, 0, 0,
                  700, 13, 14, 4),
        IDLE_CASE("index5-first-inc", 5, (1u << 14u), 0, 0, 0,
                  700, 13, 14, 4),
        IDLE_CASE("index5-first-dec", 5, (1u << 15u), 0, 0, 0,
                  700, 13, 14, 4),
        IDLE_CASE("index5-y-dec", 5, (1u << 8u) | (1u << 12u), 0, 0, 0,
                  699, 13, 14, 4),
        IDLE_CASE("index5-y-inc", 5, (1u << 8u) | (1u << 13u), 0, 0, 0,
                  699, 13, 14, 4),
        IDLE_CASE("index5-tenth-second-dec", 5,
                  (1u << 9u) | (1u << 12u), 0, 0, 0,
                  702, 13, 14, 4),
        IDLE_CASE("index5-tenth-second-inc", 5,
                  (1u << 9u) | (1u << 13u), 0, 0, 0,
                  702, 13, 14, 4),
        IDLE_CASE("index5-tenth-first-inc", 5,
                  (1u << 9u) | (1u << 14u), 0, 0, 0,
                  702, 13, 14, 4),
        IDLE_CASE("index5-tenth-first-dec", 5,
                  (1u << 9u) | (1u << 15u), 0, 0, 0,
                  702, 13, 14, 4),
        IDLE_CASE("index6-camera-position2", 6, 0, 0, 0, 0, 3063, 37, 38, 4),
        IDLE_CASE("index7-camera-average", 7, 0, 0, 0, 0, 146, 4, 5, 4),
        IDLE_CASE("index8-hiji", 8, 0, 0, 0, 0, 45, 2, 3, 3),
        IDLE_CASE("index8-mode-buttons", 8, 0, (1u << 8u) | (1u << 9u),
                  0, 0, 49, 2, 3, 3),
        IDLE_CASE("index8-increment", 8, (1u << 14u), 0,
                  0, 0, 50, 2, 3, 3),
        IDLE_CASE("index8-decrement-noop", 8, (1u << 15u), 0,
                  0, 0, 44, 2, 3, 3),
        IDLE_CASE("index8-decrement-active", 8, (1u << 15u), 0,
                  0x158, 0xa001, 49, 2, 3, 3),
        IDLE_CASE("index8-increment-wrap", 8, (1u << 14u), 0,
                  0x158, 0xfeff, 50, 2, 3, 3),
        IDLE_CASE("index8-increment-limit", 8, (1u << 14u), 0,
                  0x158, 0xff00, 45, 2, 3, 3),
        IDLE_CASE("index9-material", 9, 0, 0, 0, 0, 1282, 17, 18, 4),
        IDLE_CASE("index10-polygon", 10, 0, 0, 0, 0, 1154, 18, 19, 4),
        IDLE_CASE("index11-ashi", 11, 0, 0, 0, 0, 40, 2, 3, 3),
        IDLE_CASE("index11-mode-buttons", 11, 0,
                  (1u << 8u) | (1u << 9u), 0, 0, 44, 2, 3, 3),
        IDLE_CASE("index11-increment", 11, (1u << 14u), 0,
                  0, 0, 42, 2, 3, 3),
        IDLE_CASE("index11-decrement", 11, (1u << 15u), 0,
                  0, 0, 41, 2, 3, 3),
        IDLE_CASE("index12-camera-xang", 12, 0, 0, 0, 0, 118, 4, 5, 4),
        IDLE_CASE("index12-camera-xang-dec", 12, (1u << 12u), 0, 0, 0,
                  119, 4, 5, 4),
        IDLE_CASE("index12-camera-xang-inc", 12, (1u << 13u), 0, 0, 0,
                  119, 4, 5, 4),
        IDLE_CASE("index13-texture", 13, 0, 0, 0, 0, 1745, 16, 17, 5),
        INPUT_EDGE_CASE("index0-release", 0, 0, (1u << 5u), 0x41,
                        270, 6, 7, 5),
        INPUT_EDGE_CASE("index0-held", 0, (1u << 5u), (1u << 5u), 0x40,
                        269, 6, 7, 5),
        INPUT_EDGE_CASE("index0-held-latched", 0, (1u << 5u), (1u << 5u),
                        0x41, 27, 2, 3, 3),
        INPUT_EDGE_CASE("index4-release", 4, 0, (1u << 5u), 0x41,
                        40, 2, 3, 3),
        INPUT_EDGE_CASE("index4-held", 4, (1u << 5u), (1u << 5u), 0x40,
                        39, 2, 3, 3),
        INPUT_EDGE_CASE("index4-held-latched", 4, (1u << 5u), (1u << 5u),
                        0x41, 27, 2, 3, 3),
        INPUT_EDGE_CASE("index8-release", 8, 0, (1u << 5u), 0x41,
                        48, 2, 3, 3),
        INPUT_EDGE_CASE("index8-held", 8, (1u << 5u), (1u << 5u), 0x40,
                        47, 2, 3, 3),
        INPUT_EDGE_CASE("index8-held-latched", 8, (1u << 5u), (1u << 5u),
                        0x41, 27, 2, 3, 3),
        INPUT_EDGE_CASE("index11-release", 11, 0, (1u << 5u), 0x41,
                        43, 2, 3, 3),
        INPUT_EDGE_CASE("index11-held", 11, (1u << 5u), (1u << 5u), 0x40,
                        42, 2, 3, 3),
        INPUT_EDGE_CASE("index11-held-latched", 11, (1u << 5u), (1u << 5u),
                        0x41, 27, 2, 3, 3),
        INPUT_EDGE_CASE("index1-release", 1, 0, (1u << 5u), 0x41,
                        537, 9, 10, 4),
        INPUT_EDGE_CASE("index1-held", 1, (1u << 5u), (1u << 5u), 0x40,
                        536, 9, 10, 4),
        INPUT_EDGE_CASE("index1-held-latched", 1, (1u << 5u), (1u << 5u), 0x41,
                        27, 2, 3, 3),
        INPUT_EDGE_CASE("index2-release", 2, 0, (1u << 5u), 0x41,
                        321, 8, 9, 4),
        INPUT_EDGE_CASE("index2-held", 2, (1u << 5u), (1u << 5u), 0x40,
                        320, 8, 9, 4),
        INPUT_EDGE_CASE("index2-held-latched", 2, (1u << 5u), (1u << 5u), 0x41,
                        27, 2, 3, 3),
        INPUT_EDGE_CASE("index3-release", 3, 0, (1u << 5u), 0x41,
                        698, 12, 13, 4),
        INPUT_EDGE_CASE("index3-held", 3, (1u << 5u), (1u << 5u), 0x40,
                        697, 12, 13, 4),
        INPUT_EDGE_CASE("index3-held-latched", 3, (1u << 5u), (1u << 5u), 0x41,
                        27, 2, 3, 3),
        INPUT_EDGE_CASE("index5-release", 5, 0, (1u << 5u), 0x41,
                        698, 13, 14, 4),
        INPUT_EDGE_CASE("index5-held", 5, (1u << 5u), (1u << 5u), 0x40,
                        697, 13, 14, 4),
        INPUT_EDGE_CASE("index5-held-latched", 5, (1u << 5u), (1u << 5u), 0x41,
                        27, 2, 3, 3),
        INPUT_EDGE_CASE("index6-release", 6, 0, (1u << 5u), 0x41,
                        3066, 37, 38, 4),
        INPUT_EDGE_CASE("index6-held", 6, (1u << 5u), (1u << 5u), 0x40,
                        3065, 37, 38, 4),
        INPUT_EDGE_CASE("index6-held-latched", 6, (1u << 5u), (1u << 5u), 0x41,
                        27, 2, 3, 3),
        INPUT_EDGE_CASE("index7-release", 7, 0, (1u << 5u), 0x41,
                        149, 4, 5, 4),
        INPUT_EDGE_CASE("index7-held", 7, (1u << 5u), (1u << 5u), 0x40,
                        148, 4, 5, 4),
        INPUT_EDGE_CASE("index7-held-latched", 7, (1u << 5u), (1u << 5u), 0x41,
                        27, 2, 3, 3),
        INPUT_EDGE_CASE("index9-release", 9, 0, (1u << 5u), 0x41,
                        1285, 17, 18, 4),
        INPUT_EDGE_CASE("index9-held", 9, (1u << 5u), (1u << 5u), 0x40,
                        1284, 17, 18, 4),
        INPUT_EDGE_CASE("index9-held-latched", 9, (1u << 5u), (1u << 5u), 0x41,
                        27, 2, 3, 3),
        INPUT_EDGE_CASE("index10-release", 10, 0, (1u << 5u), 0x41,
                        1157, 18, 19, 4),
        INPUT_EDGE_CASE("index10-held", 10, (1u << 5u), (1u << 5u), 0x40,
                        1156, 18, 19, 4),
        INPUT_EDGE_CASE("index10-held-latched", 10, (1u << 5u), (1u << 5u), 0x41,
                        27, 2, 3, 3),
        INPUT_EDGE_CASE("index12-release", 12, 0, (1u << 5u), 0x41,
                        121, 4, 5, 4),
        INPUT_EDGE_CASE("index12-held", 12, (1u << 5u), (1u << 5u), 0x40,
                        120, 4, 5, 4),
        INPUT_EDGE_CASE("index12-held-latched", 12, (1u << 5u), (1u << 5u), 0x41,
                        27, 2, 3, 3),
        INPUT_EDGE_CASE("index13-release", 13, 0, (1u << 5u), 0x41,
                        1748, 16, 17, 5),
        INPUT_EDGE_CASE("index13-held", 13, (1u << 5u), (1u << 5u), 0x40,
                        43, 2, 3, 3),
        INPUT_EDGE_CASE("index13-held-latched", 13, (1u << 5u), (1u << 5u), 0x41,
                        27, 2, 3, 3),
        TRANSITION_CASE("index0-to-1", 0, 0, (1u << 12u), 13016, 12, 13, 4),
        TRANSITION_CASE("index1-to-2", 1, 0, (1u << 12u), 12589, 10, 11, 4),
        TRANSITION_CASE("index2-to-3", 2, 0, (1u << 12u), 12966, 14, 15, 4),
        TRANSITION_CASE("index3-to-4", 3, 0, (1u << 12u), 13243, 14, 15, 4),
        TRANSITION_CASE("index4-to-5", 4, 0, (1u << 12u), 12972, 14, 15, 4),
        TRANSITION_CASE("index5-to-6", 5, 0, (1u << 12u), 15150, 34, 35, 4),
        TRANSITION_CASE("index6-to-7", 6, 0, (1u << 12u), 12416, 6, 7, 4),
        TRANSITION_CASE("index7-to-8", 7, 0, (1u << 12u), 12254, 4, 5, 4),
        TRANSITION_CASE("index9-to-8", 9, 0, (1u << 13u), 12254, 4, 5, 4),
        TRANSITION_CASE("index8-to-9", 8, 0, (1u << 12u), 13481, 18, 19, 4),
        TRANSITION_CASE("index9-to-10", 9, 0, (1u << 12u), 13386, 20, 21, 4),
        TRANSITION_CASE("index10-to-11", 10, 0, (1u << 12u), 12249, 4, 5, 4),
        TRANSITION_CASE("index12-to-11", 12, 0, (1u << 13u), 12249, 4, 5, 4),
        TRANSITION_CASE("index11-to-12", 11, 0, (1u << 12u), 12365, 6, 7, 4),
        TRANSITION_CASE("index2-to-1", 2, 0, (1u << 13u), 13016, 12, 13, 4),
        TRANSITION_CASE("index3-to-2", 3, 0, (1u << 13u), 12589, 10, 11, 4),
        TRANSITION_CASE("index4-to-3", 4, 0, (1u << 13u), 12966, 14, 15, 4),
        TRANSITION_CASE("index5-to-4", 5, 0, (1u << 13u), 13243, 14, 15, 4),
        TRANSITION_CASE("index6-to-5", 6, 0, (1u << 13u), 12972, 14, 15, 4),
        TRANSITION_CASE("index7-to-6", 7, 0, (1u << 13u), 15150, 34, 35, 4),
        TRANSITION_CASE("index8-to-7", 8, 0, (1u << 13u), 12416, 6, 7, 4),
        TRANSITION_CASE("index9-to-8", 9, 0, (1u << 13u), 12254, 4, 5, 4),
        TRANSITION_CASE("index10-to-9", 10, 0, (1u << 13u), 13481, 18, 19, 4),
        TRANSITION_CASE("index11-to-10", 11, 0, (1u << 13u), 13386, 20, 21, 4),
        TRANSITION_CASE("index12-to-11", 12, 0, (1u << 13u), 12249, 4, 5, 4),
        TRANSITION_CASE("index13-to-12", 13, 0, (1u << 13u), 12365, 6, 7, 4),
        TRANSITION_CASE("index12-to-13", 12, 0, (1u << 12u), 12288, 4, 5, 4),
        TRANSITION_CASE("index0-to-13-wrap", 0, 0, (1u << 13u), 12289, 4, 5, 4),
        TRANSITION_CASE("index13-to-0-wrap", 13, 0, (1u << 12u), 12475, 7, 8, 5),
        TRANSITION_CASE("index1-to-0", 1, 0, (1u << 13u), 12473, 7, 8, 5),
        TRANSITION_CASE("index13-to-0-wrap-blank", 13, (1u << 9u),
                        (1u << 12u), 12348, 6, 7, 5),
        TRANSITION_CASE("index1-to-0-blank", 1, (1u << 9u),
                        (1u << 13u), 12346, 6, 7, 5),
    };
    uint8_t *main_rom = NULL;
    uint8_t *main_data = NULL;
    size_t main_rom_size = 0u;
    size_t main_data_size = 0u;
    size_t index = 0u;

    if (argc != 2) {
        fprintf(stderr, "usage: %s ROM_DIR\n", argv[0]);
        return EXIT_FAILURE;
    }
    CHECK(vf2_romset_build_region(
              argv[1], VF2_REGION_MAINCPU,
              &main_rom, &main_rom_size) == VF2_OK);
    CHECK(vf2_romset_build_region(
              argv[1], VF2_REGION_MAIN_DATA,
              &main_data, &main_data_size) == VF2_OK);
    if (main_rom == NULL || main_data == NULL) {
        free(main_data);
        free(main_data);
    free(main_rom);
        return EXIT_FAILURE;
    }

    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        run_case(main_rom, main_rom_size, main_data, main_data_size, &cases[index]);
    }
    free(main_data);
    free(main_rom);

    if (failures != 0) {
        fprintf(stderr, "%d phase17-zero differential test(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    printf("phase17-zero differential tests passed: %zu cases\n",
           sizeof(cases) / sizeof(cases[0]));
    return EXIT_SUCCESS;
}
