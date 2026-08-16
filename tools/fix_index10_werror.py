from pathlib import Path

p = Path('src/recovered/texture_bridge_match.c')
s = p.read_text()
repls = {
'''    if(status==VF2_OK)status=phase17_index10_tile(machine,UINT32_C(15),UINT32_C(1),UINT16_C(24));if(status==VF2_OK)status=phase17_index10_tile(machine,UINT32_C(15),UINT32_C(61),UINT16_C(25));''': '''    if (status == VF2_OK) {
        status = phase17_index10_tile(
            machine, UINT32_C(15), UINT32_C(1), UINT16_C(24)
        );
    }
    if (status == VF2_OK) {
        status = phase17_index10_tile(
            machine, UINT32_C(15), UINT32_C(61), UINT16_C(25)
        );
    }''',
'''    if(status==VF2_OK)status=phase17_index10_tile(machine,UINT32_C(39),UINT32_C(1),UINT16_C(26));if(status==VF2_OK)status=phase17_index10_tile(machine,UINT32_C(39),UINT32_C(61),UINT16_C(27));''': '''    if (status == VF2_OK) {
        status = phase17_index10_tile(
            machine, UINT32_C(39), UINT32_C(1), UINT16_C(26)
        );
    }
    if (status == VF2_OK) {
        status = phase17_index10_tile(
            machine, UINT32_C(39), UINT32_C(61), UINT16_C(27)
        );
    }''',
'''        if(navigation!=0u&&(navigation&UINT32_C(0x04000104))==0u)return VF2_ERROR_UNSUPPORTED;status=phase17_index10_render_state1(machine);if(status==VF2_OK&&(navigation&UINT32_C(0x04000104))!=0u){status=phase17_index10_restore_menu(machine);exiting=1;instructions=UINT64_C(51001);calls=UINT64_C(1452);}else if(status==VF2_OK){instructions=UINT64_C(36729);calls=UINT64_C(1436);}''': '''        if (navigation != 0u &&
            (navigation & UINT32_C(0x04000104)) == 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = phase17_index10_render_state1(machine);
        if (status == VF2_OK &&
            (navigation & UINT32_C(0x04000104)) != 0u) {
            status = phase17_index10_restore_menu(machine);
            exiting = 1;
            instructions = UINT64_C(51001);
            calls = UINT64_C(1452);
        } else if (status == VF2_OK) {
            instructions = UINT64_C(36729);
            calls = UINT64_C(1436);
        }''',
'''    if(status==VF2_OK&&!exiting)status=vf2_model2a_write(machine,UINT32_C(0x005ff602),&spill,sizeof(spill));if(status!=VF2_OK)return status;''': '''    if (status == VF2_OK && !exiting) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
        );
    }
    if (status != VF2_OK) {
        return status;
    }'''
}
for old, new in repls.items():
    if old not in s:
        raise SystemExit('expected pattern not found')
    s = s.replace(old, new, 1)
p.write_text(s)
