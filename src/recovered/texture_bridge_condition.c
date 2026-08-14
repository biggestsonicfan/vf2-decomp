#include "vf2/hybrid.h"

#define VF2_TEXTURE_ENTRY_GATE UINT32_C(0x0004bd00)
#define VF2_TEXTURE_STATUS_DISPATCH UINT32_C(0x0004bd24)
#define VF2_TEXTURE_ENTRY_STATE UINT32_C(0x0055000c)

#if defined(__GNUC__) || defined(__clang__)
vf2_status vf2_texture_entry_gate_with_condition(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
) __asm__("execute_texture_orchestrator_entry_gate");

vf2_status vf2_texture_entry_gate_with_condition(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->ip != VF2_TEXTURE_ENTRY_GATE || cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_model2a_read_u32(
        machine,
        VF2_TEXTURE_ENTRY_STATE,
        &cpu->registers[VF2_I960_G0_REGISTER]
    );
    if (status != VF2_OK) {
        return status;
    }
    if (cpu->registers[VF2_I960_G0_REGISTER] != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    cpu->ip = VF2_TEXTURE_STATUS_DISPATCH;
    cpu->executed_instructions += UINT64_C(2);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_ENTRY_GATE;
    report->entry_address = VF2_TEXTURE_ENTRY_GATE;
    report->exit_address = VF2_TEXTURE_STATUS_DISPATCH;
    report->iterations = UINT64_C(1);
    report->recovered_instruction_count = UINT64_C(2);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}
#endif
