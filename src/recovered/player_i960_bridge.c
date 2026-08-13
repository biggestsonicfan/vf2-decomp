#include "vf2/i960/executor.h"

/*
 * Base fallback for the recovered player bridge chain.
 *
 * Earlier revisions kept three snapshot-specific native shortcuts here for
 * 0x28178, 0x17710 and 0x1791c. Those corridors now have semantic recovery
 * layers in player_i960_bridge_tail.c, including dynamic stream parsing,
 * branch-generic control flow and generalized motion integration.
 *
 * Keeping the old literal shortcuts beneath those layers could hide a
 * regression by silently accepting the original observed snapshot. The base
 * layer is therefore intentionally just the architectural executor: a state
 * not owned by a semantic recovery remains a real i960 fallback.
 */
vf2_status vf2_hybrid_i960_run(
    vf2_i960_cpu *cpu,
    vf2_model2a *machine,
    const vf2_i960_run_options *options,
    vf2_i960_run_result *result
)
{
    return vf2_i960_run(cpu, machine, options, result);
}
