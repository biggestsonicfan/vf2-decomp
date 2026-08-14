#include "vf2/hybrid.h"

#if defined(__GNUC__) || defined(__clang__)
#define VF2_TEXTURE_ORCHESTRATOR_ENTRY UINT32_C(0x0004bd00)
#define VF2_TEXTURE_STATUS_DISPATCH_ENTRY UINT32_C(0x0004bd24)
#define VF2_TEXTURE_RECORD_STATUS_ENTRY UINT32_C(0x0004bd5c)
#define VF2_TEXTURE_ACTIVE_PREPARE_ENTRY UINT32_C(0x0004bde0)
#define VF2_TEXTURE_RECORD_STATUS_EXIT UINT32_C(0x0004bde0)
#define VF2_TEXTURE_STATUS_LINE_ENTRY UINT32_C(0x0004d2c0)
#define VF2_TEXTURE_ACTIVE_PREPARE_TARGET UINT32_C(0x0004d16c)
#define VF2_TEXTURE_TREE_DISPATCH_ENTRY UINT32_C(0x0004c544)
#define VF2_TEXTURE_TREE_DISPATCH_EXIT UINT32_C(0x0004c6e0)
#define VF2_TEXTURE_WORD_PREPARE_ENTRY UINT32_C(0x0004cb64)
#define VF2_TEXTURE_WORD_PREPARE_EXIT UINT32_C(0x0004cc28)
#define VF2_TEXTURE_COLOR_PREPARE_ENTRY UINT32_C(0x0004cd18)
#define VF2_TEXTURE_COLOR_PREPARE_EXIT UINT32_C(0x0004cdb0)
#define VF2_TEXTURE_CONVERT_LOOP_ENTRY UINT32_C(0x0004cdb0)
#define VF2_TEXTURE_CONVERT_ENTRY UINT32_C(0x0004ce88)
#define VF2_TEXTURE_CONVERT_LOOP_RETURN UINT32_C(0x0004ce0c)
#define VF2_TEXTURE_ACTIVE_FLAGS UINT32_C(0x0055c2f4)

vf2_status vf2_hybrid_post_frame_bridge_execute_impl(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
);

static void set_compare_result(
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
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | bits;
    cpu->compare_result = result;
}

static vf2_status set_active_flag_bit_condition(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t bit
)
{
    uint32_t flags = 0u;
    const vf2_status status = vf2_model2a_read_u32(
        machine, VF2_TEXTURE_ACTIVE_FLAGS, &flags
    );

    if (status != VF2_OK) {
        return status;
    }
    set_compare_result(
        cpu,
        (flags & (UINT32_C(1) << bit)) != 0u
            ? VF2_I960_COMPARE_EQUAL
            : VF2_I960_COMPARE_NONE
    );
    return VF2_OK;
}

vf2_status vf2_hybrid_post_frame_bridge_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint32_t entry = cpu != NULL ? cpu->ip : 0u;
    const uint32_t entry_r3 = cpu != NULL ? cpu->registers[3] : 0u;
    const uint32_t entry_r7 = cpu != NULL ? cpu->registers[7] : 0u;
    const vf2_status status = vf2_hybrid_post_frame_bridge_execute_impl(
        machine, cpu, report
    );

    if (status != VF2_OK || cpu == NULL) {
        return status;
    }

    if (entry == VF2_TEXTURE_ORCHESTRATOR_ENTRY) {
        set_compare_result(cpu, VF2_I960_COMPARE_EQUAL);
    } else if (entry == VF2_TEXTURE_STATUS_DISPATCH_ENTRY) {
        if (cpu->ip == VF2_TEXTURE_STATUS_LINE_ENTRY) {
            set_compare_result(cpu, VF2_I960_COMPARE_NONE);
        } else if (cpu->ip == VF2_TEXTURE_RECORD_STATUS_ENTRY) {
            set_compare_result(cpu, VF2_I960_COMPARE_EQUAL);
        }
    } else if (entry == VF2_TEXTURE_RECORD_STATUS_ENTRY) {
        if (cpu->ip == VF2_TEXTURE_STATUS_DISPATCH_ENTRY) {
            set_compare_result(cpu, VF2_I960_COMPARE_EQUAL);
        } else if (cpu->ip == VF2_TEXTURE_RECORD_STATUS_EXIT) {
            if ((int32_t)entry_r3 >= 0) {
                set_compare_result(
                    cpu,
                    entry_r3 == 0u
                        ? VF2_I960_COMPARE_EQUAL
                        : VF2_I960_COMPARE_LESS
                );
            } else {
                set_compare_result(
                    cpu,
                    cpu->registers[8] == 0u
                        ? VF2_I960_COMPARE_EQUAL
                        : VF2_I960_COMPARE_LESS
                );
            }
        }
    } else if (entry == VF2_TEXTURE_ACTIVE_PREPARE_ENTRY &&
               cpu->ip == VF2_TEXTURE_ACTIVE_PREPARE_TARGET) {
        const uint32_t bits45 = (UINT32_C(1) << 4u) | (UINT32_C(1) << 5u);

        set_compare_result(
            cpu,
            (entry_r7 & bits45) == bits45
                ? VF2_I960_COMPARE_EQUAL
                : VF2_I960_COMPARE_NONE
        );
    } else if (entry == VF2_TEXTURE_TREE_DISPATCH_ENTRY &&
               cpu->ip == VF2_TEXTURE_TREE_DISPATCH_EXIT) {
        const vf2_status cc_status = set_active_flag_bit_condition(
            machine, cpu, UINT32_C(1)
        );
        if (cc_status != VF2_OK) {
            return cc_status;
        }
    } else if (entry == VF2_TEXTURE_WORD_PREPARE_ENTRY &&
               cpu->ip == VF2_TEXTURE_WORD_PREPARE_EXIT) {
        const vf2_status cc_status = set_active_flag_bit_condition(
            machine, cpu, UINT32_C(2)
        );
        if (cc_status != VF2_OK) {
            return cc_status;
        }
    } else if (entry == VF2_TEXTURE_COLOR_PREPARE_ENTRY &&
               cpu->ip == VF2_TEXTURE_COLOR_PREPARE_EXIT) {
        const vf2_status cc_status = set_active_flag_bit_condition(
            machine, cpu, UINT32_C(1)
        );
        if (cc_status != VF2_OK) {
            return cc_status;
        }
    } else if (entry == VF2_TEXTURE_CONVERT_LOOP_ENTRY) {
        if (cpu->ip == VF2_TEXTURE_CONVERT_ENTRY) {
            set_compare_result(cpu, VF2_I960_COMPARE_LESS);
        } else if (cpu->ip == VF2_TEXTURE_CONVERT_LOOP_RETURN) {
            set_compare_result(cpu, VF2_I960_COMPARE_EQUAL);
        }
    }
    return VF2_OK;
}
#endif
