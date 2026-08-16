from pathlib import Path

p = Path('src/recovered/texture_bridge_match.c')
s = p.read_text()

marker = 'static vf2_status phase17_index6_render_page5(\n'
helpers = r'''static int32_t phase17_index6_round_even(float value)
{
    const double input = (double)value;
    const int64_t truncated = (int64_t)input;
    const double fraction = input - (double)truncated;
    int64_t rounded = truncated;

    if (fraction > 0.5 ||
        (fraction == 0.5 && (truncated & INT64_C(1)) != 0)) {
        ++rounded;
    } else if (fraction < -0.5 ||
               (fraction == -0.5 && (truncated & INT64_C(1)) != 0)) {
        --rounded;
    }
    return (int32_t)rounded;
}

static vf2_status phase17_index6_decimal(
    vf2_model2a *machine, int32_t value, uint32_t destination
)
{
    uint32_t magnitude = value < 0
        ? (uint32_t)(-(int64_t)value) : (uint32_t)value;
    uint16_t tiles[6];
    uint32_t index = 0u;
    vf2_status status = VF2_OK;

    tiles[0] = value < 0 ? UINT16_C(0x802d) : UINT16_C(0x8020);
    if (magnitude <= UINT32_C(8191)) {
        tiles[1] = UINT16_C(0x8020);
        for (index = 0u; index < 4u; ++index) {
            status = read_u16(
                machine,
                UINT32_C(0x02040000) + magnitude * UINT32_C(8) +
                    index * UINT32_C(2),
                &tiles[index + 2u]
            );
            if (status != VF2_OK) {
                return status;
            }
        }
    } else {
        uint32_t remaining = magnitude;
        for (index = 0u; index < 5u; ++index) {
            tiles[index + 1u] = UINT16_C(0x8020);
        }
        index = 5u;
        do {
            --index;
            tiles[index + 1u] = (uint16_t)(
                UINT16_C(0x8030) + (uint16_t)(remaining % UINT32_C(10))
            );
            remaining /= UINT32_C(10);
        } while (remaining != 0u && index != 0u);
    }
    for (index = 0u; status == VF2_OK && index < 6u; ++index) {
        status = write_u16(
            machine, destination + index * UINT32_C(2), tiles[index]
        );
    }
    return status;
}

static uint64_t phase17_index6_decimal_instruction_delta(int32_t value)
{
    uint32_t magnitude = value < 0
        ? (uint32_t)(-(int64_t)value) : (uint32_t)value;
    uint32_t digits = 1u;
    uint64_t instructions = UINT64_C(20);

    if (magnitude <= UINT32_C(8191)) {
        return UINT64_C(0);
    }
    while (magnitude >= UINT32_C(10) && digits < UINT32_C(5)) {
        magnitude /= UINT32_C(10);
        ++digits;
    }
    instructions = digits < UINT32_C(5)
        ? UINT64_C(34) + UINT64_C(6) * (uint64_t)digits
        : UINT64_C(63);
    if (value < 0) {
        instructions += UINT64_C(2);
    }
    return instructions - UINT64_C(20);
}

static vf2_status phase17_index6_rate_bar(
    vf2_model2a *machine,
    int32_t wins,
    int32_t losses,
    uint32_t destination,
    uint64_t *instruction_delta,
    uint64_t *call_delta
)
{
    const uint32_t width = UINT32_C(20);
    const int32_t total = wins + losses;
    uint32_t filled = 0u;
    uint32_t full = 0u;
    uint32_t remainder = 0u;
    uint32_t index = 0u;
    uint32_t cursor = destination;
    vf2_status status = VF2_OK;

    if (total == 0) {
        status = write_u16(machine, cursor, UINT16_C(0x802d));
        cursor += UINT32_C(2);
        for (index = 1u; status == VF2_OK && index < width; ++index) {
            status = write_u16(machine, cursor, UINT16_C(0x8020));
            cursor += UINT32_C(2);
        }
        return status;
    }
    if (wins < 0 || losses < 0 || total < 0) {
        return VF2_ERROR_UNSUPPORTED;
    }

    {
        const float ratio = (float)wins / (float)total;
        int32_t scaled = phase17_index6_round_even(
            ratio * (float)(width * UINT32_C(8))
        );
        if (scaled < 0 || scaled > (int32_t)(width * UINT32_C(8))) {
            return VF2_ERROR_UNSUPPORTED;
        }
        full = (uint32_t)scaled >> 3u;
        remainder = (uint32_t)scaled & UINT32_C(7);
    }

    for (index = 0u; status == VF2_OK && index < full; ++index) {
        status = write_u16(machine, cursor, UINT16_C(0x8007));
        cursor += UINT32_C(2);
    }
    if (status == VF2_OK && remainder != 0u) {
        status = write_u16(
            machine, cursor,
            (uint16_t)(UINT16_C(0x8007) + (uint16_t)remainder)
        );
        cursor += UINT32_C(2);
    }
    filled = full + (remainder != 0u ? UINT32_C(1) : UINT32_C(0));
    for (index = filled; status == VF2_OK && index < width; ++index) {
        status = write_u16(machine, cursor, UINT16_C(0x8020));
        cursor += UINT32_C(2);
    }

    if (status == VF2_OK && instruction_delta != NULL) {
        /* Relative to the all-zero (-1.0) helper path measured in the ROM.
         * The caller's nonzero-sum branch costs three extra instructions.
         * Each emitted 0x8440 tile adds seven net instructions; a fractional
         * tile saves two, while a completely full 20-tile bar saves one. */
        uint64_t delta = remainder == 0u
            ? UINT64_C(17) + UINT64_C(7) * (uint64_t)filled
            : UINT64_C(15) + UINT64_C(7) * (uint64_t)filled;
        if (filled == width && remainder == 0u) {
            --delta;
        }
        *instruction_delta += delta;
    }
    if (status == VF2_OK && call_delta != NULL) {
        *call_delta += (uint64_t)filled;
    }
    return status;
}

'''
if marker not in s:
    raise SystemExit('page5 marker not found')
