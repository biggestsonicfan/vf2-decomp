from pathlib import Path
p=Path('src/recovered/texture_bridge_match.c')
s=p.read_text()
a=s.index('static vf2_status execute_frame_phase17_bit7_index5(')
b=s.index('static vf2_status execute_frame_phase17_bit7_index11(',a)
f=s[a:b]

def one(old,new,label):
    global f
    n=f.count(old)
    if n!=1: raise SystemExit(f'{label}: {n}')
    f=f.replace(old,new,1)

one('''        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005ff680), UINT32_C(0x010005d4)
            );
        }
        if (status != VF2_OK) return status;

        instructions = UINT64_C(19056);''','''        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005ff680), UINT32_C(0x010005d4)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005ff780), UINT32_C(1)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005ff784), UINT32_C(0x0059c388)
            );
        }
        if (status == VF2_OK) {
            const uint8_t exit_scratch[2] = {UINT8_C(0x90), UINT8_C(0x59)};
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff801), exit_scratch,
                sizeof(exit_scratch)
            );
        }
        if (status != VF2_OK) return status;

        instructions = UINT64_C(19056);''','exit plus scratch')

one('''    cpu->registers[17] = edit_delta == 0 ? UINT32_C(0x3f4f5c29) : 0u;
    cpu->registers[18] = edit_delta == 0 ? UINT32_C(0xc0a0a3d7) : UINT32_C(15);''','''    cpu->registers[17] =
        (phase_a5 == UINT8_C(0) && edit_delta < 0)
            ? UINT32_C(0x3f4f5c29)
            : (edit_delta == 0 ? UINT32_C(0x3f4f5c29) : 0u);
    cpu->registers[18] =
        (phase_a5 == UINT8_C(0) && edit_delta < 0)
            ? UINT32_C(0xc0a0a3d7)
            : (edit_delta == 0 ? UINT32_C(0xc0a0a3d7) : UINT32_C(15));''','exit minus regs')

s=s[:a]+f+s[b:]
p.write_text(s)
