from pathlib import Path

p = Path('src/recovered/texture_bridge_match.c')
s = p.read_text()
start = s.index('static vf2_status phase17_index6_rate_bar(')
func_start = s.index('static vf2_status execute_frame_phase17_bit7_index6(')
func_end = s.index('static vf2_status phase17_index7_render_choices(', func_start)

# The synthetic measurements were made with the state-9 +fighter input still
# asserted. Relative to the true state-9 idle path the bar deltas are four
# instructions smaller than the first staging formula.
segment = s[start:func_start]
old = '''        uint64_t delta = remainder == 0u\n            ? UINT64_C(17) + UINT64_C(7) * (uint64_t)filled\n            : UINT64_C(15) + UINT64_C(7) * (uint64_t)filled;'''
new = '''        uint64_t delta = remainder == 0u\n            ? UINT64_C(13) + UINT64_C(7) * (uint64_t)filled\n            : UINT64_C(11) + UINT64_C(7) * (uint64_t)filled;'''
if old not in segment:
    raise SystemExit('rate delta formula not found')
segment = segment.replace(old, new, 1)
segment = segment.replace(
    "         * The caller's nonzero-sum branch costs three extra instructions.\n",
    "         * This delta is relative to the measured zero-sum state-9 path.\n",
    1
)
s = s[:start] + segment + s[func_start:]
func_start = s.index('static vf2_status execute_frame_phase17_bit7_index6(')
func_end = s.index('static vf2_status phase17_index7_render_choices(', func_start)
func = s[func_start:func_end]
func = func.replace(
    '            UINT64_C(16215), UINT64_C(16169)\n',
    '            UINT64_C(16215), UINT64_C(16173)\n',
    1
)
func = func.replace(
    '        instructions = UINT64_C(1900) + render_instruction_delta;\n',
    '        instructions = UINT64_C(1904) + render_instruction_delta;\n',
    1
)

# Shared TEST teardown preserves several state-9 values rather than the
# generic BOOKKEEPING page poststate used by states 1/3/5/7.
needle = '        cpu->registers[31] = UINT32_C(0x005ff500);\n        cpu->arithmetic_control =\n'
pos = func.find(needle)
if pos < 0:
    raise SystemExit('exit poststate insertion point not found')
insert = '''        cpu->registers[31] = UINT32_C(0x005ff500);\n        if (phase_a5 == UINT8_C(9)) {\n            cpu->registers[14] = UINT32_C(0x00009f9c);\n            cpu->registers[15] = UINT32_C(0x00008800);\n            cpu->registers[18] = 0u;\n            cpu->registers[22] = UINT32_C(0x10);\n        }\n        cpu->arithmetic_control =\n'''
func = func[:pos] + func[pos:].replace(needle, insert, 1)
s = s[:func_start] + func + s[func_end:]
p.write_text(s)

note = Path('decomp/i960/notes/selector17_index6_bookkeeping_vs_diagram.md')
if note.exists():
    n = note.read_text()
    n = n.replace(
        'Synthetic nonzero records were also measured to recover dynamic bar\naccounting. Changing only the first pair gave:',
        'Synthetic nonzero records were also measured to recover dynamic bar\naccounting. These fixtures retained the canonical fighter-advance input\n`0x1000`; changing only the first pair gave:'
    )
    n = n.replace(
        'These establish that each emitted full/partial bar tile adds one\nnested `0x8440` call and seven net instructions relative to the\nblank path, with the fractional/full-width edge adjustments encoded\nin the recovered helper.',
        'Subtracting the measured four-instruction fighter-advance path from\nthese fixtures establishes the state-9 bar delta: `13 + 7*T` for an\nexact tile boundary, `11 + 7*T` when a partial tile is emitted, and one\nless instruction for a completely full 20-tile bar. Each emitted\nfull/partial tile adds one nested `0x8440` call.'
    )
    note.write_text(n)
