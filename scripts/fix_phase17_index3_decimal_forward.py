from pathlib import Path

path = Path('src/recovered/texture_bridge_match.c')
text = path.read_text()
marker = 'static vf2_status finish_frame_phase17_index3_observed(\n'
prototype = '''static vf2_status phase17_zero_render_decimal(\n    vf2_model2a *machine,\n    int32_t value,\n    uint32_t destination\n);\n\n'''
if prototype not in text:
    if marker not in text:
        raise SystemExit('index3 helper marker not found')
    text = text.replace(marker, prototype + marker, 1)
path.write_text(text)
