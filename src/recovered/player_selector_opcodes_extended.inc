/* Additional semantic handlers for the 0x1a1e4 selector bytecode. */

static int player_selector_extended_opcode_supported(uint8_t opcode)
{
    return opcode == UINT8_C(2) ||
           opcode == UINT8_C(4) ||
           opcode == UINT8_C(11);
}

static vf2_status player_selector_extended_opcode_size(
    uint8_t opcode,
    uint32_t *size
)
{
    if (size == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    switch (opcode) {
    case 2u:
        *size = UINT32_C(7);
        return VF2_OK;
    case 4u:
        *size = UINT32_C(17);
        return VF2_OK;
    case 11u:
        *size = UINT32_C(11);
        return VF2_OK;
    default:
        return VF2_ERROR_UNSUPPORTED;
    }
}

static int player_selector_opcode2_first_override(uint32_t selector)
{
    static const uint16_t selectors[] = {
        UINT16_C(0x0046), UINT16_C(0x0334),
        UINT16_C(0x0332), UINT16_C(0x0335)
    };
    size_t index = 0u;

    for (index = 0u; index < sizeof(selectors) / sizeof(selectors[0]); ++index) {
        if (selector == (uint32_t)selectors[index]) {
            return 1;
        }
    }
    return 0;
}

static vf2_status player_selector_apply_opcode2(
    vf2_model2a *machine,
    uint32_t player,
    uint32_t selector,
    uint32_t cursor
)
{
    uint32_t branch_base = 0u;
    uint32_t state = 0u;
    uint16_t first = 0u;
    uint16_t second = 0u;
    uint8_t branch_byte = 0u;
    uint8_t value = 0u;
    vf2_status status = VF2_OK;

    status = player_selector_read_u8(machine, cursor + UINT32_C(1), &value);
    if (status == VF2_OK) {
        status = player_selector_write_u8(
            machine, player + UINT32_C(0x0808), value
        );
    }
    if (status == VF2_OK) {
        status = player_selector_read_u8(machine, cursor + UINT32_C(2), &value);
    }
    if (status == VF2_OK) {
        status = player_selector_write_u8(
            machine, player + UINT32_C(0x0809), value
        );
    }
    if (status == VF2_OK) {
        status = player_selector_read_u16(machine, cursor + UINT32_C(3), &first);
    }
    if (status == VF2_OK) {
        status = player_selector_read_u16(machine, cursor + UINT32_C(5), &second);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_PLAYER_SELECTOR_BRANCH_BASE, &branch_base
        );
    }
    if (status == VF2_OK && branch_base != 0u) {
        status = player_selector_read_u8(
            machine, branch_base + UINT32_C(0x3351), &branch_byte
        );
    }
    if (status != VF2_OK || branch_base == 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    if ((branch_byte & (UINT8_C(1) << 6u)) != 0u) {
        if (selector == UINT32_C(0x012d)) {
            first = UINT16_C(27);
        }
        if (player_selector_opcode2_first_override(selector)) {
            first = UINT16_C(7);
        }
    }
    status = player_selector_write_u16(
        machine, player + UINT32_C(0x080a), first
    );
    if (status != VF2_OK) {
        return status;
    }

    if (selector == UINT32_C(0x012e)) {
        second = UINT16_C(0x0049);
    } else if (selector == UINT32_C(0x0375)) {
        second = UINT16_C(46);
    }
    if ((branch_byte & (UINT8_C(1) << 6u)) != 0u) {
        if (selector == UINT32_C(0x02b9) || selector == UINT32_C(0x02ba)) {
            second = UINT16_C(43);
        } else if (selector == UINT32_C(0x02a1)) {
            second = UINT16_C(55);
        }
        if (selector == UINT32_C(0x021a) || selector == UINT32_C(0x021b)) {
            second = UINT16_C(48);
        } else if (selector == UINT32_C(0x0220)) {
            second = UINT16_C(44);
        } else if (selector == UINT32_C(0x045f)) {
            second = UINT16_C(42);
            status = player_selector_write_u16(
                machine, player + UINT32_C(0x080a), second
            );
            if (status != VF2_OK) {
                return status;
            }
        }
    }
    status = player_selector_write_u16(
        machine, player + UINT32_C(0x080c), second
    );
    if (status == VF2_OK) {
        status = player_selector_write_u8(
            machine, player + UINT32_C(0x0a00), UINT8_C(1)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player + UINT32_C(0x01a4), &state
        );
    }
    if (status == VF2_OK) {
        state |= UINT32_C(1) << 1u;
        status = vf2_model2a_write_u32(
            machine, player + UINT32_C(0x01a4), state
        );
    }
    return status;
}

static vf2_status player_selector_apply_opcode4(
    vf2_model2a *machine,
    uint32_t player,
    uint32_t selector,
    uint32_t cursor
)
{
    uint32_t branch_base = 0u;
    uint32_t player_flags = 0u;
    uint32_t counter = 0u;
    uint16_t packed = 0u;
    int32_t scale = 0;
    int32_t base = 0;
    int32_t output = 0;
    uint8_t branch_byte = 0u;
    uint8_t player_type = 0u;
    uint8_t source_scale = 0u;
    uint8_t value = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;
    static const uint16_t direct_offsets[8] = {
        UINT16_C(0x0820), UINT16_C(0x0821),
        UINT16_C(0x0824), UINT16_C(0x0825),
        UINT16_C(0x0826), UINT16_C(0x0827),
        UINT16_C(0x082a), UINT16_C(0x082b)
    };
    static const uint8_t direct_source[8] = {
        1u, 2u, 4u, 5u, 6u, 7u, 8u, 9u
    };
    static const uint16_t tail_offsets[5] = {
        UINT16_C(0x0823), UINT16_C(0x0843),
        UINT16_C(0x085c), UINT16_C(0x085d), UINT16_C(0x085e)
    };

    for (index = 0u; index < 8u; ++index) {
        status = player_selector_read_u8(
            machine, cursor + (uint32_t)direct_source[index], &value
        );
        if (status == VF2_OK) {
            status = player_selector_write_u8(
                machine, player + (uint32_t)direct_offsets[index], value
            );
        }
        if (status != VF2_OK) {
            return status;
        }
    }
    if (selector == UINT32_C(0x00bd)) {
        status = player_selector_write_u16(
            machine, player + UINT32_C(0x082a), UINT16_C(0xbff4)
        );
        if (status != VF2_OK) {
            return status;
        }
    }

    status = player_selector_read_u16(machine, cursor + UINT32_C(10), &packed);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_PLAYER_SELECTOR_BRANCH_BASE, &branch_base
        );
    }
    if (status == VF2_OK && branch_base != 0u) {
        status = player_selector_read_u8(
            machine, branch_base + UINT32_C(0x3351), &branch_byte
        );
    }
    if (status != VF2_OK || branch_base == 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    if ((branch_byte & (UINT8_C(1) << 6u)) != 0u &&
        selector == UINT32_C(0x0320)) {
        packed |= UINT16_C(1) << 14u;
    }
    status = player_selector_write_u16(
        machine, player + UINT32_C(0x0828), packed
    );
    if (status != VF2_OK) {
        return status;
    }

    for (index = 0u; index < 5u; ++index) {
        status = player_selector_read_u8(
            machine, cursor + UINT32_C(12) + (uint32_t)index, &value
        );
        if (status == VF2_OK) {
            status = player_selector_write_u8(
                machine, player + (uint32_t)tail_offsets[index], value
            );
        }
        if (status != VF2_OK) {
            return status;
        }
    }

    status = player_selector_read_u8(machine, cursor + UINT32_C(3), &source_scale);
    if (status != VF2_OK) {
        return status;
    }
    scale = (int32_t)(int8_t)source_scale;
    if (selector == UINT32_C(0x012e)) {
        scale = 40;
    }

    if ((branch_byte & (UINT8_C(1) << 6u)) != 0u) {
        if (selector == UINT32_C(0x0162)) {
            scale = 23;
        }
        status = player_selector_write_u8(
            machine, player + UINT32_C(0x085f), 0u
        );
        if (status == VF2_OK) {
            status = player_selector_read_u8(
                machine, player + UINT32_C(0x01b1), &player_type
            );
        }
        if (status != VF2_OK) {
            return status;
        }

        if (selector == UINT32_C(0x02f1) || selector == UINT32_C(0x02f0)) {
            packed &= UINT16_C(0xbfff);
            scale = 29;
        } else if (selector == UINT32_C(0x028d) ||
                   selector == UINT32_C(0x0331)) {
            packed &= UINT16_C(0xbfff);
            status = player_selector_write_u8(
                machine, player + UINT32_C(0x085f), UINT8_C(1)
            );
            scale = 29;
        } else if (player_type != UINT8_C(1)) {
            if (selector != UINT32_C(0x02b0)) {
                packed &= UINT16_C(0xbfff);
                status = player_selector_write_u8(
                    machine, player + UINT32_C(0x085f), UINT8_C(1)
                );
                scale = 29;
            }
        } else if (selector == UINT32_C(0x02b4)) {
            packed &= UINT16_C(0xbfff);
            status = player_selector_write_u8(
                machine, player + UINT32_C(0x085f), UINT8_C(1)
            );
            scale = 29;
        }
        if (status != VF2_OK) {
            return status;
        }
        status = player_selector_write_u16(
            machine, player + UINT32_C(0x0828), packed
        );
        if (status != VF2_OK) {
            return status;
        }
    }

    status = player_selector_read_u8(
        machine, player + UINT32_C(0x069c), &value
    );
    if (status != VF2_OK) {
        return status;
    }
    base = (int32_t)(int8_t)value;
    output = ((base * 5 + 100) * scale) / 100;
    status = player_selector_write_u8(
        machine, player + UINT32_C(0x0822), (uint8_t)output
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player + UINT32_C(0x1230), &counter
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, player + UINT32_C(0x1230), counter + UINT32_C(1)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, player, &player_flags);
    }
    if (status == VF2_OK && (player_flags & (UINT32_C(1) << 6u)) != 0u) {
        status = player_selector_read_u8(
            machine, player + UINT32_C(0x0820), &value
        );
        if (status == VF2_OK) {
            status = player_selector_read_u8(
                machine, UINT32_C(0x02007c78) + (uint32_t)value, &value
            );
        }
        if (status == VF2_OK) {
            status = player_selector_write_u8(
                machine, player + UINT32_C(0x0820), value
            );
        }
    }
    return status;
}

