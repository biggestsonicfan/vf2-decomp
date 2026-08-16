from pathlib import Path

path = Path("src/recovered/texture_bridge_match.c")
text = path.read_text()
old = "indirect_target != UINT32_C(0x00059180)"
new = "indirect_target != UINT32_C(0x00059164)"
if text.count(old) != 1:
    raise SystemExit(f"expected exactly one match, found {text.count(old)}")
path.write_text(text.replace(old, new, 1))
