from pathlib import Path
p=Path('src/recovered/hybrid.c')
s=p.read_text()
s=s.replace('''    uint32_t tail_instruction_delta = 0u;\n    bool high_result = false;\n''','''    uint32_t tail_instruction_delta = 0u;\n    bool preserve_tail_compare = false;\n    bool high_result = false;\n''',1)
s=s.replace('''                    if (status == VF2_OK) {\n                        /* Account for the 0x18a4c CALL itself; the helper\n                         * count includes 0x18bd4, 0x1ab34 and 0x18b58. */\n                        tail_instruction_delta += UINT32_C(1) +\n                            helper_instructions;\n                    }\n''','''                    if (status == VF2_OK) {\n                        /* Account for the 0x18a4c CALL itself; the helper\n                         * count includes 0x18bd4, 0x1ab34 and 0x18b58. */\n                        tail_instruction_delta += UINT32_C(1) +\n                            helper_instructions;\n                        preserve_tail_compare = true;\n                    }\n''',1)
old='''    if (status == VF2_OK) {\n        hybrid_set_compare_result(cpu, VF2_I960_COMPARE_NONE);\n        cpu->ip = UINT32_C(0x00018a50);\n'''
new='''    if (status == VF2_OK) {\n        if (!preserve_tail_compare) {\n            hybrid_set_compare_result(cpu, VF2_I960_COMPARE_NONE);\n        }\n        cpu->ip = UINT32_C(0x00018a50);\n'''
if old not in s: raise SystemExit('tail compare anchor missing')
s=s.replace(old,new,1)
p.write_text(s)
