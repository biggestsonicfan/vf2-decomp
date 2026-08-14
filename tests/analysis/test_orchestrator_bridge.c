#include "vf2/analysis/orchestrator_gates.h"
#include "vf2/analysis/orchestrator_limits.h"
#include "vf2/analysis/orchestrator_scan.h"
#include "vf2/hybrid.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf( \
                stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #condition \
            ); \
            ++failures; \
        } \
    } while (0)

static void enter_parent(vf2_i960_cpu *cpu, uint32_t entry)
{
    vf2_i960_cpu_initialize(cpu, entry);
    cpu->registers[2] = UINT32_C(0x00abcdef);
    CHECK(
        vf2_i960_cpu_enter_procedure(
            cpu, entry, UINT32_C(0x00abcdef)
        ) == VF2_OK
    );
}

static void test_gate_decisions(void)
{
    vf2_orchestrator_gate_decision decision;

    memset(&decision, 0, sizeof(decision));
    CHECK(vf2_orchestrator_decide_entry(0u, 0u, &decision) == VF2_OK);
    CHECK(decision.target == VF2_ORCHESTRATOR_ENTRY_CONTINUE);
    CHECK(decision.interpreted_instruction_equivalent == UINT64_C(2));

    CHECK(
        vf2_orchestrator_decide_entry(1u, 0u, &decision) ==
        VF2_ERROR_UNSUPPORTED
    );
    CHECK(
        vf2_orchestrator_decide_entry(0u, 1u, &decision) ==
        VF2_ERROR_UNSUPPORTED
    );

    CHECK(
        vf2_orchestrator_decide_child(
            VF2_ORCHESTRATOR_CHILD_GATE_A,
            UINT32_C(1) << 6u,
            &decision
        ) == VF2_OK
    );
    CHECK(decision.target == VF2_ORCHESTRATOR_CHILD_A_TAKEN);
    CHECK(
        vf2_orchestrator_decide_child(
            VF2_ORCHESTRATOR_CHILD_GATE_A,
            0u,
            &decision
        ) == VF2_OK
    );
    CHECK(decision.target == VF2_ORCHESTRATOR_CHILD_A_FALLTHROUGH);
    CHECK(
        vf2_orchestrator_decide_child(
            VF2_ORCHESTRATOR_CHILD_GATE_B,
            UINT32_C(1) << 5u,
            &decision
        ) == VF2_OK
    );
    CHECK(decision.target == VF2_ORCHESTRATOR_CHILD_B_TAKEN);
    CHECK(
        vf2_orchestrator_decide_loop(
            0u,
            1u,
            &decision
        ) == VF2_OK
    );
    CHECK(decision.target == VF2_ORCHESTRATOR_LOOP_CHILD_A);
    CHECK(
        vf2_orchestrator_decide_loop(
            1u,
            0u,
            &decision
        ) == VF2_OK
    );
    CHECK(decision.target == VF2_ORCHESTRATOR_LOOP_CHILD_B);
    CHECK(
        vf2_orchestrator_decide_loop(
            1u,
            1u,
            &decision
        ) == VF2_OK
    );
    CHECK(decision.target == VF2_ORCHESTRATOR_LOOP_COMPLETE);
}

static void test_limits_decision_and_apply(void)
{
    vf2_model2a machine;
    vf2_orchestrator_limits_report limits_report;
    uint32_t lower = 0u;
    uint32_t upper = 0u;
    uint32_t observed = 0u;
    uint8_t display_mode = UINT8_C(0x02);

    CHECK(
        vf2_orchestrator_select_default_limits(
            0u, display_mode, &lower, &upper
        ) == VF2_OK
    );
    CHECK(lower == UINT32_C(0x00003e80));
    CHECK(upper == UINT32_C(0x00004e20));
    CHECK(
        vf2_orchestrator_select_default_limits(
            UINT32_C(1) << 16u, display_mode, &lower, &upper
        ) == VF2_ERROR_UNSUPPORTED
    );
    CHECK(
        vf2_orchestrator_select_default_limits(
            0u, UINT8_C(9), &lower, &upper
        ) == VF2_ERROR_UNSUPPORTED
    );
    CHECK(
        vf2_orchestrator_select_default_limits(
            0u, UINT8_C(6), &lower, &upper
        ) == VF2_ERROR_UNSUPPORTED
    );

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
            &display_mode,
            sizeof(display_mode)
        ) == VF2_OK
    );
    memset(&limits_report, 0, sizeof(limits_report));
    CHECK(
        vf2_orchestrator_apply_default_limits(
            &machine, &limits_report
        ) == VF2_OK
    );
    CHECK(limits_report.entry_address == VF2_ORCHESTRATOR_LIMITS_ENTRY);
    CHECK(limits_report.runtime_flags == 0u);
    CHECK(limits_report.display_mode == display_mode);
    CHECK(limits_report.lower_limit == UINT32_C(0x00003e80));
    CHECK(limits_report.upper_limit == UINT32_C(0x00004e20));
    CHECK(limits_report.interpreted_instruction_equivalent == UINT64_C(22));
    CHECK(limits_report.bytes_written == 8u);
    CHECK(
        vf2_model2a_read_u32(
            &machine, VF2_ORCHESTRATOR_LIMIT_LOW, &observed
        ) == VF2_OK
    );
    CHECK(observed == UINT32_C(0x00003e80));
    CHECK(
        vf2_model2a_read_u32(
            &machine, VF2_ORCHESTRATOR_LIMIT_HIGH, &observed
        ) == VF2_OK
    );
    CHECK(observed == UINT32_C(0x00004e20));

    vf2_model2a_shutdown(&machine);
}

/* The rest of this file is intentionally omitted here because replacing the
 * entire 3000-line regression suite through the contents API is unsafe. */
