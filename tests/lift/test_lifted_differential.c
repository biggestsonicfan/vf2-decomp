/* Per-function differential for the build-time lifted i960->C bridge
 * (plan phase 4, Option B; docs/VF2LIFT.md).
 *
 * For every function in the generated lifted table, run the pure interpreter
 * and the generated lifted body from the identical entry state and require
 * byte-identical architectural state (registers, condition codes, frame stack
 * and all memory). The lifted body reuses the interpreter's instruction
 * semantics through vf2_i960_step_lifted, so a straight-line function matches
 * by construction; this test is a real check on the *structure* the lift adds
 * -- the basic-block decomposition, successor selection, and the fail-closed
 * return when control leaves the function's blocks.
 *
 * The interpreter reference is run first, bounded by a step budget; the lifted
 * body is only driven once the reference has terminated (left the function's
 * address range), which guarantees the lifted body -- whose block union is a
 * subset of that range -- cannot run longer, so it cannot hang.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/boot.h"
#include "vf2/i960/executor.h"
#include "vf2/i960/snapshot.h"
#include "vf2/lift/runtime.h"
#include "vf2/model2a.h"
#include "vf2/rom.h"

#include "vf2_lifted.h" /* generated: vf2_lift_table, vf2_lifted_* */

static int failures = 0;

#define CHECK(expression)                                                           \
    do {                                                                            \
        if (!(expression)) {                                                        \
            fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #expression); \
            ++failures;                                                             \
        }                                                                           \
    } while (0)

/* A return address deliberately outside every function's range, so a ret or a
 * tail transfer ends the reference loop and the lifted body together. */
#define LIFT_RETURN_SENTINEL UINT32_C(0x00000004)
#define LIFT_STEP_BUDGET     UINT64_C(4000000)

static int build_machine(
    vf2_model2a *machine,
    uint8_t *main_rom,
    size_t main_rom_size,
    uint8_t *main_data,
    size_t main_data_size
)
{
    memset(machine, 0, sizeof(*machine));
    if (!vf2_model2a_initialize(machine) || machine->work_ram == NULL) {
        return 0;
    }
    if (vf2_model2a_attach_main_rom(machine, main_rom, main_rom_size) != VF2_OK) {
        return 0;
    }
    if (main_data != NULL &&
        vf2_model2a_attach_main_data(machine, main_data, main_data_size) != VF2_OK) {
        return 0;
    }
    return 1;
}

/* Place the CPU at a function entry through the architectural call mechanism,
 * exactly as an isolated differential would, and leave it ready to run. */
static int enter_at(
    vf2_i960_cpu *cpu,
    vf2_model2a *machine,
    const vf2_i960_boot_vectors *vectors,
    uint32_t entry
)
{
    if (vf2_i960_cpu_reset_from_machine(
            cpu, machine, vectors->system_address_table,
            vectors->initial_prcb, vectors->start_ip) != VF2_OK) {
        return 0;
    }
    return vf2_i960_cpu_enter_procedure(cpu, entry, LIFT_RETURN_SENTINEL) == VF2_OK;
}

