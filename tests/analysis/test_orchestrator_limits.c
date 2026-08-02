#include <stdint.h>
#include <stdio.h>

#include "vf2/analysis/orchestrator_limits.h"

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

static void test_default_branch_class(void)
{
    static const uint8_t accepted_modes[] = {
        0u, 1u, 4u, 5u, 8u, 10u, 11u, 16u, 31u
    };
    static const uint8_t unsupported_modes[] = {
        2u, 3u, 6u, 7u, 9u, 12u, 13u, 14u, 15u, 32u, 255u
    };
    size_t index = 0u;

    for (index = 0u;
         index < sizeof(accepted_modes) / sizeof(accepted_modes[0]);
         ++index) {
        uint32_t lower = 0u;
        uint32_t upper = 0u;
        CHECK(
            vf2_orchestrator_select_default_limits(
                0u,
                accepted_modes[index],
                &lower,
                &upper
            ) == VF2_OK
        );
        CHECK(lower == UINT32_C(0x00003e80));
        CHECK(upper == UINT32_C(0x00004e20));
    }

    for (index = 0u;
         index < sizeof(unsupported_modes) / sizeof(unsupported_modes[0]);
         ++index) {
        uint32_t lower = UINT32_C(0x11111111);
        uint32_t upper = UINT32_C(0x22222222);
        CHECK(
            vf2_orchestrator_select_default_limits(
                0u,
                unsupported_modes[index],
                &lower,
                &upper
            ) == VF2_ERROR_UNSUPPORTED
        );
        CHECK(lower == UINT32_C(0x11111111));
        CHECK(upper == UINT32_C(0x22222222));
    }

    {
        uint32_t lower = 0u;
        uint32_t upper = 0u;
        CHECK(
            vf2_orchestrator_select_default_limits(
                UINT32_C(1) << 16u,
                0u,
                &lower,
                &upper
            ) == VF2_ERROR_UNSUPPORTED
        );
        CHECK(
            vf2_orchestrator_select_default_limits(
                0u, 0u, NULL, &upper
            ) == VF2_ERROR_INVALID_ARGUMENT
        );
        CHECK(
            vf2_orchestrator_select_default_limits(
                0u, 0u, &lower, NULL
            ) == VF2_ERROR_INVALID_ARGUMENT
        );
    }
}

static void test_machine_application(void)
{
    vf2_model2a machine;
    vf2_orchestrator_limits_report report;
    uint32_t value = 0u;
    uint8_t mode = 0u;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }

    CHECK(
        vf2_model2a_write_u32(
            &machine, VF2_ORCHESTRATOR_RUNTIME_FLAGS, 0u
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write(
            &machine,
            VF2_ORCHESTRATOR_DISPLAY_MODE,
            &mode,
            sizeof(mode)
        ) == VF2_OK
    );
    CHECK(
        vf2_orchestrator_apply_default_limits(&machine, &report) == VF2_OK
    );
    CHECK(report.entry_address == VF2_ORCHESTRATOR_LIMITS_ENTRY);
    CHECK(report.runtime_flags == 0u);
    CHECK(report.display_mode == 0u);
    CHECK(report.lower_limit == UINT32_C(0x00003e80));
    CHECK(report.upper_limit == UINT32_C(0x00004e20));
    CHECK(report.interpreted_instruction_equivalent == UINT64_C(22));
    CHECK(report.bytes_written == 8u);
    CHECK(
        vf2_model2a_read_u32(
            &machine, VF2_ORCHESTRATOR_LIMIT_LOW, &value
        ) == VF2_OK
    );
    CHECK(value == UINT32_C(0x00003e80));
    CHECK(
        vf2_model2a_read_u32(
            &machine, VF2_ORCHESTRATOR_LIMIT_HIGH, &value
        ) == VF2_OK
    );
    CHECK(value == UINT32_C(0x00004e20));

    mode = 9u;
    CHECK(
        vf2_model2a_write(
            &machine,
            VF2_ORCHESTRATOR_DISPLAY_MODE,
            &mode,
            sizeof(mode)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine,
            VF2_ORCHESTRATOR_LIMIT_LOW,
            UINT32_C(0x11111111)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine,
            VF2_ORCHESTRATOR_LIMIT_HIGH,
            UINT32_C(0x22222222)
        ) == VF2_OK
    );
    CHECK(
        vf2_orchestrator_apply_default_limits(&machine, NULL) ==
        VF2_ERROR_UNSUPPORTED
    );
    CHECK(
        vf2_model2a_read_u32(
            &machine, VF2_ORCHESTRATOR_LIMIT_LOW, &value
        ) == VF2_OK
    );
    CHECK(value == UINT32_C(0x11111111));
    CHECK(
        vf2_model2a_read_u32(
            &machine, VF2_ORCHESTRATOR_LIMIT_HIGH, &value
        ) == VF2_OK
    );
    CHECK(value == UINT32_C(0x22222222));

    vf2_model2a_shutdown(&machine);
}

int main(void)
{
    test_default_branch_class();
    test_machine_application();

    if (failures != 0) {
        fprintf(stderr, "%d orchestrator-limit test(s) failed\n", failures);
        return 1;
    }
    printf("orchestrator-limit tests passed\n");
    return 0;
}