s = s.replace(marker, helpers + marker, 1)

old_sig = '''static vf2_status phase17_index6_render_page5(\n    vf2_model2a *machine,\n    uint8_t state,\n    uint8_t fighter,\n    uint64_t *characters\n)'''
new_sig = '''static vf2_status phase17_index6_render_page5(\n    vf2_model2a *machine,\n    uint8_t state,\n    uint8_t fighter,\n    uint64_t *characters,\n    uint64_t *instruction_delta,\n    uint64_t *call_delta\n)'''
if old_sig not in s:
    raise SystemExit('page5 signature not found')
s = s.replace(old_sig, new_sig, 1)

old_names = '    static const char *names[10] = {"AKIRA","JACKY","SARAH","KAGE","LAU","JEFFRY","PAI","WOLF","SHUN","LION"};'
new_names = '    static const char *names[10] = {"AKIRA ","JACKY ","SARAH ","KAGE  ","LAU   ","JEFFRY","PAI   ","WOLF  ","SHUN  ","LION  "};'
if old_names not in s:
    raise SystemExit('names array not found')
s = s.replace(old_names, new_names, 1)

old_state9 = '''    if(state==UINT8_C(9)){\n        status=write_phase17_index0_text(machine,UINT32_C(10*0x80),30u,names[fighter]);count+=(uint64_t)strlen(names[fighter]);\n        for(i=0u;status==VF2_OK&&i<10u;++i){status=write_phase17_index0_text(machine,(uint32_t)rows[i]*UINT32_C(0x80),28u,"0");++count;if(status==VF2_OK){status=write_phase17_index0_text(machine,(uint32_t)rows[i]*UINT32_C(0x80),36u,"0");++count;}if(status==VF2_OK){status=write_phase17_index0_text(machine,(uint32_t)rows[i]*UINT32_C(0x80),38u,"-");++count;}}\n    }'''
new_state9 = '''    if(state==UINT8_C(9)){\n        static const uint8_t fighter_map[10]={0u,1u,2u,3u,4u,5u,6u,7u,8u,10u};\n        const uint32_t record=UINT32_C(0x01d036a4)+(uint32_t)fighter_map[fighter]*UINT32_C(48);\n        status=write_phase17_index0_text(machine,UINT32_C(10*0x80),30u,names[fighter]);\n        count+=UINT64_C(6);\n        for(i=0u;status==VF2_OK&&i<10u;++i){\n            const uint32_t pair_offset=i<9u?(uint32_t)i*UINT32_C(4):UINT32_C(40);\n            uint16_t raw_wins=0u,raw_losses=0u;\n            int32_t wins=0,losses=0;\n            const uint32_t line=(uint32_t)rows[i];\n            status=read_u16(machine,record+pair_offset,&raw_wins);\n            if(status==VF2_OK)status=read_u16(machine,record+pair_offset+UINT32_C(2),&raw_losses);\n            wins=(int32_t)(int16_t)raw_wins;\n            losses=(int32_t)(int16_t)raw_losses;\n            if(status==VF2_OK)status=phase17_index6_decimal(machine,wins,UINT32_C(0x01000000)+line*UINT32_C(0x80)+UINT32_C(23*2));\n            if(status==VF2_OK)status=phase17_index6_decimal(machine,losses,UINT32_C(0x01000000)+line*UINT32_C(0x80)+UINT32_C(31*2));\n            if(status==VF2_OK&&instruction_delta!=NULL){\n                *instruction_delta+=phase17_index6_decimal_instruction_delta(wins);\n                *instruction_delta+=phase17_index6_decimal_instruction_delta(losses);\n            }\n            if(status==VF2_OK)status=phase17_index6_rate_bar(machine,wins,losses,UINT32_C(0x01000000)+line*UINT32_C(0x80)+UINT32_C(38*2),instruction_delta,call_delta);\n            count+=UINT64_C(32);\n        }\n    }'''
if old_state9 not in s:
    raise SystemExit('state9 placeholder block not found')