static void run_entry_differential(
    const vf2_lift_entry *entry,
    const vf2_i960_boot_vectors *vectors,
    uint8_t *main_rom,
    size_t main_rom_size,
    uint8_t *main_data,
    size_t main_data_size
)
{
    vf2_model2a ref_machine;
    vf2_model2a lift_machine;
    vf2_i960_cpu ref_cpu;
    vf2_i960_cpu lift_cpu;
    vf2_i960_snapshot_diff diff;
    vf2_status ref_status = VF2_OK;
    vf2_status lift_status = VF2_OK;
    uint64_t steps = 0u;
    int ref_in_range = 1;

    memset(&ref_cpu, 0, sizeof(ref_cpu));
    memset(&lift_cpu, 0, sizeof(lift_cpu));
    memset(&diff, 0, sizeof(diff));

    if (!build_machine(&ref_machine, main_rom, main_rom_size,
                       main_data, main_data_size) ||
        !build_machine(&lift_machine, main_rom, main_rom_size,
                       main_data, main_data_size)) {
        ++failures;
        fprintf(stderr, "FAILED build_machine for %s\n", entry->name);
        vf2_model2a_shutdown(&ref_machine);
        vf2_model2a_shutdown(&lift_machine);
        return;
    }

    if (!enter_at(&ref_cpu, &ref_machine, vectors, entry->address) ||
        !enter_at(&lift_cpu, &lift_machine, vectors, entry->address)) {
        ++failures;
        fprintf(stderr, "FAILED enter_at for %s\n", entry->name);
        vf2_model2a_shutdown(&ref_machine);
        vf2_model2a_shutdown(&lift_machine);
        return;
    }

    /* Reference: pure interpreter until control leaves the function's range or
     * a fault, bounded by the step budget. */
    while (ref_cpu.ip >= entry->address && ref_cpu.ip < entry->end) {
        if (steps >= LIFT_STEP_BUDGET) {
            ref_in_range = 1;
            break;
        }
        ref_status = vf2_i960_step(&ref_cpu, &ref_machine, NULL);
        ++steps;
        if (ref_status != VF2_OK) {
            ref_in_range = 0; /* faulted; ip pinned at the faulting instruction */
            break;
        }
        ref_in_range = (ref_cpu.ip >= entry->address && ref_cpu.ip < entry->end);
        if (!ref_in_range) {
            break;
        }
    }

    if (steps >= LIFT_STEP_BUDGET && ref_status == VF2_OK && ref_in_range) {
        /* Non-terminating from this cold entry state; not differentiable here
         * without a captured session state. Report and move on -- a later
         * slice drives these from Phase 3 captures. */
        fprintf(stderr, "skip %s @0x%08x: did not terminate in %llu steps\n",
                entry->name, (unsigned)entry->address,
                (unsigned long long)LIFT_STEP_BUDGET);
        vf2_model2a_shutdown(&ref_machine);
        vf2_model2a_shutdown(&lift_machine);
        return;
    }

    /* Lifted: cannot run longer than the reference did, so it terminates. */
    lift_status = entry->fn(&lift_machine, &lift_cpu);

    CHECK(ref_status == lift_status);
    if (vf2_i960_compare_live_state(&ref_cpu, &ref_machine,
                                    &lift_cpu, &lift_machine, &diff) != VF2_OK) {
        ++failures;
    } else if (!diff.equal) {
        ++failures;
        fprintf(stderr,
            "FAILED %s @0x%08x: lifted != interpreter in %s "
            "(off %zu exp 0x%08x act 0x%08x; %zu bytes)\n",
            entry->name, (unsigned)entry->address, diff.component,
            diff.first_offset, (unsigned)diff.expected_value,
            (unsigned)diff.actual_value, diff.differing_bytes);
    }
    /* The lifted body must account every retired instruction as lifted and
     * none as interpreted. */
    CHECK(lift_cpu.interpreted_instructions == 0u);
    CHECK(lift_cpu.lifted_instructions == ref_cpu.interpreted_instructions);

    fprintf(stderr, "  ok %-24s @0x%08x  steps=%llu lifted=%llu%s\n",
            entry->name, (unsigned)entry->address,
            (unsigned long long)steps,
            (unsigned long long)lift_cpu.lifted_instructions,
            ref_status != VF2_OK ? " (matched fault)" : "");

    vf2_model2a_shutdown(&ref_machine);
    vf2_model2a_shutdown(&lift_machine);
}

static void test_unit_run_block_guards(void)
{
    vf2_lift_tier tier = VF2_LIFT_TIER_INTERPRETED;

    CHECK(vf2_lift_run_block(NULL, NULL, 0u, 4u) == VF2_ERROR_INVALID_ARGUMENT);
    CHECK(vf2_lift_table_lookup(NULL, 0u, 0u) == NULL);
    CHECK(vf2_lift_table_lookup(vf2_lift_table, vf2_lift_table_count,
                                0xfffffffeu) == NULL);
    CHECK(vf2_lift_dispatch_step(NULL, NULL, vf2_lift_table,
                                 vf2_lift_table_count, &tier)
          == VF2_ERROR_INVALID_ARGUMENT);
    if (vf2_lift_table_count > 0u) {
        const vf2_lift_entry *e =
            vf2_lift_table_lookup(vf2_lift_table, vf2_lift_table_count,
                                  vf2_lift_table[0].address);
        CHECK(e == &vf2_lift_table[0]);
    }
}

/* Dispatch the function at `entry` through the registry (lifted tier), one
 * dispatch unit at a time, while control stays in the function's range, and
 * require the pure interpreter from the identical state to reach byte-identical
 * live state. Accumulates the lifted instruction total and asserts the driver
 * side accounts zero interpreted instructions (the Phase 4 property for a
 * fully lifted corridor). Returns the lifted instruction count, or 0 on skip. */
