from pathlib import Path

path = Path("src/recovered/hybrid.c")
text = path.read_text()

old = '''    uint32_t signed_distance_instruction_delta = 0u;
    uint16_t fighter0_distance_raw = 0u;
'''
new = '''    uint32_t signed_distance_instruction_delta = 0u;
    uint32_t state8_bit1_instruction_delta = 0u;
    uint16_t fighter0_distance_raw = 0u;
'''
if old not in text:
    raise SystemExit("declaration anchor not found")
text = text.replace(old, new, 1)

old = '''                     extra_state == (UINT32_C(1) << 6u) ||
                     extra_state == (UINT32_C(1) << 14u) ||
'''
new = '''                     extra_state == (UINT32_C(1) << 1u) ||
                     extra_state == (UINT32_C(1) << 6u) ||
                     extra_state == (UINT32_C(1) << 14u) ||
'''
if old not in text:
    raise SystemExit("mode-bit6 whitelist anchor not found")
text = text.replace(old, new, 1)

old = '''                    /* Bit 1 enters the 0x188cc state tree, bit 4 uses a
                     * distinct fast path, and two-sided bit 8 changes the
                     * fighter-order accounting. Keep those fail-closed. */
'''
new = '''                    /* Bit 4 uses a distinct fast path and two-sided bit 8
                     * changes the fighter-order accounting. Keep those
                     * unmeasured combinations fail-closed. */
'''
if old not in text:
    raise SystemExit("guard comment anchor not found")
text = text.replace(old, new, 1)

anchor = '''    if (status == VF2_OK) {
        uint16_t progress = 0u;
        uint16_t limit = 0u;
        status = vf2_model2a_read_u32(machine, fighter1 + UINT32_C(0x00000844), &r14);
'''
block = '''    /* 0x188cc..0x18978: when state bit 8 and bit 1 are both set, the ROM
     * enters a distance/type sub-tree before the shared tail.  Account this
     * relative to the ordinary bit-8 path, whose two BBC instructions are
     * already included by the exact-state formula. */
    if (status == VF2_OK &&
        (r8 & ((UINT32_C(1) << 8u) | (UINT32_C(1) << 1u))) ==
            ((UINT32_C(1) << 8u) | (UINT32_C(1) << 1u))) {
        uint8_t fighter1_type = 0u;
        status = hybrid_read_u8(
            machine, fighter1 + UINT32_C(0x0000019f), &fighter1_type
        );
        if (status == VF2_OK && fighter1_type == UINT8_C(24)) {
            state8_bit1_instruction_delta = UINT32_C(2);
        } else if (status == VF2_OK) {
            const uint16_t distance0 = (uint16_t)(
                (uint32_t)(int32_t)(int16_t)fighter0_distance_raw - r11
            );
            const bool first_exact =
                distance0 == UINT16_C(0x1554) ||
                distance0 == UINT16_C(0xeaac);

            if (!first_exact) {
                state8_bit1_instruction_delta = UINT32_C(11);
            } else {
                const uint16_t distance1 = (uint16_t)(
                    (uint32_t)(int32_t)(int16_t)fighter1_distance_raw - r11
                );
                const bool second_mirrored_exact =
                    distance1 == UINT16_C(0x6aac) ||
                    distance1 == UINT16_C(0x9554);

                if (!second_mirrored_exact) {
                    state8_bit1_instruction_delta = UINT32_C(20);
                } else {
                    uint32_t state844 = 0u;
                    uint32_t state_mask = UINT32_C(0x03ff8000);

                    state8_bit1_instruction_delta = UINT32_C(30);
                    if ((int32_t)r9 > (int32_t)UINT32_C(0x40000000)) {
                        state_mask &= ~((UINT32_C(1) << 22u) |
                                        (UINT32_C(1) << 23u));
                        state8_bit1_instruction_delta += UINT32_C(2);
                    }
                    if ((int32_t)r9 > (int32_t)UINT32_C(0x3fe66666)) {
                        state_mask &= ~((UINT32_C(1) << 18u) |
                                        (UINT32_C(1) << 21u) |
                                        (UINT32_C(1) << 19u));
                        state8_bit1_instruction_delta += UINT32_C(3);
                    }
                    if ((int32_t)r9 > (int32_t)UINT32_C(0x3fcccccd)) {
                        state_mask &= ~(UINT32_C(1) << 20u);
                        state8_bit1_instruction_delta += UINT32_C(1);
                    }
                    status = vf2_model2a_read_u32(
                        machine, fighter1 + UINT32_C(0x00000844), &state844
                    );
                    if (status == VF2_OK) {
                        r10 |= state844 & state_mask;
                    }
                }
            }
        }
    }
'''
if anchor not in text:
    raise SystemExit("state8-bit1 insertion anchor not found")
text = text.replace(anchor, block + anchor, 1)

old = '''            const uint32_t observed_state_mask =
                (UINT32_C(1) << 4u) | (UINT32_C(1) << 6u) |
'''
new = '''            const uint32_t observed_state_mask =
                (UINT32_C(1) << 1u) | (UINT32_C(1) << 4u) |
                (UINT32_C(1) << 6u) |
'''
if old not in text:
    raise SystemExit("observed-state anchor not found")
text = text.replace(old, new, 1)

old = '''            body_instructions += signed_distance_instruction_delta;
        }
'''
new = '''            body_instructions += signed_distance_instruction_delta;
            body_instructions += state8_bit1_instruction_delta;
        }
'''
if old not in text:
    raise SystemExit("instruction-delta anchor not found")
text = text.replace(old, new, 1)

path.write_text(text)
