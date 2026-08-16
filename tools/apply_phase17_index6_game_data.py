from pathlib import Path

p = Path('src/recovered/texture_bridge_match.c')
s = p.read_text()
marker = 'static vf2_status execute_frame_phase17_bit7_index6(\n'
if marker not in s:
    raise SystemExit('index6 marker not found')

helpers = r'''static vf2_status phase17_index6_render_game_data_static(
    vf2_model2a *machine,
    uint64_t *characters
)
{
    static const struct { uint8_t row, col; const char *text; } fixed[] = {
        {4,22,"GAME DATA"},{4,34,"AKIRA"},
        {6,17,"GAME COUNT 1P"},{7,17,"GAME COUNT VS"},
        {8,4,"TOTAL TIME 1P"},{9,4,"TOTAL TIME VS"},
        {10,6,"AVG TIME 1P"},{11,6,"AVG TIME VS"},
        {12,6,"MIN TIME 1P"},{13,6,"MIN TIME VS"},
        {14,6,"MAX TIME 1P"},{15,6,"MAX TIME VS"},
        {16,13,"CONTINUE COUNT 1P"},{17,13,"CONTINUE COUNT VS"},
        {18,18,"SET COUNT 1P"},{19,18,"SET COUNT VS"},
        {20,17,"DRAW COUNT 1P"},{21,17,"DRAW COUNT VS"},
        {22,10,"WIN BY K.O. COUNT 1P"},{23,10,"WIN BY K.O. COUNT VS"},
        {24,7,"WIN BY RINGOUT COUNT 1P"},{25,7,"WIN BY RINGOUT COUNT VS"},
        {26,9,"WIN BY JUDGE COUNT 1P"},{27,9,"WIN BY JUDGE COUNT VS"},
        {28,3,"--------------1P DATA-------------"},
        {29,3,"RND.TOTAL WIN.  TOTAL AVG. WINRATE"},
        {30,3,"(th)  (times)     (second) (*1000)"},
        {31,4," 1"},{32,4," 2"},{33,4," 3"},{34,4," 4"},
        {35,4," 5"},{36,4," 6"},{37,4," 7"},{38,4," 8"},
        {39,4," 9"},{40,4,"10"},{41,4,"11"},
        {8,42,"-----VS DATA-----"},{9,42,"GAMETIME    COUNT"},
        {10,42,"   (sec)  (times)"},
        {44,15,"PUSH SERVICE BUTTON TO CONTINUE."},
        {45,18,"PUSH TEST BUTTON TO EXIT."},
        {2,38,"AKIRA  "}
    };
    static const char *ranges[33] = {
        "~ 10","~ 13","~ 16","~ 19","~ 22","~ 25","~ 28","~ 31",
        "~ 34","~ 37","~ 40","~ 43","~ 46","~ 49","~ 52","~ 55",
        "~ 58","~ 61","~ 64","~ 67","~ 70","~ 73","~ 76","~ 79",
        "~ 82","~ 85","~ 88","~ 91","~ 94","~ 97","~100","~103"," 104~"
    };
    uint32_t row = 0u, col = 0u;
    size_t i = 0u;
    uint64_t count = 0u;
    vf2_status status = VF2_OK;

    for (row = 4u; status == VF2_OK && row < 48u; ++row) {
        for (col = 0u; status == VF2_OK && col < 62u; ++col) {
            status = write_u16(
                machine, UINT32_C(0x01000000) + row * UINT32_C(0x80) +
                    col * UINT32_C(2), 0u
            );
        }
    }
    for (i = 0u; status == VF2_OK && i < sizeof(fixed)/sizeof(fixed[0]); ++i) {
        status = write_phase17_index0_text(
            machine, (uint32_t)fixed[i].row * UINT32_C(0x80),
            fixed[i].col, fixed[i].text
        );
        count += (uint64_t)strlen(fixed[i].text);
    }
    for (i = 0u; status == VF2_OK && i < 33u; ++i) {
        const uint32_t r = UINT32_C(11) + (uint32_t)i;
        const uint32_t c = i == 32u ? UINT32_C(42) : UINT32_C(43);
        status = write_phase17_index0_text(
            machine, r * UINT32_C(0x80), c, ranges[i]
        );
        count += (uint64_t)strlen(ranges[i]);
    }
    if (status == VF2_OK && characters != NULL) {
        *characters = count;
    }
    return status;
}

static vf2_status phase17_index6_render_game_data_zero(
    vf2_model2a *machine,
    uint64_t *characters
)
{
    static const uint8_t count_rows[] = {
        6u,7u,16u,17u,18u,19u,20u,21u,22u,23u,24u,25u,26u,27u
    };
    uint32_t row = 0u;
    size_t i = 0u;
    uint64_t count = 0u;
    vf2_status status = VF2_OK;

    for (i = 0u; status == VF2_OK && i < sizeof(count_rows); ++i) {
        status = phase17_index6_decimal(
            machine, 0,
            UINT32_C(0x01000000) + (uint32_t)count_rows[i] * UINT32_C(0x80) +
                UINT32_C(31 * 2)
        );
    }
    if (status == VF2_OK) status = write_phase17_index0_text(machine, UINT32_C(8 * 0x80), UINT32_C(18), "     0D  0H  0M  0S");
    if (status == VF2_OK) status = write_phase17_index0_text(machine, UINT32_C(9 * 0x80), UINT32_C(18), "     0D  0H  0M  0S");
    if (status == VF2_OK) status = write_phase17_index0_text(machine, UINT32_C(10 * 0x80), UINT32_C(18), "    --M --S");
    if (status == VF2_OK) status = write_phase17_index0_text(machine, UINT32_C(11 * 0x80), UINT32_C(18), "    --M --S");
    for (row = 12u; status == VF2_OK && row <= 15u; ++row) {
        status = write_phase17_index0_text(machine, row * UINT32_C(0x80), UINT32_C(18), "     0M  0S");
    }
    for (row = 31u; status == VF2_OK && row <= 41u; ++row) {
        status = phase17_index6_decimal(machine, 0, UINT32_C(0x01000000) + row * UINT32_C(0x80) + UINT32_C(6 * 2));
        if (status == VF2_OK) status = phase17_index6_decimal(machine, 0, UINT32_C(0x01000000) + row * UINT32_C(0x80) + UINT32_C(12 * 2));
        if (status == VF2_OK) status = phase17_index6_decimal(machine, 0, UINT32_C(0x01000000) + row * UINT32_C(0x80) + UINT32_C(18 * 2));
        if (status == VF2_OK) status = write_phase17_index0_text(machine, row * UINT32_C(0x80), UINT32_C(24), "  ----");
    }
    for (row = 11u; status == VF2_OK && row <= 43u; ++row) {
        status = phase17_index6_decimal(machine, 0, UINT32_C(0x01000000) + row * UINT32_C(0x80) + UINT32_C(52 * 2));
    }
    count = UINT64_C(14) * UINT64_C(6) + UINT64_C(19) * UINT64_C(2) +
        UINT64_C(11) * UINT64_C(2) + UINT64_C(9) * UINT64_C(4) +
        UINT64_C(11) * UINT64_C(24) + UINT64_C(33) * UINT64_C(6);
    if (status == VF2_OK && characters != NULL) {
        *characters = count;
    }
    return status;
}

static vf2_status phase17_index6_finish_game_data_pair(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    uint8_t state
)
{
    const uint8_t spill = UINT8_C(0x56);
    uint64_t characters = 0u;
    uint64_t instructions = 0u;
    uint64_t calls = 0u;
    vf2_status status = VF2_OK;

    if (state == UINT8_C(10)) {
        const uint8_t next = UINT8_C(11);
        status = phase17_index6_render_game_data_static(machine, &characters);
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x005000a5), &next, sizeof(next));
        }
        instructions = UINT64_C(20595);
        calls = UINT64_C(144);
    } else if (state == UINT8_C(11)) {
        uint32_t offset = 0u;
        uint8_t value = 0u;
        for (offset = 0u; status == VF2_OK && offset < UINT32_C(0x200); ++offset) {
            status = vf2_model2a_read(machine, UINT32_C(0x01d00000) + offset, &value, sizeof(value));
            if (status == VF2_OK && value != 0u) {
                return VF2_ERROR_UNSUPPORTED;
            }
        }
        if (status == VF2_OK) {
            status = phase17_index6_render_game_data_zero(machine, &characters);
        }
        instructions = UINT64_C(4576);
        calls = UINT64_C(133);
    } else {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x005ff602), &spill, sizeof(spill));
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions += instructions;
    cpu->procedure_calls += calls;
    cpu->procedure_returns += calls;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[0]=0u; cpu->registers[1]=UINT32_C(0x005ff580);
    cpu->registers[2]=UINT32_C(0x0000a010); cpu->registers[3]=0u;
    cpu->registers[4]=UINT32_C(0x00515400); cpu->registers[5]=UINT32_C(0x3f800000);
    cpu->registers[6]=0u; cpu->registers[7]=0u;
    cpu->registers[8]=UINT32_MAX; cpu->registers[9]=UINT32_MAX;
    cpu->registers[10]=UINT32_MAX; cpu->registers[11]=UINT32_MAX;
    cpu->registers[12]=0u; cpu->registers[13]=0u;
    cpu->registers[14]=UINT32_C(3)+(uint32_t)state;
    cpu->registers[15]=UINT32_C(0x00008a00);
    cpu->registers[16]=state==UINT8_C(10)?UINT32_C(0x2e):0u;
    cpu->registers[17]=0u; cpu->registers[18]=UINT32_C(0xc0a0a3d7);
    cpu->registers[19]=0u; cpu->registers[20]=UINT32_C(0x00560000);
    cpu->registers[21]=UINT32_C(0x0050e850); cpu->registers[22]=UINT32_C(0x000055b6);
    cpu->registers[23]=UINT32_C(0x00510980); cpu->registers[24]=UINT32_C(0x00512980);
    cpu->registers[25]=state==UINT8_C(10)?UINT32_C(0x01001724):UINT32_C(0x010015e8);
    cpu->registers[26]=UINT32_C(0x00800000); cpu->registers[27]=UINT32_C(0x00880000);
    cpu->registers[28]=UINT32_C(0x00004000); cpu->registers[29]=UINT32_C(0x00516480);
    cpu->registers[30]=UINT32_C(0x00000220); cpu->registers[31]=UINT32_C(0x005ff500);
    if (state == UINT8_C(10)) {
        cpu->arithmetic_control=(cpu->arithmetic_control&~UINT32_C(7))|UINT32_C(1);
        cpu->compare_result=VF2_I960_COMPARE_GREATER;
    } else {
        cpu->arithmetic_control&=~UINT32_C(7);
        cpu->compare_result=VF2_I960_COMPARE_NONE;
    }

    report->kind=VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address=VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address=cpu->ip;
    report->iterations=UINT64_C(1);
    report->rows=characters;
    report->recovered_instruction_count=instructions;
    report->recovered_procedure_calls=calls;
    report->recovered_procedure_returns=calls+UINT64_C(1);
    report->cpu_poststate_applied=1;
    return VF2_OK;
}

'''
s = s.replace(marker, helpers + marker, 1)

