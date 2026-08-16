from pathlib import Path

path = Path("src/recovered/texture_bridge_video.c")
text = path.read_text()
anchor = """        instructions += UINT64_C(2);
    } else {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_write_u32(
        machine, UINT32_C(0x005001dc), callback
    );
"""
replacement = """        instructions += UINT64_C(2);
    } else if (callback == UINT32_C(0x00001284) &&
               (newly_enabled & UINT32_C(0x00f7f700)) > UINT32_C(0x000001fe)) {
        /* Measured active-input path through 0x00001200: r9 is the masked
         * newly-enabled control word and the installed table contributes only
         * one byte shifted left by one. A value above 0x1fe therefore cannot
         * match; the ROM takes cmpobne and reinstalls the 0x1284 fallback.
         * Keep smaller values and other callback tables explicit unsupported
         * until they are measured. */
        callback = UINT32_C(0x00001284);
        instructions += UINT64_C(6);
    } else {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_write_u32(
        machine, UINT32_C(0x005001dc), callback
    );
"""
count = text.count(anchor)
if count != 1:
    raise SystemExit(f"expected one target anchor, found {count}")
path.write_text(text.replace(anchor, replacement, 1))
