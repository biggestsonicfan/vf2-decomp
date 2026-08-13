#include "texture_bridge_internal.h"

vf2_status execute_frame_geometry_gate_legacy(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
);

static void geometry_gate_set_cc(
    vf2_i960_cpu *cpu,
    vf2_i960_compare_result result
)
{
    uint32_t bits = 0u;

    if (result == VF2_I960_COMPARE_LESS) {
        bits = UINT32_C(4);
    } else if (result == VF2_I960_COMPARE_EQUAL) {
        bits = UINT32_C(2);
    } else if (result == VF2_I960_COMPARE_GREATER) {
        bits = UINT32_C(1);
    }
    cpu->compare_result = result;
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | bits;
}

vf2_status execute_frame_geometry_gate(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t flags = 0u;
    uint8_t frame_state = 0u;
    uint8_t alt_byte = 0u;
    vf2_i960_compare_result final_cc = VF2_I960_COMPARE_NONE;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00500704), &flags
    );
    if (status != VF2_OK) {
        return status;
    }
    if ((flags & ((UINT32_C(1) << 26u) | UINT32_C(4))) != 0u) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x0050002a),
            &frame_state, sizeof(frame_state)
        );
        if (status != VF2_OK) {
            return status;
        }
        if (frame_state != UINT8_C(17)) {
            final_cc = UINT8_C(17) < frame_state
                ? VF2_I960_COMPARE_LESS
                : VF2_I960_COMPARE_GREATER;
        } else {
            status = vf2_model2a_read(
                machine, UINT32_C(0x005000a6),
                &alt_byte, sizeof(alt_byte)
            );
            if (status != VF2_OK) {
                return status;
            }
            final_cc = alt_byte != 0u
                ? VF2_I960_COMPARE_LESS
                : VF2_I960_COMPARE_EQUAL;
        }
    }

    status = execute_frame_geometry_gate_legacy(machine, cpu, report);
    if (status == VF2_OK) {
        geometry_gate_set_cc(cpu, final_cc);
    }
    return status;
}