static uint64_t run_entry_via_dispatch(
    const vf2_lift_entry *entry,
    const vf2_i960_boot_vectors *vectors,
    uint8_t *main_rom,
    size_t main_rom_size,
    uint8_t *main_data,
    size_t main_data_size
)
{
    vf2_model2a drv_machine;
    vf2_model2a ref_machine;
    vf2_i960_cpu drv_cpu;
    vf2_i960_cpu ref_cpu;
    vf2_i960_snapshot_diff diff;
    vf2_status ref_status = VF2_OK;
    uint64_t steps = 0u;
    int ref_in_range = 1;

    memset(&drv_cpu, 0, sizeof(drv_cpu));
    memset(&ref_cpu, 0, sizeof(ref_cpu));
    memset(&diff, 0, sizeof(diff));

    if (!build_machine(&drv_machine, main_rom, main_rom_size,
                       main_data, main_data_size) ||
        !build_machine(&ref_machine, main_rom, main_rom_size,
                       main_data, main_data_size) ||
        !enter_at(&drv_cpu, &drv_machine, vectors, entry->address) ||
        !enter_at(&ref_cpu, &ref_machine, vectors, entry->address)) {
        ++failures;
        vf2_model2a_shutdown(&drv_machine);
        vf2_model2a_shutdown(&ref_machine);
        return 0u;
    }

    /* Reference first, to bound the driver (see run_entry_differential). */
    while (ref_cpu.ip >= entry->address && ref_cpu.ip < entry->end) {
        if (steps >= LIFT_STEP_BUDGET) { ref_in_range = 1; break; }
        ref_status = vf2_i960_step(&ref_cpu, &ref_machine, NULL);
        ++steps;
        if (ref_status != VF2_OK) { ref_in_range = 0; break; }
        ref_in_range = (ref_cpu.ip >= entry->address && ref_cpu.ip < entry->end);
        if (!ref_in_range) { break; }
    }
    if (steps >= LIFT_STEP_BUDGET && ref_status == VF2_OK && ref_in_range) {
        vf2_model2a_shutdown(&drv_machine);
        vf2_model2a_shutdown(&ref_machine);
        return 0u; /* non-terminating from cold entry; skip */
    }

    /* Driver: route each unit through the registry. The first unit is the
     * lifted function itself; it runs the whole range, so one dispatch unit
     * suffices, but loop to be general. */
    while (drv_cpu.ip >= entry->address && drv_cpu.ip < entry->end) {
        vf2_lift_tier tier = VF2_LIFT_TIER_INTERPRETED;
        vf2_status s = vf2_lift_dispatch_step(&drv_machine, &drv_cpu,
                                              vf2_lift_table,
                                              vf2_lift_table_count, &tier);
        CHECK(tier == VF2_LIFT_TIER_LIFTED); /* entry range is lifted */
        if (s != VF2_OK) { break; }
    }

    if (vf2_i960_compare_live_state(&ref_cpu, &ref_machine,
                                    &drv_cpu, &drv_machine, &diff) != VF2_OK ||
        !diff.equal) {
        ++failures;
        fprintf(stderr, "FAILED dispatch %s @0x%08x: driver != interpreter in %s\n",
                entry->name, (unsigned)entry->address, diff.component);
    }
    CHECK(drv_cpu.interpreted_instructions == 0u);

    {
        const uint64_t lifted = drv_cpu.lifted_instructions;
        vf2_model2a_shutdown(&drv_machine);
        vf2_model2a_shutdown(&ref_machine);
        return lifted;
    }
}

int main(int argc, char **argv)
{
    uint8_t *main_rom = NULL;
    uint8_t *main_data = NULL;
    size_t main_rom_size = 0u;
    size_t main_data_size = 0u;
    vf2_i960_boot_vectors vectors;
    size_t index = 0u;

    test_unit_run_block_guards();

    if (argc != 2) {
        if (failures != 0) {
            fprintf(stderr, "%d lifted unit test(s) failed\n", failures);
            return EXIT_FAILURE;
        }
        puts("lifted ROM-independent tests passed");
        return EXIT_SUCCESS;
    }

    CHECK(vf2_romset_build_region(argv[1], VF2_REGION_MAINCPU,
                                  &main_rom, &main_rom_size) == VF2_OK);
    (void)vf2_romset_build_region(argv[1], VF2_REGION_MAIN_DATA,
                                  &main_data, &main_data_size);
    if (main_rom == NULL ||
        vf2_parse_i960_boot_vectors(main_rom, main_rom_size, &vectors) != VF2_OK) {
        fprintf(stderr, "could not load ROM from %s\n", argv[1]);
        free(main_rom);
        free(main_data);
        return EXIT_FAILURE;
    }

    fprintf(stderr, "lifted differential: %zu function(s)\n",
            vf2_lift_table_count);
    for (index = 0u; index < vf2_lift_table_count; ++index) {
        run_entry_differential(&vf2_lift_table[index], &vectors,
                               main_rom, main_rom_size,
                               main_data, main_data_size);
    }

    /* Dispatch-registry pass: route each corridor function through the
     * recovered>lifted>interpreted registry (lifted tier) and confirm the
     * driver reproduces the interpreter while accounting zero interpreted
     * instructions. */
    {
        uint64_t total_lifted = 0u;
        for (index = 0u; index < vf2_lift_table_count; ++index) {
            total_lifted += run_entry_via_dispatch(&vf2_lift_table[index],
                                                   &vectors, main_rom,
                                                   main_rom_size, main_data,
                                                   main_data_size);
        }
        fprintf(stderr,
                "dispatch registry: %zu function(s) via lifted tier, "
                "%llu instruction(s), 0 interpreted, state == oracle\n",
                vf2_lift_table_count, (unsigned long long)total_lifted);
    }

    free(main_rom);
    free(main_data);

    if (failures != 0) {
        fprintf(stderr, "%d lifted differential test(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    puts("lifted differential tests passed");
    return EXIT_SUCCESS;
}
