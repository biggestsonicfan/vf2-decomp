#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vf2/native_differential.h"

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

static int initialize_pair(
    vf2_model2a *reference_machine,
    vf2_model2a *native_machine
)
{
    memset(reference_machine, 0, sizeof(*reference_machine));
    memset(native_machine, 0, sizeof(*native_machine));
    if (!vf2_model2a_initialize(reference_machine)) {
        return 0;
    }
    if (!vf2_model2a_initialize(native_machine)) {
        vf2_model2a_shutdown(reference_machine);
        return 0;
    }
    return 1;
}

static void shutdown_pair(
    vf2_model2a *reference_machine,
    vf2_model2a *native_machine
)
{
    vf2_model2a_shutdown(reference_machine);
    vf2_model2a_shutdown(native_machine);
}

static void test_invalid_arguments(void)
{
    CHECK(
        vf2_native_differential_run_until(
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            0u,
            0u,
            NULL
        ) == VF2_ERROR_INVALID_ARGUMENT
    );
}

static void test_zero_length_match(void)
{
    vf2_model2a reference_machine;
    vf2_model2a native_machine;
    vf2_i960_cpu reference_cpu;
    vf2_i960_cpu native_cpu;
    vf2_native_runtime_state native_state;
    vf2_native_differential_report report;

    CHECK(initialize_pair(&reference_machine, &native_machine));
    if (reference_machine.work_ram == NULL || native_machine.work_ram == NULL) {
        return;
    }

    vf2_i960_cpu_reset(
        &reference_cpu,
        0u,
        0u,
        UINT32_C(0x12345678)
    );
    native_cpu = reference_cpu;
    CHECK(vf2_native_runtime_initialize(&native_state, 4u) == VF2_OK);
    memset(&report, 0xff, sizeof(report));

    CHECK(
        vf2_native_differential_run_until(
            &reference_machine,
            &reference_cpu,
            &native_machine,
            &native_cpu,
            &native_state,
            UINT32_C(0x12345678),
            0u,
            &report
        ) == VF2_OK
    );
    CHECK(report.reached_stop == 1);
    CHECK(report.blocks_compared == 0u);
    CHECK(report.reference_instructions_executed == 0u);
    CHECK(report.native_recovered_instructions == 0u);
    CHECK(report.diff.equal);
    CHECK(report.final_reference_address == UINT32_C(0x12345678));
    CHECK(report.final_native_address == UINT32_C(0x12345678));

    shutdown_pair(&reference_machine, &native_machine);
}

static void test_initial_ip_mismatch(void)
{
    vf2_model2a reference_machine;
    vf2_model2a native_machine;
    vf2_i960_cpu reference_cpu;
    vf2_i960_cpu native_cpu;
    vf2_native_runtime_state native_state;
    vf2_native_differential_report report;

    CHECK(initialize_pair(&reference_machine, &native_machine));
    if (reference_machine.work_ram == NULL || native_machine.work_ram == NULL) {
        return;
    }

    vf2_i960_cpu_reset(&reference_cpu, 0u, 0u, UINT32_C(0x1000));
    vf2_i960_cpu_reset(&native_cpu, 0u, 0u, UINT32_C(0x2000));
    CHECK(vf2_native_runtime_initialize(&native_state, 4u) == VF2_OK);
    memset(&report, 0, sizeof(report));

    CHECK(
        vf2_native_differential_run_until(
            &reference_machine,
            &reference_cpu,
            &native_machine,
            &native_cpu,
            &native_state,
            UINT32_C(0x3000),
            1u,
            &report
        ) == VF2_ERROR_UNSUPPORTED
    );
    CHECK(report.reached_stop == 0);
    CHECK(report.blocks_compared == 0u);
    CHECK(!report.diff.equal);
    CHECK(strcmp(report.diff.component, "cpu-ip") == 0);
    CHECK(report.diff.expected_value == UINT32_C(0x1000));
    CHECK(report.diff.actual_value == UINT32_C(0x2000));

    shutdown_pair(&reference_machine, &native_machine);
}

static void test_budget_exhaustion_is_explicit(void)
{
    vf2_model2a reference_machine;
    vf2_model2a native_machine;
    vf2_i960_cpu reference_cpu;
    vf2_i960_cpu native_cpu;
    vf2_native_runtime_state native_state;
    vf2_native_differential_report report;

    CHECK(initialize_pair(&reference_machine, &native_machine));
    if (reference_machine.work_ram == NULL || native_machine.work_ram == NULL) {
        return;
    }

    vf2_i960_cpu_reset(&reference_cpu, 0u, 0u, UINT32_C(0x1000));
    native_cpu = reference_cpu;
    CHECK(vf2_native_runtime_initialize(&native_state, 4u) == VF2_OK);
    memset(&report, 0, sizeof(report));

    CHECK(
        vf2_native_differential_run_until(
            &reference_machine,
            &reference_cpu,
            &native_machine,
            &native_cpu,
            &native_state,
            UINT32_C(0x2000),
            0u,
            &report
        ) == VF2_ERROR_UNSUPPORTED
    );
    CHECK(report.reached_stop == 0);
    CHECK(report.blocks_compared == 0u);
    CHECK(report.diff.equal);
    CHECK(report.final_reference_address == UINT32_C(0x1000));
    CHECK(report.final_native_address == UINT32_C(0x1000));

    shutdown_pair(&reference_machine, &native_machine);
}

static void test_native_unsupported_path_is_reported(void)
{
    vf2_model2a reference_machine;
    vf2_model2a native_machine;
    vf2_i960_cpu reference_cpu;
    vf2_i960_cpu native_cpu;
    vf2_native_runtime_state native_state;
    vf2_native_differential_report report;

    CHECK(initialize_pair(&reference_machine, &native_machine));
    if (reference_machine.work_ram == NULL || native_machine.work_ram == NULL) {
        return;
    }

    vf2_i960_cpu_reset(
        &reference_cpu,
        0u,
        0u,
        UINT32_C(0xdeadbeef)
    );
    native_cpu = reference_cpu;
    CHECK(vf2_native_runtime_initialize(&native_state, 4u) == VF2_OK);
    memset(&report, 0, sizeof(report));

    CHECK(
        vf2_native_differential_run_until(
            &reference_machine,
            &reference_cpu,
            &native_machine,
            &native_cpu,
            &native_state,
            UINT32_C(0xcafebabe),
            1u,
            &report
        ) == VF2_ERROR_UNSUPPORTED
    );
    CHECK(report.reached_stop == 0);
    CHECK(report.blocks_compared == 0u);
    CHECK(report.last_step.entry_address == UINT32_C(0xdeadbeef));
    CHECK(report.last_step.kind == VF2_NATIVE_RUNTIME_STEP_NONE);
    CHECK(native_state.blocks_executed == 0u);

    shutdown_pair(&reference_machine, &native_machine);
}

int main(void)
{
    test_invalid_arguments();
    test_zero_length_match();
    test_initial_ip_mismatch();
    test_budget_exhaustion_is_explicit();
    test_native_unsupported_path_is_reported();

    if (failures != 0) {
        fprintf(stderr, "%d native differential test(s) failed\n", failures);
        return EXIT_FAILURE;
    }

    puts("native differential tests passed");
    return EXIT_SUCCESS;
}
