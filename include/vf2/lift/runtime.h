#ifndef VF2_LIFT_RUNTIME_H
#define VF2_LIFT_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

/* Runtime support for the build-time i960->C recompiler (tools/vf2lift).
 *
 * Generated code is written into the build directory only and is never
 * committed (docs/VF2LIFT.md). Each lifted guest function becomes one C
 * function with this signature; its body is a native switch over its basic
 * blocks (compiled control flow) and each block runs through the shared
 * interpreter semantics via vf2_lift_run_block, so there is no second
 * implementation of the opcode semantics. */

typedef vf2_status (*vf2_lift_fn)(vf2_model2a *machine, vf2_i960_cpu *cpu);

typedef struct vf2_lift_entry {
    uint32_t address;   /* guest function entry (inclusive)                  */
    uint32_t end;       /* guest function end (exclusive); range it owns     */
    vf2_lift_fn fn;     /* the generated C body                              */
    const char *name;   /* recovered/analysis name, for status and logging   */
} vf2_lift_entry;

/* Execute the guest basic block [start, end) at cpu->ip, one instruction at a
 * time through vf2_i960_step_lifted (identical semantics to the interpreter,
 * accounted as lifted). Returns as soon as control leaves the block -- i.e.
 * cpu->ip is no longer inside [start, end) -- leaving cpu->ip at the successor
 * the terminator selected (fall-through block, branch target, callee entry or
 * caller return). A fault from the shared step is returned verbatim and
 * cpu->ip is left where the interpreter left it (fail closed). */
vf2_status vf2_lift_run_block(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t start,
    uint32_t end
);

/* Find the lifted function that owns a guest address (address <= ip < end),
 * searching a caller-supplied table. Returns NULL when none matches. The
 * table need not be sorted. */
const vf2_lift_entry *vf2_lift_table_lookup(
    const vf2_lift_entry *table,
    size_t count,
    uint32_t ip
);

#endif
