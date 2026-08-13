static vf2_status player_selector_scratch_locate(
    const vf2_model2a *machine,
    uint32_t player,
    vf2_player_selector_scratch_plan *plan,
    uint32_t *count_pointer_start
)
{
    uint32_t raw_pointer = 0u;
    uint32_t count_pointer_end = 0u;
    uint32_t count_sum = 0u;
    uint32_t alignment = 0u;
    uint16_t selector16 = 0u;
    size_t count_records = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || plan == NULL || count_pointer_start == NULL ||
        player == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(plan, 0, sizeof(*plan));
    plan->player = player;

    status = player_selector_scratch_read_u16(
        machine, player + UINT32_C(0x1a8), &selector16
    );
    if (status != VF2_OK) {
        return status;
    }
    plan->selector = selector16;
    if (plan->selector == 0u) {
        *count_pointer_start = 0u;
        return VF2_OK;
    }

    status = vf2_model2a_read_u32(
        machine,
        VF2_PLAYER_SELECTOR_SCRATCH_TABLE +
            plan->selector * UINT32_C(4),
        &plan->table_pointer
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player + UINT32_C(0xbd8), &plan->scratch_base
        );
    }
    if (status != VF2_OK || plan->table_pointer == 0u ||
        plan->scratch_base == 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = player_selector_scratch_add(
        plan->table_pointer, UINT32_C(2), &raw_pointer
    );
    if (status == VF2_OK) {
        status = player_selector_scratch_add(
            raw_pointer,
            (uint32_t)VF2_PLAYER_SELECTOR_SCRATCH_RAW_COUNT,
            count_pointer_start
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    plan->code_pointer_end = *count_pointer_start;

    status = player_selector_scratch_expand_codes(
        machine,
        plan,
        raw_pointer,
        *count_pointer_start,
        &count_pointer_end,
        &count_sum,
        &count_records
    );
    if (status != VF2_OK) {
        return status;
    }

    alignment = (uint32_t)((count_records + 22u) & 3u);
    alignment = (UINT32_C(4) - alignment) & UINT32_C(3);
    status = player_selector_scratch_add(
        count_pointer_end, alignment, &plan->aligned_count_pointer
    );
    if (status != VF2_OK) {
        return status;
    }
    plan->payload_start = plan->aligned_count_pointer;
    return player_selector_scratch_advance(
        &plan->payload_start, count_sum, UINT32_C(4)
    );
}
