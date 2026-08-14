#include "texture_bridge_internal.h"

#define VF2_TEXTURE_STATUS_SELECTOR UINT32_C(0x0050002b)
#define VF2_TEXTURE_STATUS_SPECIAL_DESTINATION UINT32_C(0x010040e2)
#define VF2_TEXTURE_STATUS_COMMON_DESTINATION UINT32_C(0x010000e2)
#define VF2_TEXTURE_STATUS_SPECIAL_SOURCE UINT32_C(0x0004d28c)
#define VF2_TEXTURE_STATUS_SPECIAL_RETURN UINT32_C(0x0004d29c)
#define VF2_TEXTURE_STATUS_COMMON_SOURCE UINT32_C(0x0004d2ac)
#define VF2_TEXTURE_STATUS_COMMON_RETURN UINT32_C(0x0004d2bc)

vf2_status execute_texture_status_tail_dispatch(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report special_report = {0};
    vf2_hybrid_bridge_report common_report = {0};
    const uint64_t start_instructions =
        cpu != NULL ? cpu->executed_instructions : 0u;
    const uint64_t start_calls =
        cpu != NULL ? cpu->procedure_calls : 0u;
    const uint64_t start_returns =
        cpu != NULL ? cpu->procedure_returns : 0u;
    uint8_t selector = 0u;
    uint64_t own_instructions = UINT64_C(8);
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read(
        machine,
        VF2_TEXTURE_STATUS_SELECTOR,
        &selector,
        sizeof(selector)
    );
    if (status != VF2_OK) {
        return status;
    }
    if (selector != UINT8_C(12) && selector != UINT8_C(13)) {
        return execute_texture_status_tail(machine, cpu, report);
    }
    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    cpu->registers[15] = selector;
    if (selector == UINT8_C(13)) {
        own_instructions += UINT64_C(2);
    }

    cpu->registers[VF2_I960_G0_REGISTER + 9u] =
        VF2_TEXTURE_STATUS_SPECIAL_DESTINATION;
    cpu->registers[14] = VF2_TEXTURE_STATUS_SPECIAL_SOURCE;
    cpu->ip = VF2_INLINE_TEXT_THUNK_ENTRY;
    status = execute_inline_text_thunk(machine, cpu, &special_report);
    if (status != VF2_OK || cpu->ip != VF2_TEXTURE_STATUS_SPECIAL_RETURN) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[VF2_I960_G0_REGISTER + 9u] =
        VF2_TEXTURE_STATUS_COMMON_DESTINATION;
    cpu->registers[14] = VF2_TEXTURE_STATUS_COMMON_SOURCE;
    cpu->ip = VF2_INLINE_TEXT_THUNK_ENTRY;
    status = execute_inline_text_thunk(machine, cpu, &common_report);
    if (status != VF2_OK || cpu->ip != VF2_TEXTURE_STATUS_COMMON_RETURN) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = finish_recovered_procedure(machine, cpu, own_instructions);
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_STATUS_TAIL;
    report->entry_address = VF2_TEXTURE_STATUS_TAIL_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(2);
    report->rows = special_report.rows + common_report.rows;
    report->bytes_written =
        special_report.bytes_written + common_report.bytes_written;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls =
        cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns =
        cpu->procedure_returns - start_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}
