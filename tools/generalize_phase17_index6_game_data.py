from pathlib import Path

p=Path('src/recovered/texture_bridge_match.c')
s=p.read_text()
start=s.index('static vf2_status phase17_index6_render_game_data_static(')
end=s.index('static vf2_status execute_frame_phase17_bit7_index6(',start)
seg=s[start:end]

seg=seg.replace(
'''static vf2_status phase17_index6_render_game_data_static(
    vf2_model2a *machine,
    uint64_t *characters
)''',
'''static vf2_status phase17_index6_render_game_data_static(
    vf2_model2a *machine,
    uint8_t slot,
    uint64_t *characters
)''',1)
seg=seg.replace(
'''    static const struct { uint8_t row, col; const char *text; } fixed[] = {
        {4,22,"GAME DATA"},{4,34,"AKIRA"},''',
'''    static const char *names[16] = {
        "AKIRA","JACKY","SARAH","KAGE","LAU","JEFFRY","PAI","WOLF",
        "SHUN","ACHO","LION","MUE","KOJAC","-----","-----","-----"
    };
    static const char *headers[16] = {
        "AKIRA  ","JACKY  ","SARAH  ","KAGE   ","LAU    ","JEFFRY ",
        "PAI    ","WOLF   ","SHUN   ","DURAL  ","LION   ","MUE    ",
        "KOJACKY","-------","-------","-------"
    };
    static const struct { uint8_t row, col; const char *text; } fixed[] = {
        {4,22,"GAME DATA"},''',1)
seg=seg.replace('''        {45,18,"PUSH TEST BUTTON TO EXIT."},
        {2,38,"AKIRA  "}
    };''','''        {45,18,"PUSH TEST BUTTON TO EXIT."}
    };''',1)
seg=seg.replace('''    uint64_t count = 0u;
    vf2_status status = VF2_OK;

    for (row = 4u;''','''    uint64_t count = 0u;
    vf2_status status = VF2_OK;

    if (slot >= UINT8_C(16)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    for (row = 4u;''',1)
needle='''    for (i = 0u; status == VF2_OK && i < sizeof(fixed)/sizeof(fixed[0]); ++i) {
        status = write_phase17_index0_text(
            machine, (uint32_t)fixed[i].row * UINT32_C(0x80),
            fixed[i].col, fixed[i].text
        );
        count += (uint64_t)strlen(fixed[i].text);
    }
'''
replacement=needle+'''    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(4 * 0x80), UINT32_C(34), names[slot]
        );
        count += (uint64_t)strlen(names[slot]);
    }
    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(2 * 0x80), UINT32_C(38), headers[slot]
        );
        count += (uint64_t)strlen(headers[slot]);
    }
'''
if needle not in seg: raise SystemExit('static loop missing')
seg=seg.replace(needle,replacement,1)

old='''static vf2_status phase17_index6_finish_game_data_pair(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    uint8_t state
)'''
new=old
if old not in seg: raise SystemExit('finish sig missing')

seg=seg.replace('''    if (state == UINT8_C(10)) {
        const uint8_t next = UINT8_C(11);
        status = phase17_index6_render_game_data_static(machine, &characters);''','''    if (state >= UINT8_C(10) && state <= UINT8_C(40) &&
        (state & UINT8_C(1)) == 0u) {
        const uint8_t slot = (uint8_t)((state - UINT8_C(10)) >> 1u);
        const uint8_t next = (uint8_t)(state + UINT8_C(1));
        static const uint8_t name_lengths[16] = {
            5u,5u,5u,4u,3u,6u,3u,4u,4u,4u,4u,3u,5u,5u,5u,5u
        };
        status = phase17_index6_render_game_data_static(machine, slot, &characters);''',1)
seg=seg.replace('''        instructions = UINT64_C(20595);
        calls = UINT64_C(144);
    } else if (state == UINT8_C(11)) {
        uint32_t offset = 0u;''','''        instructions = UINT64_C(20555) +
            UINT64_C(8) * (uint64_t)name_lengths[slot];
        calls = UINT64_C(144);
    } else if (state >= UINT8_C(11) && state <= UINT8_C(41) &&
               (state & UINT8_C(1)) != 0u) {
        const uint8_t slot = (uint8_t)((state - UINT8_C(11)) >> 1u);
        uint32_t offset = 0u;''',1)
