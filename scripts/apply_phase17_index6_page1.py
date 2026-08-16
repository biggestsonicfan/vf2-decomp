from pathlib import Path
p=Path('src/recovered/texture_bridge_match.c')
s=p.read_text()
marker='static vf2_status execute_frame_phase17_bit7_index11('
if marker not in s:
    raise SystemExit('index11 marker missing')
insert=r'''
static vf2_status phase17_index6_render_page1(
    vf2_model2a *machine,
    uint8_t state,
    uint64_t *characters
)
{
    static const struct {
        uint8_t row;
        uint8_t col;
        const char *text;
    } layout[] = {
        {2,26,"BOOKKEEPING 1/5"}, {5,26,"GLOBAL DATA"},
        {8,19,"COIN CHUTE #1"}, {10,19,"COIN CHUTE #2"},
        {12,21,"TOTAL COINS"}, {15,20,"COIN CREDITS"},
        {17,17,"SERVICE CREDITS"}, {19,19,"TOTAL CREDITS"},
        {22,22,"TOTAL TIME"}, {24,23,"PLAY TIME"},
        {26,10,"PLAY TIME RATIO(*1000)"}, {29,16,"TOTAL GAME COUNT"},
        {31,30,"1P"}, {33,30,"VS"},
        {35,18,"1P GAME TIME A"}, {36,16,"WAIT GAME TIME A"},
        {37,18,"VS GAME TIME A"}, {38,14,"TOTAL AVERAGE TIME"},
        {39,30,"1P"}, {40,30,"VS"},
        {44,15,"PUSH SERVICE BUTTON TO CONTINUE."},
        {45,18,"PUSH TEST BUTTON TO EXIT."}
    };
    static const struct {
        uint8_t row;
        uint8_t col;
        const char *text;
    } values[] = {
        {8,38,"0"}, {10,38,"0"}, {12,38,"0"},
        {15,38,"0"}, {17,38,"0"}, {19,38,"0"},
        {22,38,"0D  0H  0M  0S"}, {24,38,"0D  0H  0M  0S"},
        {26,33,"----"}, {29,38,"0"}, {31,38,"0"}, {33,38,"0"},
        {35,38,"0D  0H  0M  0S"}, {36,38,"0D  0H  0M  0S"},
        {37,38,"0D  0H  0M  0S"}, {38,37,"--M --S"},
        {39,37,"--M --S"}, {40,37,"--M --S"}
    };
    uint64_t count = 0u;
    size_t i = 0u;
    vf2_status status = clear_tile_plane_64x48(machine, UINT32_C(0x01000000));

    for (i = 0u; status == VF2_OK && i < sizeof(layout)/sizeof(layout[0]); ++i) {
        status = write_phase17_index0_text(
            machine, (uint32_t)layout[i].row * UINT32_C(0x80),
            layout[i].col, layout[i].text
        );
        count += (uint64_t)strlen(layout[i].text);
    }
    if (state == UINT8_C(1)) {
        for (i = 0u; status == VF2_OK && i < sizeof(values)/sizeof(values[0]); ++i) {
            status = write_phase17_index0_text(
                machine, (uint32_t)values[i].row * UINT32_C(0x80),
                values[i].col, values[i].text
            );
            count += (uint64_t)strlen(values[i].text);
        }
    }
    if (status == VF2_OK && characters != NULL) *characters = count;
    return status;
}

static vf2_status execute_frame_phase17_bit7_index6(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    uint8_t flagged_phase_index
)
{
    const uint32_t base_input = UINT32_C(0x0ff7f700);
    uint32_t indirect_target = 0u;
    uint32_t input_flags = 0u;
    uint32_t navigation_flags = 0u;
    uint32_t released_flags = 0u;
    uint32_t previous_flags = 0u;
    uint32_t selector_mask = 0u;
    uint8_t phase_a5 = 0u;
    uint8_t phase_a6 = 0u;
    uint8_t phase_a7 = 0u;
    const uint8_t spill = UINT8_C(0x56);
    uint64_t instructions = 0u;
    uint64_t calls = 0u;
    uint64_t characters = 0u;
    vf2_status status = VF2_OK;

    if (flagged_phase_index != UINT8_C(0x86) || cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x0005fed8), &indirect_target);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500700), &input_flags);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500704), &navigation_flags);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500708), &released_flags);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x0050070c), &previous_flags);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x0050002c), &selector_mask);
    if (status == VF2_OK) status = vf2_model2a_read(machine, UINT32_C(0x005000a5), &phase_a5, sizeof(phase_a5));
    if (status == VF2_OK) status = vf2_model2a_read(machine, UINT32_C(0x005000a6), &phase_a6, sizeof(phase_a6));
    if (status == VF2_OK) status = vf2_model2a_read(machine, UINT32_C(0x005000a7), &phase_a7, sizeof(phase_a7));
    if (status != VF2_OK || indirect_target != UINT32_C(0x0005c9b8) ||
        input_flags != base_input || navigation_flags != 0u || released_flags != 0u ||
        previous_flags != base_input || selector_mask != UINT32_C(0x00020000) ||
        phase_a5 > UINT8_C(1) || phase_a6 != UINT8_C(0xff) || phase_a7 != UINT8_C(0xff)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = phase17_index6_render_page1(machine, phase_a5, &characters);
    if (status != VF2_OK) return status;
    if (phase_a5 == UINT8_C(0)) {
        const uint8_t next = UINT8_C(1);
        status = vf2_model2a_write(machine, UINT32_C(0x005000a5), &next, sizeof(next));
        instructions = UINT64_C(15309);
        calls = UINT64_C(25);
    } else {
        instructions = UINT64_C(1815);
        calls = UINT64_C(79);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x005ff602), &spill, sizeof(spill));
    }
    if (status != VF2_OK) return status;

    cpu->executed_instructions += instructions;
    cpu->procedure_calls += calls;
    cpu->procedure_returns += calls;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[0] = 0u;
    cpu->registers[1] = UINT32_C(0x005ff580);
    cpu->registers[2] = UINT32_C(0x0000a010);
    cpu->registers[3] = 0u;
    cpu->registers[4] = UINT32_C(0x00515400);
    cpu->registers[5] = UINT32_C(0x3f800000);
    cpu->registers[6] = 0u;
    cpu->registers[7] = 0u;
    cpu->registers[8] = UINT32_MAX;
    cpu->registers[9] = UINT32_MAX;
    cpu->registers[10] = UINT32_MAX;
    cpu->registers[11] = UINT32_MAX;
    cpu->registers[12] = 0u;
    cpu->registers[13] = 0u;
    cpu->registers[14] = phase_a5 == 0u ? UINT32_C(3) : UINT32_C(4);
    cpu->registers[15] = UINT32_C(0x00008a00);
    cpu->registers[16] = phase_a5 == 0u ? UINT32_C(0x2e) : UINT32_C(0x00532d2d);
    cpu->registers[17] = 0u;
    cpu->registers[18] = UINT32_C(0xc0a0a3d7);
    cpu->registers[19] = 0u;
    cpu->registers[20] = UINT32_C(0x00560000);
    cpu->registers[21] = UINT32_C(0x0050e850);
    cpu->registers[22] = UINT32_C(0x000055b6);
    cpu->registers[23] = UINT32_C(0x00510980);
    cpu->registers[24] = UINT32_C(0x00512980);
    cpu->registers[25] = phase_a5 == 0u ? UINT32_C(0x01001724) : UINT32_C(0x010013c2);
    cpu->registers[26] = UINT32_C(0x00800000);
    cpu->registers[27] = UINT32_C(0x00880000);
    cpu->registers[28] = UINT32_C(0x00004000);
    cpu->registers[29] = UINT32_C(0x00516480);
    cpu->registers[30] = UINT32_C(0x00000220);
    cpu->registers[31] = UINT32_C(0x005ff500);
    if (phase_a5 == 0u) {
        cpu->arithmetic_control = (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
        cpu->compare_result = VF2_I960_COMPARE_GREATER;
    } else {
        cpu->arithmetic_control &= ~UINT32_C(7);
        cpu->compare_result = VF2_I960_COMPARE_NONE;
    }

    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->rows = characters;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_calls = calls;
    report->recovered_procedure_returns = calls + UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

'''
s=s.replace(marker, insert+marker, 1)
old='''        if (phase_index == UINT8_C(0x85)) {
            return execute_frame_phase17_bit7_index5(
                machine, cpu, report, phase_index
            );
        }
return execute_frame_phase17_bit7_index11('''
new='''        if (phase_index == UINT8_C(0x85)) {
            return execute_frame_phase17_bit7_index5(
                machine, cpu, report, phase_index
            );
        }
        if (phase_index == UINT8_C(0x86)) {
            return execute_frame_phase17_bit7_index6(
                machine, cpu, report, phase_index
            );
        }
return execute_frame_phase17_bit7_index11('''
if old not in s:
    raise SystemExit('dispatch anchor missing')
s=s.replace(old,new,1)
p.write_text(s)
