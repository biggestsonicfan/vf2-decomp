from pathlib import Path

path = Path("src/recovered/hybrid.c")
text = path.read_text()

old = """                     extra_state == (UINT32_C(1) << 1u) ||\n                     extra_state == (UINT32_C(1) << 6u) ||\n"""
new = """                     extra_state == (UINT32_C(1) << 1u) ||\n                     (!countdown_path &&\n                      extra_state == (UINT32_C(1) << 4u)) ||\n                     extra_state == (UINT32_C(1) << 6u) ||\n"""
if old not in text:
    raise SystemExit("whitelist anchor not found")
text = text.replace(old, new, 1)

old = """                    /* Bit 4 uses a distinct fast path and two-sided bit 8\n                     * changes the fighter-order accounting. Keep those\n                     * unmeasured combinations fail-closed. */\n"""
new = """                    /* The isolated bit-4 fast path is ROM-backed only\n                     * with a zero countdown. Two-sided bit 8 and mixed\n                     * state combinations remain fail-closed. */\n"""
if old not in text:
    raise SystemExit("guard comment anchor not found")
text = text.replace(old, new, 1)

anchor = """    if (status == VF2_OK &&\n        (((r7 | r8) & (UINT32_C(1) << 1u)) != 0u) &&\n"""
block = """    if (status == VF2_OK && mode_bit6 && countdown_path &&\n        r7 == ((UINT32_C(1) << 8u) | (UINT32_C(1) << 4u)) &&\n        r8 == 0u) {\n        /* The swapped isolated state8+bit4 countdown corridor has not\n         * been recovered; keep the direct-entry orientation fail-closed. */\n        status = VF2_ERROR_UNSUPPORTED;\n    }\n"""
if anchor not in text:
    raise SystemExit("reverse countdown guard anchor not found")
text = text.replace(anchor, block + anchor, 1)

old = """        if (status == VF2_OK) {\n            if (relative_position_setbits == 0u) {\n                --body_instructions;\n"""
new = """        if (status == VF2_OK) {\n            if (!countdown_path && mode_bit6 &&\n                r7 == ((UINT32_C(1) << 8u) | (UINT32_C(1) << 4u)) &&\n                r8 == 0u) {\n                /* In the swapped fighter order, mode bit 6 plus isolated\n                 * state bits 8+4 rejoins eight instructions earlier than\n                 * the generic mode-bit-6 fallback accounting. */\n                body_instructions -= UINT32_C(8);\n            }\n            if (relative_position_setbits == 0u) {\n                --body_instructions;\n"""
if old not in text:
    raise SystemExit("post-accounting anchor not found")
text = text.replace(old, new, 1)

path.write_text(text)
