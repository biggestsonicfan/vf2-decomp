#include "vf2/recovered.h"

void vf2_recovered_boot_entry(
    vf2_recovered_boot_state *state,
    uint32_t system_address_table,
    uint32_t initial_prcb,
    uint32_t initial_entry
)
{
    if (state == 0) {
        return;
    }

    state->system_address_table = system_address_table;
    state->initial_prcb = initial_prcb;
    state->initial_entry = initial_entry;
}
