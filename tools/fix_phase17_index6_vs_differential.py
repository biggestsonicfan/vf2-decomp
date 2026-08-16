from pathlib import Path

p = Path('src/recovered/texture_bridge_match.c')
s = p.read_text()
start = s.index('static vf2_status phase17_index6_render_page5(')
end = s.index('static vf2_status execute_frame_phase17_bit7_index6(', start)
page = s[start:end]

old = '    vf2_status status=clear_tile_plane_64x48(machine,UINT32_C(0x01000000));\n    if(fighter>UINT8_C(9))return VF2_ERROR_UNSUPPORTED;\n    for(i=0u;status==VF2_OK&&i<sizeof(fixed)/sizeof(fixed[0]);++i){status=write_phase17_index0_text(machine,(uint32_t)fixed[i].row*UINT32_C(0x80),fixed[i].col,fixed[i].text);count+=(uint64_t)strlen(fixed[i].text);}\n'
new = '    vf2_status status=VF2_OK;\n    if(fighter>UINT8_C(9))return VF2_ERROR_UNSUPPORTED;\n    if(state!=UINT8_C(9))status=clear_tile_plane_64x48(machine,UINT32_C(0x01000000));\n    if(state!=UINT8_C(9))for(i=0u;status==VF2_OK&&i<sizeof(fixed)/sizeof(fixed[0]);++i){status=write_phase17_index0_text(machine,(uint32_t)fixed[i].row*UINT32_C(0x80),fixed[i].col,fixed[i].text);count+=(uint64_t)strlen(fixed[i].text);}\n'
if old not in page:
    raise SystemExit('page5 initialization block not found')
page = page.replace(old, new, 1)

style_marker = '    for(row=0u;status==VF2_OK&&row<UINT32_C(48);++row)for(col=0u;status==VF2_OK&&col<UINT32_C(64);++col)'
if style_marker not in page:
    raise SystemExit('page5 style marker not found')
page = page.replace(
    style_marker,
    '    if(state==UINT8_C(9)){if(status==VF2_OK&&characters!=NULL)*characters=count;return status;}\n' + style_marker,
    1
)
s = s[:start] + page + s[end:]

func_start = s.index('static vf2_status execute_frame_phase17_bit7_index6(')
func_end = s.index('static vf2_status phase17_index7_render_choices(', func_start)
func = s[func_start:func_end]

func = func.replace(
    '            UINT64_C(16215), UINT64_C(16173)\n',
    '            UINT64_C(16215), UINT64_C(16169)\n',
    1
)
func = func.replace(
    '        instructions = UINT64_C(1904) + render_instruction_delta;\n',
    '        instructions = UINT64_C(1900) + render_instruction_delta;\n',
    1
)

needle = '    cpu->registers[15] = UINT32_C(0x00008a00);'
pos = func.rfind(needle)
if pos < 0:
    raise SystemExit('normal r15 poststate not found')
replacement = ('    cpu->registers[15] = phase_a5 == UINT8_C(9)\n'
               '        ? UINT32_C(0x00008800) : UINT32_C(0x00008a00);')
func = func[:pos] + replacement + func[pos + len(needle):]

needle = '    cpu->registers[22] = UINT32_C(0x000055b6);'
pos = func.rfind(needle)
if pos < 0:
    # The first staging patch may already have made an earlier occurrence conditional.
    needle = '    cpu->registers[22] = phase_a5 == UINT8_C(9) ? UINT32_C(0x10) : UINT32_C(0x000055b6);'
    pos = func.rfind(needle)
if pos < 0:
    raise SystemExit('normal g6 poststate not found')
replacement = ('    cpu->registers[22] = phase_a5 == UINT8_C(9)\n'
               '        ? UINT32_C(0x10) : UINT32_C(0x000055b6);')
func = func[:pos] + replacement + func[pos + len(needle):]

s = s[:func_start] + func + s[func_end:]
p.write_text(s)