seg=seg.replace('''            status = vf2_model2a_read(machine, UINT32_C(0x01d00000) + offset, &value, sizeof(value));''','''            status = vf2_model2a_read(
                machine, UINT32_C(0x01d00000) +
                    (uint32_t)slot * UINT32_C(0x200) + offset,
                &value, sizeof(value)
            );''',1)
seg=seg.replace('''    cpu->registers[14]=UINT32_C(3)+(uint32_t)state;
    cpu->registers[15]=UINT32_C(0x00008a00);
    cpu->registers[16]=state==UINT8_C(10)?UINT32_C(0x2e):0u;''','''    cpu->registers[14]=UINT32_C(3)+(uint32_t)state;
    cpu->registers[15]=UINT32_C(0x00008a00);
    cpu->registers[16]=(state & UINT8_C(1)) == 0u ? UINT32_C(0x2e) : 0u;''',1)
seg=seg.replace('''    cpu->registers[25]=state==UINT8_C(10)?UINT32_C(0x01001724):UINT32_C(0x010015e8);''','''    cpu->registers[25]=(state & UINT8_C(1)) == 0u
        ? UINT32_C(0x01001724) : UINT32_C(0x010015e8);''',1)
seg=seg.replace('''    if (state == UINT8_C(10)) {''','''    if ((state & UINT8_C(1)) == 0u) {''',1)

s=s[:start]+seg+s[end:]
# Expand dispatcher admission 11 -> 41.
old='''        phase_a5 > UINT8_C(11) || phase_a6 != UINT8_C(0xff) ||
        ((phase_a5 < UINT8_C(9) && phase_a7 != UINT8_C(0xff)) ||
         (phase_a5 == UINT8_C(9) && phase_a7 > UINT8_C(9)) ||
         (phase_a5 >= UINT8_C(10) && phase_a7 != UINT8_C(0xff)))) {'''
new='''        phase_a5 > UINT8_C(41) || phase_a6 != UINT8_C(0xff) ||
        ((phase_a5 < UINT8_C(9) && phase_a7 != UINT8_C(0xff)) ||
         (phase_a5 == UINT8_C(9) && phase_a7 > UINT8_C(9)) ||
         (phase_a5 >= UINT8_C(10) && phase_a7 != UINT8_C(0xff)))) {'''
if old not in s: raise SystemExit('precondition current form missing')
s=s.replace(old,new,1)

p.write_text(s)

note=Path('decomp/i960/notes/selector17_index6_bookkeeping_game_data.md')
n=note.read_text()
n += '''\n## All hidden character slots\n\nThe shared pair is reused for 16 slots, states 10..41. The body-name table is\n`AKIRA, JACKY, SARAH, KAGE, LAU, JEFFRY, PAI, WOLF, SHUN, ACHO, LION, MUE,\nKOJAC, -----, -----, -----`. The independent header table labels the same slots\n`AKIRA, JACKY, SARAH, KAGE, LAU, JEFFRY, PAI, WOLF, SHUN, DURAL, LION, MUE,\nKOJACKY, -------, -------, -------`; the ACHO/DURAL and KOJAC/KOJACKY aliases\nare therefore preserved rather than normalized.\n\nDirect ROM measurements of all 32 states with each 512-byte block zeroed show\nthat every odd render state is invariant at 4548/131/132 from the index-6\ndispatcher, while construction-state cost is `20527 + 8*strlen(body_name)`\ninstructions with 142/143 calls/returns. Adding the outer frame overhead gives\nthe recovered accounting `20555 + 8*strlen(body_name)` / 144 / 145 for even\nstates and 4576 / 133 / 134 for odd states.\n'''
n=n.replace('This cut intentionally admits only the measured zero-data state-11\npath. The ROM reuses the same 10/11 bodies for later hidden character\nslots; non-zero statistics and repeated slots will be generalized in\nthe next recovery cut.','This cut admits the measured zero-data path for all sixteen hidden GAME DATA slots. Non-zero statistics remain an explicit next extension.')
note.write_text(n)
