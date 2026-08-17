from pathlib import Path

path = Path("src/recovered/hybrid.c")
text = path.read_text()

old = '''    if (status == VF2_OK &&
        (r8 & ((UINT32_C(1) << 8u) | (UINT32_C(1) << 1u))) ==
            ((UINT32_C(1) << 8u) | (UINT32_C(1) << 1u)) &&
        (r7 != 0u ||
         r8 != ((UINT32_C(1) << 8u) | (UINT32_C(1) << 1u)))) {
        /* Only the isolated state8+bit1 corridor is ROM-backed here.
         * Mixed state8+bit1 combinations remain fail-closed. */
        status = VF2_ERROR_UNSUPPORTED;
    }
'''
new = '''    if (status == VF2_OK &&
        (((r7 | r8) & (UINT32_C(1) << 1u)) != 0u) &&
        (((r7 | r8) & (UINT32_C(1) << 8u)) != 0u)) {
        const uint32_t isolated_state8_bit1 =
            (UINT32_C(1) << 8u) | (UINT32_C(1) << 1u);
        const bool forward_isolated =
            r7 == 0u && r8 == isolated_state8_bit1;
        const bool reverse_isolated =
            r7 == isolated_state8_bit1 && r8 == 0u;
        if (!forward_isolated && !reverse_isolated) {
            /* Only the two fighter-order orientations of isolated
             * state8+bit1 are ROM-backed here. */
            status = VF2_ERROR_UNSUPPORTED;
        }
    }
'''
if old not in text:
    raise SystemExit("isolated guard anchor not found")
text = text.replace(old, new, 1)

old = '''            const uint32_t active_state = (r7 | r8) & observed_state_mask;
            const bool isolated_state8_bit1 =
                r7 == 0u &&
                r8 == ((UINT32_C(1) << 8u) | (UINT32_C(1) << 1u));
            const bool exact_state_accounting =
                !countdown_path &&
                (!mode_bit6 || mode_bit6_supported_bit8) &&
                active_state != 0u &&
                (isolated_state8_bit1 ||
                 ((r7 | r8) & ~observed_state_mask) == 0u);
'''
new = '''            const uint32_t active_state = (r7 | r8) & observed_state_mask;
            const uint32_t state8_bit1 =
                (UINT32_C(1) << 8u) | (UINT32_C(1) << 1u);
            const bool isolated_state8_bit1_forward =
                r7 == 0u && r8 == state8_bit1;
            const bool isolated_state8_bit1_reverse =
                r7 == state8_bit1 && r8 == 0u;
            const bool exact_state_accounting =
                !countdown_path &&
                (!mode_bit6 || mode_bit6_supported_bit8) &&
                active_state != 0u &&
                (isolated_state8_bit1_forward ||
                 isolated_state8_bit1_reverse ||
                 ((r7 | r8) & ~observed_state_mask) == 0u);
'''
if old not in text:
    raise SystemExit("exact accounting anchor not found")
text = text.replace(old, new, 1)

path.write_text(text)
