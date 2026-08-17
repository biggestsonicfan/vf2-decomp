from pathlib import Path

path = Path("src/recovered/hybrid.c")
text = path.read_text()

old = """                     extra_state == (UINT32_C(1) << 1u) ||\n                     extra_state == (UINT32_C(1) << 6u) ||\n"""
new = """                     extra_state == (UINT32_C(1) << 1u) ||\n                     extra_state == (UINT32_C(1) << 4u) ||\n                     extra_state == (UINT32_C(1) << 6u) ||\n"""
if old not in text:
    raise SystemExit("whitelist anchor not found")
text = text.replace(old, new, 1)

old = """        if (status == VF2_OK) {\n            if (relative_position_setbits == 0u) {\n                --body_instructions;\n"""
new = """        if (status == VF2_OK) {\n            if (mode_bit6 &&\n                r7 == ((UINT32_C(1) << 8u) | (UINT32_C(1) << 4u)) &&\n                r8 == 0u) {\n                /* In the swapped fighter order, mode bit 6 plus isolated\n                 * state bits 8+4 rejoins eight instructions earlier than\n                 * the generic mode-bit-6 fallback accounting. */\n                body_instructions -= UINT32_C(8);\n            }\n            if (relative_position_setbits == 0u) {\n                --body_instructions;\n"""
if old not in text:
    raise SystemExit("post-accounting anchor not found")
text = text.replace(old, new, 1)

path.write_text(text)