static int player_selector_opcode11_toggles_bit15(uint32_t selector)
{
    static const uint16_t selectors[] = {
        UINT16_C(0x0242), UINT16_C(0x0226), UINT16_C(0x0322),
        UINT16_C(0x0210), UINT16_C(0x0211), UINT16_C(0x0026),
        UINT16_C(0x02c3), UINT16_C(0x02b3), UINT16_C(0x0122),
        UINT16_C(0x025c), UINT16_C(0x0276), UINT16_C(0x0278)
    };
    size_t index = 0u;

    for (index = 0u; index < sizeof(selectors) / sizeof(selectors[0]); ++index) {
        if (selector == (uint32_t)selectors[index]) {
            return 1;
        }
    }
    return 0;
}

static vf2_status player_selector_apply_opcode11(
    vf2_model2a *machine,
    uint32_t player,
    uint32_t selector,
    uint32_t cursor
)
{
    uint32_t packed = 0u;
    uint32_t player_flags = 0u;
    uint8_t value = 0u;
    size_t index = 0u;
    vf2_status status = vf2_model2a_read_u32(
        machine, cursor + UINT32_C(1), &packed
    );
    static const uint16_t offsets[6] = {
        UINT16_C(0x0848), UINT16_C(0x0849), UINT16_C(0x084a),
        UINT16_C(0x084b), UINT16_C(0x084e), UINT16_C(0x084f)
    };

    if (status == VF2_OK && player_selector_opcode11_toggles_bit15(selector)) {
        packed ^= UINT32_C(1) << 15u;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, player, &player_flags);
    }
    if (status == VF2_OK && (player_flags & (UINT32_C(1) << 6u)) != 0u) {
        packed ^= UINT32_C(1) << 15u;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, player + UINT32_C(0x0844), packed
        );
    }
    if (status == VF2_OK) {
        status = player_selector_write_u16(
            machine, player + UINT32_C(0x06be),
            (uint16_t)(packed & UINT32_C(0x7fff))
        );
    }
    for (index = 0u; status == VF2_OK && index < 6u; ++index) {
        status = player_selector_read_u8(
            machine, cursor + UINT32_C(5) + (uint32_t)index, &value
        );
        if (status == VF2_OK) {
            status = player_selector_write_u8(
                machine, player + (uint32_t)offsets[index], value
            );
        }
    }
    return status;
}

static vf2_status player_selector_extended_apply_opcode(
    vf2_model2a *machine,
    uint32_t player,
    uint32_t selector,
    uint32_t cursor,
    uint8_t opcode
)
{
    switch (opcode) {
    case 2u:
        return player_selector_apply_opcode2(machine, player, selector, cursor);
    case 4u:
        return player_selector_apply_opcode4(machine, player, selector, cursor);
    case 11u:
        return player_selector_apply_opcode11(machine, player, selector, cursor);
    default:
        return VF2_ERROR_UNSUPPORTED;
    }
}
