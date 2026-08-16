from pathlib import Path
p=Path('src/recovered/hybrid.c')
s=p.read_text()
old='''        if (status == VF2_OK) {\n            native_instructions += UINT64_C(6); /* mov/stib, mov/stib, ldob/cmpobe */\n            if (countdown != 0u) {\n                --countdown;\n'''
new='''        if (status == VF2_OK) {\n            native_instructions += UINT64_C(6); /* mov/stib, mov/stib, ldob/cmpobe */\n            /* 0x164f0 CMPobe 0,r3 leaves its comparison observable at the\n             * task RET: zero is EQUAL; every nonzero uint8 countdown is LESS\n             * when comparing literal 0 against r3. */\n            hybrid_set_compare_result(\n                cpu, countdown == 0u ? VF2_I960_COMPARE_EQUAL\n                                     : VF2_I960_COMPARE_LESS\n            );\n            if (countdown != 0u) {\n                --countdown;\n'''
if old not in s: raise SystemExit('countdown anchor missing')
p.write_text(s.replace(old,new,1))
