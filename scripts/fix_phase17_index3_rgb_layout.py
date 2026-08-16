from pathlib import Path

path = Path('src/recovered/texture_bridge_match.c')
text = path.read_text()

old = '''    for (channel = 0u; status == VF2_OK && channel < 3u; ++channel) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x00500234) + channel,
            &values[channel], sizeof(values[channel])
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x00500237) + channel,
                &values[channel + UINT32_C(3)],
                sizeof(values[channel + UINT32_C(3)])
            );
        }
    }
'''
new = '''    for (channel = 0u; status == VF2_OK && channel < 3u; ++channel) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x00500234) + channel * UINT32_C(2),
            &values[channel], sizeof(values[channel])
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x00500235) + channel * UINT32_C(2),
                &values[channel + UINT32_C(3)],
                sizeof(values[channel + UINT32_C(3)])
            );
        }
    }
'''
if old not in text:
    raise SystemExit('RGB scratch layout block not found')
text = text.replace(old, new, 1)

old_stride = 'channel_bases[channel] + level * UINT32_C(0x180) +\n'
new_stride = 'channel_bases[channel] + level * UINT32_C(0x200) +\n'
if old_stride not in text:
    raise SystemExit('RGB LUT stride not found')
text = text.replace(old_stride, new_stride, 1)

path.write_text(text)
