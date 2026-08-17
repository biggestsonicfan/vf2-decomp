from pathlib import Path

path = Path("src/recovered/hybrid.c")
text = path.read_text()

old = """                     extra_state == (UINT32_C(1) << 1u) ||\n                     extra_state == (UINT32_C(1) << 6u) ||\n"""
new = """                     extra_state == (UINT32_C(1) << 1u) ||\n                     extra_state == (UINT32_C(1) << 4u) ||\n                     extra_state == (UINT32_C(1) << 6u) ||\n"""
if old not in text:
    raise SystemExit("whitelist anchor not found")
text = text.replace(old, new, 1)

old = """            const bool isolated_state8_bit1_reverse =\n                r7 == state8_bit1 && r8 == 0u;\n            const bool exact_state_accounting =\n                !countdown_path &&\n                (!mode_bit6 || mode_bit6_supported_bit8) &&\n"""
new = """            const bool isolated_state8_bit1_reverse =\n                r7 == state8_bit1 && r8 == 0u;\n            const uint32_t state8_bit4 =\n                (UINT32_C(1) << 8u) | (UINT32_C(1) << 4u);\n            const bool isolated_state8_bit4_reverse =\n                r7 == state8_bit4 && r8 == 0u;\n            const bool exact_state_accounting =\n                !countdown_path &&\n                (!mode_bit6 || mode_bit6_supported_bit8 ||\n                 isolated_state8_bit4_reverse) &&\n"""
if old not in text:
    raise SystemExit("exact-accounting anchor not found")
text = text.replace(old, new, 1)

path.write_text(text)
