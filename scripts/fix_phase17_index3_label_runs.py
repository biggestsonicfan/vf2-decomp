from pathlib import Path

path = Path("src/recovered/texture_bridge_match.c")
text = path.read_text()
old = '''    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(41 * 0x80), UINT32_C(17),
            "COLOR         BIAS  GAIN SCROLL:"
        );
    }
'''
new = '''    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(41 * 0x80), UINT32_C(17), "COLOR"
        );
    }
    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(41 * 0x80), UINT32_C(31), "BIAS"
        );
    }
    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(41 * 0x80), UINT32_C(37), "GAIN SCROLL:"
        );
    }
'''
if text.count(old) != 1:
    raise SystemExit(f"label anchor count={text.count(old)}")
path.write_text(text.replace(old, new, 1))
