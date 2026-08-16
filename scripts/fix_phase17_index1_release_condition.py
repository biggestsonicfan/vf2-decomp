from pathlib import Path

path = Path("src/recovered/texture_bridge_match.c")
text = path.read_text()
start = text.index("static vf2_status execute_frame_phase17_bit7_index1(")
end = text.index("static vf2_status execute_frame_phase17_bit7_index11(", start)
block = text[start:end]
anchor = """        cpu->registers[31] = UINT32_C(0x005ff500);
        set_equal_condition(cpu);
        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
"""
replacement = """        cpu->registers[31] = UINT32_C(0x005ff500);
        if (release_transition) {
            cpu->arithmetic_control =
                (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(4);
            cpu->compare_result = VF2_I960_COMPARE_LESS;
        } else {
            set_equal_condition(cpu);
        }
        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
"""
if block.count(anchor) != 1:
    raise SystemExit(f"release poststate anchor count={block.count(anchor)}")
block = block.replace(anchor, replacement, 1)
text = text[:start] + block + text[end:]
path.write_text(text)
