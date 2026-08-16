from pathlib import Path
path = Path('src/recovered/texture_bridge_match.c')
text = path.read_text()
old = '''    if (phase_a5 == UINT8_C(0)) {
        return finish_frame_phase17_index4_exit_control(
            machine, cpu, report, edit_delta, characters
        );
    }
    return finish_frame_phase17_index4_observed(
'''
new = '''    if (phase_a5 == UINT8_C(0) && navigation_delta == 0) {
        return finish_frame_phase17_index4_exit_control(
            machine, cpu, report, edit_delta, characters
        );
    }
    return finish_frame_phase17_index4_observed(
'''
if old not in text:
    raise SystemExit('exit navigation anchor not found')
path.write_text(text.replace(old, new, 1))
