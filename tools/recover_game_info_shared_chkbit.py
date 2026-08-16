from pathlib import Path
p=Path('src/recovered/hybrid.c')
s=p.read_text()
s=s.replace('''    uint32_t tail_instruction_delta = 0u;\n    bool preserve_tail_compare = false;\n    bool high_result = false;\n''','''    uint32_t tail_instruction_delta = 0u;\n    bool high_result = false;\n''',1)
anchor='''    if (status == VF2_OK) r10 |= r14 & UINT32_C(0xc0000000);\n'''
insert='''    if (status == VF2_OK) r10 |= r14 & UINT32_C(0xc0000000);\n    if (status == VF2_OK) {\n        /* 0x189a8..0x189bc: the first SHLO result is overwritten by SHRO,\n         * then CHKBIT 10 controls ALTERBIT 3 and remains the condition code\n         * unless the later type-22 helper executes its own CHKBIT. */\n        status = vf2_model2a_read_u32(machine, fighter0, &r4);\n        if (status == VF2_OK) {\n            r3 = (r10 >> 5u) ^ r4;\n            const bool bit10_set =\n                (r3 & (UINT32_C(1) << 10u)) != 0u;\n            hybrid_set_compare_result(\n                cpu, bit10_set ? VF2_I960_COMPARE_EQUAL\n                               : VF2_I960_COMPARE_NONE\n            );\n            r10 = bit10_set\n                ? r10 | (UINT32_C(1) << 3u)\n                : r10 & ~(UINT32_C(1) << 3u);\n        }\n    }\n'''
if anchor not in s: raise SystemExit('shared chkbit anchor missing')
s=s.replace(anchor,insert,1)
s=s.replace('''                        tail_instruction_delta += UINT32_C(1) +\n                            helper_instructions;\n                        preserve_tail_compare = true;\n''','''                        tail_instruction_delta += UINT32_C(1) +\n                            helper_instructions;\n''',1)
s=s.replace('''    if (status == VF2_OK) {\n        if (!preserve_tail_compare) {\n            hybrid_set_compare_result(cpu, VF2_I960_COMPARE_NONE);\n        }\n        cpu->ip = UINT32_C(0x00018a50);\n''','''    if (status == VF2_OK) {\n        cpu->ip = UINT32_C(0x00018a50);\n''',1)
p.write_text(s)