s = s.replace(old_state9, new_state9, 1)

old_decls = '''    int exit_requested = 0;\n    uint64_t instructions = 0u;\n    uint64_t calls = 0u;\n    uint64_t characters = 0u;'''
new_decls = '''    int exit_requested = 0;\n    uint64_t instructions = 0u;\n    uint64_t calls = 0u;\n    uint64_t characters = 0u;\n    uint64_t render_instruction_delta = 0u;\n    uint64_t render_call_delta = 0u;'''
if old_decls not in s:
    raise SystemExit('index6 declarations not found')
s = s.replace(old_decls, new_decls, 1)

old_nav = '''    if ((phase_a5 & UINT8_C(1)) != 0u &&\n        (navigation_flags & UINT32_C(0x04000014)) != 0u) {\n        /* 0x5cb80 checks the shared TEST/exit mask before page navigation. */\n        exit_requested = 1;\n    } else if ((phase_a5 & UINT8_C(1)) != 0u && navigation_flags == UINT32_C(0x100)) {\n        page_navigation_delta = 1;\n    } else if ((phase_a5 & UINT8_C(1)) != 0u &&\n               navigation_flags == UINT32_C(0x200)) {\n        page_navigation_delta = -1;\n    } else if (phase_a5 == UINT8_C(9) && navigation_flags == UINT32_C(0x1000)) {\n        fighter_delta = 1;\n    } else if (phase_a5 == UINT8_C(9) && navigation_flags == UINT32_C(0x2000)) {\n        fighter_delta = -1;\n    } else if (navigation_flags != 0u) {\n        return VF2_ERROR_UNSUPPORTED;\n    }'''
new_nav = '''    if ((phase_a5 & UINT8_C(1)) != 0u &&\n        (navigation_flags & UINT32_C(0x04000014)) != 0u) {\n        /* 0x5cb80 checks the shared TEST/exit mask before page navigation. */\n        exit_requested = 1;\n    } else if (phase_a5 == UINT8_C(9)) {\n        if ((navigation_flags & UINT32_C(0x08001008)) != 0u) {\n            fighter_delta = 1;\n        } else if ((navigation_flags & UINT32_C(0x2000)) != 0u) {\n            fighter_delta = -1;\n        }\n        if ((navigation_flags & UINT32_C(0x08000108)) != 0u) {\n            page_navigation_delta = 1;\n        } else if ((navigation_flags & UINT32_C(0x200)) != 0u) {\n            page_navigation_delta = -1;\n        }\n        if (navigation_flags != 0u && fighter_delta == 0 &&\n            page_navigation_delta == 0) {\n            return VF2_ERROR_UNSUPPORTED;\n        }\n    } else if ((phase_a5 & UINT8_C(1)) != 0u &&\n               navigation_flags == UINT32_C(0x100)) {\n        page_navigation_delta = 1;\n    } else if ((phase_a5 & UINT8_C(1)) != 0u &&\n               navigation_flags == UINT32_C(0x200)) {\n        page_navigation_delta = -1;\n    } else if (navigation_flags != 0u) {\n        return VF2_ERROR_UNSUPPORTED;\n    }'''
if old_nav not in s:
    raise SystemExit('index6 navigation block not found')
s = s.replace(old_nav, new_nav, 1)

old_call = '''                    : phase17_index6_render_page5(machine, phase_a5,\n                        phase_a5 == UINT8_C(8) ? UINT8_C(0) : phase_a7,\n                        &characters))));'''
new_call = '''                    : phase17_index6_render_page5(machine, phase_a5,\n                        phase_a5 == UINT8_C(8) ? UINT8_C(0) : phase_a7,\n                        &characters, &render_instruction_delta,\n                        &render_call_delta))));'''
if old_call not in s:
    raise SystemExit('page5 call not found')
s = s.replace(old_call, new_call, 1)

