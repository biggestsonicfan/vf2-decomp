from pathlib import Path

path = Path("src/recovered/hybrid.c")
text = path.read_text()

anchor = '''    /* The nonzero countdown corridor enters the shared 0x18890 tail. */
'''
guard = '''    if (status == VF2_OK &&
        (r7 & (UINT32_C(1) << 8u)) != 0u &&
        (r8 & (UINT32_C(1) << 8u)) != 0u &&
        (countdown_path ||
         r7 != (UINT32_C(1) << 8u) ||
         r8 != (UINT32_C(1) << 8u))) {
        /* Only the isolated zero-countdown bilateral state-bit-8 corridor
         * is ROM-backed. Keep mixed/countdown bilateral states fail-closed. */
        status = VF2_ERROR_UNSUPPORTED;
    }
'''
if anchor not in text:
    raise SystemExit("bilateral guard anchor not found")
text = text.replace(anchor, guard + anchor, 1)

old = '''                mode_bit6_supported_bit8 =
                    r7 == 0u &&
                    (extra_state == 0u ||
'''
new = '''                const bool bilateral_state8 =
                    r7 == (UINT32_C(1) << 8u) && extra_state == 0u;
                const bool bilateral_second_order =
                    bilateral_state8 && return_address == UINT32_C(0x000164c4);
                mode_bit6_supported_bit8 =
                    (r7 == 0u &&
                     (extra_state == 0u ||
'''
if old not in text:
    raise SystemExit("mode-bit6 start anchor not found")
text = text.replace(old, new, 1)

old = '''                     extra_state ==
                         ((UINT32_C(1) << 15u) |
                          (UINT32_C(1) << 16u)));
                if (!mode_bit6_supported_bit8) {
'''
new = '''                     extra_state ==
                         ((UINT32_C(1) << 15u) |
                          (UINT32_C(1) << 16u)))) ||
                    (bilateral_state8 &&
                     return_address == UINT32_C(0x000164b0));
                if (!mode_bit6_supported_bit8 && !bilateral_second_order) {
'''
if old not in text:
    raise SystemExit("mode-bit6 end anchor not found")
text = text.replace(old, new, 1)

old = '''            body_instructions += signed_distance_instruction_delta;
            body_instructions += state8_bit1_instruction_delta;
        }
'''
new = '''            body_instructions += signed_distance_instruction_delta;
            body_instructions += state8_bit1_instruction_delta;
            if (return_address == UINT32_C(0x000164b0) &&
                r7 == (UINT32_C(1) << 8u) &&
                r8 == (UINT32_C(1) << 8u)) {
                /* Bilateral state bit 8 adds one instruction only in the
                 * first fighter order. */
                ++body_instructions;
            }
            if (mode_bit6 && return_address == UINT32_C(0x000164c4) &&
                r7 == (UINT32_C(1) << 8u) &&
                r8 == (UINT32_C(1) << 8u)) {
                /* The swapped bilateral mode-bit-6 call rejoins one
                 * instruction earlier than the generic fallback. */
                --body_instructions;
            }
        }
'''
if old not in text:
    raise SystemExit("accounting anchor not found")
text = text.replace(old, new, 1)

path.write_text(text)
