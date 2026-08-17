from pathlib import Path

path = Path("src/recovered/hybrid.c")
text = path.read_text()
old = """                     extra_state == (UINT32_C(1) << 1u) ||\n                     extra_state == (UINT32_C(1) << 6u) ||\n"""
new = """                     extra_state == (UINT32_C(1) << 1u) ||\n                     extra_state == (UINT32_C(1) << 4u) ||\n                     extra_state == (UINT32_C(1) << 6u) ||\n"""
if old not in text:
    raise SystemExit("whitelist anchor not found")
path.write_text(text.replace(old, new, 1))
