#include "vf2/lift/runtime.h"

/* See include/vf2/lift/runtime.h and docs/VF2LIFT.md. */

vf2_status vf2_lift_run_block(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t start,
    uint32_t end
)
{
    if (machine == NULL || cpu == NULL || start >= end) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    /* A basic block has a single entry and no internal branch targets, so the
     * only way cpu->ip stays inside [start, end) after a step is linear
     * advance to the next instruction of this block. The terminator transfers
     * control to a successor outside the block (or, for a self-looping block,
     * back to start, which is the intended guest behaviour and continues the
     * loop natively). */
    while (cpu->ip >= start && cpu->ip < end) {
        const vf2_status status = vf2_i960_step_lifted(cpu, machine, NULL);
        if (status != VF2_OK) {
            return status;
        }
    }
    return VF2_OK;
}

const vf2_lift_entry *vf2_lift_table_lookup(
    const vf2_lift_entry *table,
    size_t count,
    uint32_t ip
)
{
    size_t index = 0u;

    if (table == NULL) {
        return NULL;
    }
    for (index = 0u; index < count; ++index) {
        if (ip >= table[index].address && ip < table[index].end) {
            return &table[index];
        }
    }
    return NULL;
}
