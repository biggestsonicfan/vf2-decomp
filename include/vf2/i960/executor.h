#ifndef VF2_I960_EXECUTOR_H
#define VF2_I960_EXECUTOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "vf2/i960/instruction.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

enum {
    VF2_I960_REGISTER_COUNT = 32u,
    VF2_I960_G0_REGISTER = 16u,
    VF2_I960_G14_REGISTER = 30u,
    VF2_I960_FP_REGISTER = 31u,
    VF2_I960_LOCAL_REGISTER_COUNT = 16u,
    VF2_I960_MAX_LOCAL_FRAMES = 128u
};

typedef enum vf2_i960_compare_result {
    VF2_I960_COMPARE_NONE = 0,
    VF2_I960_COMPARE_LESS,
    VF2_I960_COMPARE_EQUAL,
    VF2_I960_COMPARE_GREATER,
    VF2_I960_COMPARE_OVERFLOW
} vf2_i960_compare_result;

typedef enum vf2_i960_halt_reason {
    VF2_I960_HALT_NONE = 0,
    VF2_I960_HALT_STOP_ADDRESS,
    VF2_I960_HALT_MAX_STEPS,
    VF2_I960_HALT_SELF_BRANCH,
    VF2_I960_HALT_INVALID_INSTRUCTION,
    VF2_I960_HALT_UNSUPPORTED_INSTRUCTION,
    VF2_I960_HALT_MEMORY_FAULT
} vf2_i960_halt_reason;

typedef struct vf2_i960_local_frame {
    uint32_t registers[VF2_I960_LOCAL_REGISTER_COUNT];
} vf2_i960_local_frame;

typedef struct vf2_i960_cpu {
    uint32_t registers[VF2_I960_REGISTER_COUNT];
    uint32_t sat;
    uint32_t prcb;
    uint32_t ip;
    uint32_t process_control;
    uint32_t arithmetic_control;
    uint32_t interrupt_control;
    vf2_i960_compare_result compare_result;
    uint64_t executed_instructions;
    uint64_t procedure_calls;
    uint64_t procedure_returns;
    uint64_t interrupt_entries;
    uint64_t interrupt_returns;
    vf2_i960_local_frame local_frames[VF2_I960_MAX_LOCAL_FRAMES];
    uint32_t local_frame_depth;
    uint32_t maximum_local_frame_depth;
    bool reinitialized;
} vf2_i960_cpu;

typedef struct vf2_i960_trace_event {
    uint64_t step;
    uint32_t ip_before;
    uint32_t ip_after;
    vf2_i960_instruction instruction;
} vf2_i960_trace_event;

typedef void (*vf2_i960_trace_callback)(
    const vf2_i960_trace_event *event,
    const vf2_i960_cpu *cpu,
    void *user_data
);

typedef struct vf2_i960_run_options {
    uint32_t stop_address;
    uint64_t max_steps;
    bool stop_on_self_branch;
    vf2_i960_trace_callback trace_callback;
    void *trace_user_data;
} vf2_i960_run_options;

typedef struct vf2_i960_run_result {
    vf2_i960_halt_reason halt_reason;
    vf2_status status;
    uint32_t halt_address;
    uint64_t executed_instructions;
} vf2_i960_run_result;

void vf2_i960_cpu_reset(
    vf2_i960_cpu *cpu,
    uint32_t sat,
    uint32_t prcb,
    uint32_t start_ip
);

/* Reset the processor and initialize its architectural frame/stack pointers
 * from the interrupt-stack pointer stored at PRCB + 24. Boot execution should
 * use this form; the plain reset remains useful for isolated unit fixtures. */
vf2_status vf2_i960_cpu_reset_from_machine(
    vf2_i960_cpu *cpu,
    vf2_model2a *machine,
    uint32_t sat,
    uint32_t prcb,
    uint32_t start_ip
);

/* Enter a procedure through the architectural call mechanism. This is used
 * by isolated differential tests that start at a recovered function entry. */
vf2_status vf2_i960_cpu_enter_procedure(
    vf2_i960_cpu *cpu,
    uint32_t target,
    uint32_t return_address
);

/* Complete the current architectural procedure frame without decoding a ROM
 * instruction. Recovered-C task bodies use this to model their final RET.
 * The caller remains responsible for accounting for the recovered RET in the
 * executed-instruction counter. */
vf2_status vf2_i960_cpu_return_procedure(
    vf2_i960_cpu *cpu,
    vf2_model2a *machine
);

/* Enter an external interrupt through the PRCB interrupt table. */
vf2_status vf2_i960_cpu_enter_interrupt(
    vf2_i960_cpu *cpu,
    vf2_model2a *machine,
    uint32_t vector,
    uint32_t level
);

vf2_status vf2_i960_step(
    vf2_i960_cpu *cpu,
    vf2_model2a *machine,
    vf2_i960_trace_event *event
);

vf2_status vf2_i960_run(
    vf2_i960_cpu *cpu,
    vf2_model2a *machine,
    const vf2_i960_run_options *options,
    vf2_i960_run_result *result
);

const char *vf2_i960_halt_reason_name(vf2_i960_halt_reason reason);

#endif