old_baseline = '''    } else {\n        instructions = UINT64_C(1904); calls = UINT64_C(34);\n    }\n\n    if (status == VF2_OK && page_navigation_delta != 0) {'''
new_baseline = '''    } else {\n        instructions = UINT64_C(1904) + render_instruction_delta;\n        calls = UINT64_C(34) + render_call_delta;\n    }\n\n    if (status == VF2_OK && page_navigation_delta != 0) {'''
if old_baseline not in s:
    raise SystemExit('state9 accounting baseline not found')
s = s.replace(old_baseline, new_baseline, 1)

old_else_if = '''    } else if (status == VF2_OK && fighter_delta != 0) {\n        uint8_t next_fighter = phase_a7;'''
new_else_if = '''    }\n    if (status == VF2_OK && fighter_delta != 0) {\n        uint8_t next_fighter = phase_a7;'''
if old_else_if not in s:
    raise SystemExit('fighter update else-if not found')
s = s.replace(old_else_if, new_else_if, 1)

old_post = '''    cpu->registers[14] = UINT32_C(3) + (uint32_t)phase_a5;\n    cpu->registers[15] = UINT32_C(0x00008a00);\n    cpu->registers[16] = fighter_delta != 0\n        ? (fighter_delta < 0 ? UINT32_MAX : UINT32_C(1))\n        : ((phase_a5 & UINT8_C(1)) == 0u\n            ? UINT32_C(0x2e)\n            : (phase_a5 == UINT8_C(1) ? UINT32_C(0x00532d2d)\n                : (phase_a5 == UINT8_C(5) ? UINT32_C(0x00002d2d) : 0u)));\n    cpu->registers[17] = phase_a5 == UINT8_C(7) ? UINT32_C(0x01d0361c) : 0u;\n    cpu->registers[18] = UINT32_C(0xc0a0a3d7);'''
new_post = '''    cpu->registers[14] = phase_a5 == UINT8_C(9)\n        ? UINT32_C(0x00009f9c)\n        : UINT32_C(3) + (uint32_t)phase_a5;\n    cpu->registers[15] = UINT32_C(0x00008a00);\n    cpu->registers[16] = fighter_delta != 0\n        ? (fighter_delta < 0 ? UINT32_MAX : UINT32_C(1))\n        : ((phase_a5 & UINT8_C(1)) == 0u\n            ? UINT32_C(0x2e)\n            : (phase_a5 == UINT8_C(1) ? UINT32_C(0x00532d2d)\n                : (phase_a5 == UINT8_C(5) ? UINT32_C(0x00002d2d) : 0u)));\n    cpu->registers[17] = phase_a5 == UINT8_C(7) ? UINT32_C(0x01d0361c) : 0u;\n    cpu->registers[18] = phase_a5 == UINT8_C(9)\n        ? 0u : UINT32_C(0xc0a0a3d7);'''
if old_post not in s:
    raise SystemExit('poststate core block not found')
s = s.replace(old_post, new_post, 1)

old_g6 = '    cpu->registers[22] = UINT32_C(0x000055b6);'
new_g6 = '    cpu->registers[22] = phase_a5 == UINT8_C(9) ? UINT32_C(0x10) : UINT32_C(0x000055b6);'
# Replace only the first occurrence after index6 function by locating function start.
func = s.index('static vf2_status execute_frame_phase17_bit7_index6(')
pos = s.index(old_g6, func)
s = s[:pos] + new_g6 + s[pos + len(old_g6):]

old_cond = '''    if ((phase_a5 & UINT8_C(1)) == 0u) {\n        cpu->arithmetic_control = (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);\n        cpu->compare_result = VF2_I960_COMPARE_GREATER;\n    } else {\n        cpu->arithmetic_control &= ~UINT32_C(7);\n        cpu->compare_result = VF2_I960_COMPARE_NONE;\n    }'''
new_cond = '''    if (phase_a5 == UINT8_C(9) && page_navigation_delta != 0) {\n        cpu->arithmetic_control =\n            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(4);\n        cpu->compare_result = VF2_I960_COMPARE_LESS;\n    } else if ((phase_a5 & UINT8_C(1)) == 0u) {\n        cpu->arithmetic_control =\n            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);\n        cpu->compare_result = VF2_I960_COMPARE_GREATER;\n    } else {\n        cpu->arithmetic_control &= ~UINT32_C(7);\n        cpu->compare_result = VF2_I960_COMPARE_NONE;\n    }'''
func = s.index('static vf2_status execute_frame_phase17_bit7_index6(')
pos = s.index(old_cond, func)
s = s[:pos] + new_cond + s[pos + len(old_cond):]

p.write_text(s)
