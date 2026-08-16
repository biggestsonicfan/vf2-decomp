from pathlib import Path
p=Path('src/recovered/hybrid.c')
s=p.read_text()
anchor='''    if (status == VF2_OK) r10 |= r14 & UINT32_C(0xc0000000);\n    if (status == VF2_OK) {\n        status = vf2_model2a_read_u32(\n            machine, fighter0 + UINT32_C(0x000005b8), &r13\n        );\n    }\n'''
insert='''    if (status == VF2_OK) r10 |= r14 & UINT32_C(0xc0000000);\n    /* Preserve the still-unrecovered 0x18788 signed-distance branch domain.\n     * This guard was present before the high-flag recovery and is independent\n     * of the newly recovered 0x18978..0x189a4 accumulation tail. */\n    if (status == VF2_OK) {\n        status = hybrid_read_u16(\n            machine, fighter0 + UINT32_C(0x000005b4), &short_value\n        );\n        r3 = (uint32_t)(int32_t)(int16_t)short_value - r11;\n        r15 = r3 << 16u;\n        r3 = r15 >> 16u;\n        r4 = UINT32_C(0x00001554);\n        r15 = 0u - r4;\n        if ((int32_t)r3 <= (int32_t)r15) {\n            status = VF2_ERROR_UNSUPPORTED;\n        }\n    }\n    if (status == VF2_OK) {\n        status = vf2_model2a_read_u32(\n            machine, fighter0 + UINT32_C(0x000005b8), &r13\n        );\n    }\n'''
if anchor not in s: raise SystemExit('anchor not found')
p.write_text(s.replace(anchor,insert,1))
