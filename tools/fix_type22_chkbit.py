from pathlib import Path
p=Path('src/recovered/hybrid.c')
s=p.read_text()
old='''    if (status == VF2_OK) {\n        fighter0_flags = (fighter1_flags & (UINT32_C(1) << 10u)) != 0u\n            ? fighter0_flags | (UINT32_C(1) << 6u)\n            : fighter0_flags & ~(UINT32_C(1) << 6u);\n        status = vf2_model2a_write_u32(machine, fighter0, fighter0_flags);\n    }\n'''
new='''    if (status == VF2_OK) {\n        const bool bit10_set =\n            (fighter1_flags & (UINT32_C(1) << 10u)) != 0u;\n        /* 0x18c54 CHKBIT 10 supplies the condition consumed by the\n         * following ALTERBIT 6 and remains the helper's final condition. */\n        hybrid_set_compare_result(\n            cpu, bit10_set ? VF2_I960_COMPARE_EQUAL : VF2_I960_COMPARE_NONE\n        );\n        fighter0_flags = bit10_set\n            ? fighter0_flags | (UINT32_C(1) << 6u)\n            : fighter0_flags & ~(UINT32_C(1) << 6u);\n        status = vf2_model2a_write_u32(machine, fighter0, fighter0_flags);\n    }\n'''
if old not in s: raise SystemExit('chkbit anchor missing')
p.write_text(s.replace(old,new,1))
