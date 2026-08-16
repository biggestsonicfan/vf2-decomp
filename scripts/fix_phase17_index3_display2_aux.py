from pathlib import Path

path = Path('src/recovered/texture_bridge_match.c')
text = path.read_text()

old = '''                uint16_t tile = UINT16_C(0x800f);
                if (row == 0u) {
                    if (column == 0u) tile = UINT16_C(0x8018);
                    else if (column <= UINT32_C(60)) tile = UINT16_C(0x8011);
                    else if (column == UINT32_C(61)) tile = UINT16_C(0x8019);
                } else if (row == UINT32_C(47)) {
                    if (column == 0u) tile = UINT16_C(0x801a);
                    else if (column <= UINT32_C(60)) tile = UINT16_C(0x8010);
                    else if (column == UINT32_C(61)) tile = UINT16_C(0x801b);
                } else {
                    if (column == 0u) tile = UINT16_C(0x8013);
                    else if (column == UINT32_C(61)) tile = UINT16_C(0x8012);
                }
'''
new = '''                uint16_t tile = UINT16_C(0x800f);
                if (row == 0u) {
                    if (column == 0u) tile = UINT16_C(0x8018);
                    else if (column <= UINT32_C(60)) tile = UINT16_C(0x8011);
                    else if (column == UINT32_C(61)) tile = UINT16_C(0x8019);
                } else if (row == UINT32_C(47)) {
                    if (column == 0u) tile = UINT16_C(0x801a);
                    else if (column <= UINT32_C(60)) tile = UINT16_C(0x8010);
                    else if (column == UINT32_C(61)) tile = UINT16_C(0x801b);
                } else {
                    if (column == 0u) tile = UINT16_C(0x8013);
                    else if (column == UINT32_C(61)) tile = UINT16_C(0x8012);
                    else if (column >= UINT32_C(62)) tile = UINT16_C(0x8020);
                }
'''
if old not in text:
    raise SystemExit('display2 border block not found')
text = text.replace(old, new, 1)

needle = '''        if (status == VF2_OK) {
            status = write_phase17_index0_text(
                machine, UINT32_C(2 * 0x80), UINT32_C(23),
                "DISPLAY TEST 2/2"
            );
        }
'''
replacement = '''        if (status == VF2_OK) {
            uint8_t aux[UINT32_C(0xc00)];
            memset(aux, 0xff, sizeof(aux));
            status = vf2_model2a_write(
                machine, UINT32_C(0x0100d000), aux, sizeof(aux)
            );
        }
''' + needle
if needle not in text:
    raise SystemExit('display2 title block not found')
text = text.replace(needle, replacement, 1)

needle2 = '''            status = clear_tile_plane_64x48(
                machine, UINT32_C(0x01000000)
            );
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x005000a4), &phase_index,
                    sizeof(phase_index)
                );
            }
'''
replacement2 = '''            status = clear_tile_plane_64x48(
                machine, UINT32_C(0x01000000)
            );
            if (status == VF2_OK) {
                uint8_t aux[UINT32_C(0xc00)];
                memset(aux, 0, sizeof(aux));
                status = vf2_model2a_write(
                    machine, UINT32_C(0x0100d000), aux, sizeof(aux)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x005000a4), &phase_index,
                    sizeof(phase_index)
                );
            }
'''
# use last relevant occurrence inside state11; there are other teardowns before index3.
pos = text.find('    if (phase_a5 == UINT8_C(11)) {')
if pos < 0:
    raise SystemExit('state11 block not found')
head, tail = text[:pos], text[pos:]
if needle2 not in tail:
    raise SystemExit('state11 clear block not found')
tail = tail.replace(needle2, replacement2, 1)
text = head + tail

path.write_text(text)
