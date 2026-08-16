from pathlib import Path

path = Path("src/recovered/texture_bridge_match.c")
text = path.read_text()
start = text.index("static vf2_status execute_frame_phase17_bit7_index1(")
end = text.index("static vf2_status execute_frame_phase17_bit7_index11(", start)
block = text[start:end]

old_guard = """        input_flags != UINT32_C(0x0ff7f700) || navigation_flags != 0u ||
        released_flags != 0u ||
        previous_flags != UINT32_C(0x0ff7f700) ||
        selector_mask != UINT32_C(0x00020000) ||
        phase_a5 > UINT8_C(1) || phase_a6 != UINT8_C(0xff)) {
"""
new_guard = """        input_flags != UINT32_C(0x0ff7f700) || navigation_flags != 0u ||
        (released_flags != 0u && released_flags != UINT32_C(4)) ||
        previous_flags != UINT32_C(0x0ff7f700) ||
        selector_mask != UINT32_C(0x00020000) ||
        phase_a5 > UINT8_C(1) || phase_a6 != UINT8_C(0xff)) {
"""
if block.count(old_guard) != 1:
    raise SystemExit(f"guard anchor count={block.count(old_guard)}")
block = block.replace(old_guard, new_guard, 1)

old_branch = """    if (phase_a5 == UINT8_C(1)) {
        const uint8_t spill = UINT8_C(0x56);

        status = write_u16(machine, UINT32_C(0x005ff600), UINT16_C(0));
"""
new_branch = """    if (phase_a5 == UINT8_C(1)) {
        const uint8_t spill = UINT8_C(0x56);
        const int release_transition = released_flags == UINT32_C(4);

        if (release_transition) {
            const uint8_t next_phase = UINT8_C(2);
            status = vf2_model2a_write(
                machine, UINT32_C(0x005000a5), &next_phase, sizeof(next_phase)
            );
        }
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x005ff600), UINT16_C(0));
        }
"""
if block.count(old_branch) != 1:
    raise SystemExit(f"branch anchor count={block.count(old_branch)}")
block = block.replace(old_branch, new_branch, 1)

old_count = """        cpu->executed_instructions += UINT64_C(1622);
        cpu->procedure_calls += UINT64_C(37);
"""
new_count = """        cpu->executed_instructions +=
            release_transition ? UINT64_C(1624) : UINT64_C(1622);
        cpu->procedure_calls += UINT64_C(37);
"""
if block.count(old_count) != 1:
    raise SystemExit(f"count anchor count={block.count(old_count)}")
block = block.replace(old_count, new_count, 1)

old_report = """        report->changed_values = UINT64_C(3);
        report->bytes_written = 3u;
        report->recovered_instruction_count = UINT64_C(1622);
"""
new_report = """        report->changed_values = release_transition ? UINT64_C(4) : UINT64_C(3);
        report->bytes_written = release_transition ? 4u : 3u;
        report->recovered_instruction_count =
            release_transition ? UINT64_C(1624) : UINT64_C(1622);
"""
if block.count(old_report) != 1:
    raise SystemExit(f"report anchor count={block.count(old_report)}")
block = block.replace(old_report, new_report, 1)

text = text[:start] + block + text[end:]
path.write_text(text)
