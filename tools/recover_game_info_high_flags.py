from pathlib import Path
p=Path('src/recovered/hybrid.c')
s=p.read_text()
s=s.replace('    uint32_t body_instructions = 101u;\n    bool high_result = false;','    uint32_t body_instructions = 101u;\n    uint32_t tail_instruction_delta = 0u;\n    bool high_result = false;',1)
a=s.index('        r3 = (UINT32_C(1) << 26u) & r14;')
start=s.rfind('    if (status == VF2_OK) {',0,a)
b=s.index('    if (status == VF2_OK) {\n        status = vf2_model2a_read_u32(\n            machine, fighter0 + UINT32_C(0x000005b8)',a)
new='''    if (status == VF2_OK) {\n        uint16_t progress = 0u;\n        uint16_t limit = 0u;\n        status = vf2_model2a_read_u32(machine, fighter1 + UINT32_C(0x00000844), &r14);\n        r3 = r14 & UINT32_C(0x3c000000);\n        if (status == VF2_OK && r3 != 0u) {\n            status = hybrid_read_u16(machine, fighter1 + UINT32_C(0x0000084e), &progress);\n            if (status == VF2_OK) status = hybrid_read_u16(machine, fighter1 + UINT32_C(0x000001aa), &limit);\n            if (status == VF2_OK) {\n                tail_instruction_delta += UINT32_C(3);\n                if (progress >= limit) { r10 |= r3; ++tail_instruction_delta; }\n            }\n        }\n    }\n    if (status == VF2_OK) r10 |= r14 & UINT32_C(0xc0000000);\n'''
s=s[:start]+new+s[b:]
s=s.replace('        cpu->executed_instructions += body_instructions;','        cpu->executed_instructions += body_instructions + tail_instruction_delta;',1)
p.write_text(s)
