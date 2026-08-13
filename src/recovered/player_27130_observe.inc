static vf2_status player_27130_observe_u8(
    const vf2_model2a *machine,
    uint32_t address,
    uint8_t *value
)
{
    return value == NULL
        ? VF2_ERROR_INVALID_ARGUMENT
        : vf2_model2a_read(machine, address, value, sizeof(*value));
}

static vf2_status player_27130_observe_u16(
    const vf2_model2a *machine,
    uint32_t address,
    uint16_t *value
)
{
    uint8_t raw[2] = {0u, 0u};
    vf2_status status = VF2_OK;

    if (value == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read(machine, address, raw, sizeof(raw));
    if (status == VF2_OK) {
        *value = (uint16_t)((uint16_t)raw[0] |
                            ((uint16_t)raw[1] << 8u));
    }
    return status;
}

static vf2_status player_27130_observe(
    const vf2_model2a *machine,
    uint32_t player,
    vf2_player_27130_facts *facts
)
{
    vf2_status status = VF2_OK;

    if (machine == NULL || facts == NULL || player == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(facts, 0, sizeof(*facts));

    status = player_27130_observe_u8(
        machine, UINT32_C(0x00508008), &facts->global_gate
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player, &facts->player_flags
        );
    }
    if (status == VF2_OK) {
        status = player_27130_observe_u8(
            machine, player + UINT32_C(0xbe4), &facts->previous_be4
        );
    }
    if (status == VF2_OK) {
        status = player_27130_observe_u16(
            machine, player + UINT32_C(0x800), &facts->selector_value
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player + UINT32_C(0x1a4), &facts->state
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player + UINT32_C(0x804), &facts->selector_state
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player + UINT32_C(0xc30), &facts->old_selector_state
        );
    }
    if (status == VF2_OK) {
        status = player_27130_observe_u8(
            machine, player + UINT32_C(0x814), &facts->selector_byte
        );
    }
    if (status == VF2_OK) {
        status = player_27130_observe_u8(
            machine, player + UINT32_C(0xa00), &facts->threshold_enabled
        );
    }
    if (status == VF2_OK) {
        status = player_27130_observe_u16(
            machine, player + UINT32_C(0x808), &facts->threshold
        );
    }
    if (status == VF2_OK) {
        status = player_27130_observe_u8(
            machine, player + UINT32_C(0xbe5), &facts->previous_be5
        );
    }
    if (status == VF2_OK) {
        status = player_27130_observe_u8(
            machine, player + UINT32_C(0xbdd), &facts->previous_bdd
        );
    }
    return status;
}