old = '''        phase_a5 > UINT8_C(9) || phase_a6 != UINT8_C(0xff) ||\n        ((phase_a5 < UINT8_C(9) && phase_a7 != UINT8_C(0xff)) ||\n         (phase_a5 == UINT8_C(9) && phase_a7 > UINT8_C(9)))) {'''
new = '''        phase_a5 > UINT8_C(11) || phase_a6 != UINT8_C(0xff) ||\n        ((phase_a5 < UINT8_C(9) && phase_a7 != UINT8_C(0xff)) ||\n         (phase_a5 == UINT8_C(9) && phase_a7 > UINT8_C(9)) ||\n         (phase_a5 >= UINT8_C(10) && phase_a7 != UINT8_C(0xff)))) {'''
if old not in s:
    raise SystemExit('index6 precondition not found')
s = s.replace(old, new, 1)

needle = '''    /* 0x0005cb70 is the common BOOKKEEPING input tail used by the stable\n'''
insert = '''    if (phase_a5 >= UINT8_C(10)) {\n        if (navigation_flags != 0u) {\n            return VF2_ERROR_UNSUPPORTED;\n        }\n        return phase17_index6_finish_game_data_pair(\n            machine, cpu, report, phase_a5\n        );\n    }\n\n'''
pos = s.index(needle, s.index(marker))
s = s[:pos] + insert + s[pos:]
p.write_text(s)
