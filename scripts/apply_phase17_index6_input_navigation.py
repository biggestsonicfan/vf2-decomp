from pathlib import Path

source = Path("src/recovered/texture_bridge_match.c")
text = source.read_text()

old = """    uint64_t instructions = 0u;\n    uint64_t calls = 0u;\n    uint64_t characters = 0u;\n    vf2_status status = VF2_OK;\n"""
new = """    int page_navigation_delta = 0;\n    int fighter_delta = 0;\n    uint64_t instructions = 0u;\n    uint64_t calls = 0u;\n    uint64_t characters = 0u;\n    vf2_status status = VF2_OK;\n"""
if old not in text:
    raise SystemExit("index6 locals anchor not found")
text = text.replace(old, new, 1)

old = """    if (status != VF2_OK || indirect_target != UINT32_C(0x0005c9b8) ||\n        input_flags != base_input || navigation_flags != 0u || released_flags != 0u ||\n        previous_flags != base_input || selector_mask != UINT32_C(0x00020000) ||\n        phase_a5 > UINT8_C(9) || phase_a6 != UINT8_C(0xff) ||\n        ((phase_a5 < UINT8_C(9) && phase_a7 != UINT8_C(0xff)) ||\n         (phase_a5 == UINT8_C(9) && phase_a7 > UINT8_C(9)))) {\n        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;\n    }\n\n    status = phase_a5 <= UINT8_C(1)\n"""
new = """    if (status != VF2_OK || indirect_target != UINT32_C(0x0005c9b8) ||\n        input_flags != base_input || released_flags != 0u ||\n        previous_flags != base_input || selector_mask != UINT32_C(0x00020000) ||\n        phase_a5 > UINT8_C(9) || phase_a6 != UINT8_C(0xff) ||\n        ((phase_a5 < UINT8_C(9) && phase_a7 != UINT8_C(0xff)) ||\n         (phase_a5 == UINT8_C(9) && phase_a7 > UINT8_C(9)))) {\n        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;\n    }\n\n    /* 0x0005cb70 is the common BOOKKEEPING input tail used by the stable\n     * odd states. Canonical 0x100/0x200 inputs move between pages; the ROM\n     * deliberately targets the even construction state of the destination\n     * page so that its static layout is rebuilt on the following frame.\n     * Page 5 additionally calls 0x00060b50 before that tail, using canonical\n     * 0x1000/0x2000 to rotate the selected fighter in a7 over 0..9. */\n    if ((phase_a5 & UINT8_C(1)) != 0u && navigation_flags == UINT32_C(0x100)) {\n        page_navigation_delta = 1;\n    } else if ((phase_a5 & UINT8_C(1)) != 0u &&\n               navigation_flags == UINT32_C(0x200)) {\n        page_navigation_delta = -1;\n    } else if (phase_a5 == UINT8_C(9) && navigation_flags == UINT32_C(0x1000)) {\n        fighter_delta = 1;\n    } else if (phase_a5 == UINT8_C(9) && navigation_flags == UINT32_C(0x2000)) {\n        fighter_delta = -1;\n    } else if (navigation_flags != 0u) {\n        return VF2_ERROR_UNSUPPORTED;\n    }\n\n    status = phase_a5 <= UINT8_C(1)\n"""
if old not in text:
    raise SystemExit("index6 validation anchor not found")
text = text.replace(old, new, 1)

old = """    } else {\n        instructions = UINT64_C(1904); calls = UINT64_C(34);\n    }\n    if (status == VF2_OK) {\n        status = vf2_model2a_write(machine, UINT32_C(0x005ff602), &spill, sizeof(spill));\n    }\n"""
new = """    } else {\n        instructions = UINT64_C(1904); calls = UINT64_C(34);\n    }\n\n    if (status == VF2_OK && page_navigation_delta != 0) {\n        uint8_t next_page_state = 0u;\n        if (page_navigation_delta > 0) {\n            next_page_state = phase_a5 == UINT8_C(9)\n                ? UINT8_C(0)\n                : (uint8_t)(phase_a5 + UINT8_C(1));\n            /* Relative to the idle common tail, the normal + path executes\n             * four extra instructions; 9 -> 0 executes the wrap clear too. */\n            instructions += phase_a5 == UINT8_C(9)\n                ? UINT64_C(5) : UINT64_C(4);\n        } else {\n            next_page_state = phase_a5 == UINT8_C(1)\n                ? UINT8_C(8)\n                : (uint8_t)(phase_a5 - UINT8_C(3));\n            /* The reverse path subtracts three because stable states are odd\n             * and destination construction states are even. 1 -> 8 wraps. */\n            instructions += phase_a5 == UINT8_C(1)\n                ? UINT64_C(4) : UINT64_C(3);\n        }\n        status = vf2_model2a_write(\n            machine, UINT32_C(0x005000a5),\n            &next_page_state, sizeof(next_page_state)\n        );\n    } else if (status == VF2_OK && fighter_delta != 0) {\n        uint8_t next_fighter = phase_a7;\n        if (fighter_delta > 0) {\n            next_fighter = phase_a7 == UINT8_C(9)\n                ? UINT8_C(0)\n                : (uint8_t)(phase_a7 + UINT8_C(1));\n            instructions += phase_a7 == UINT8_C(9)\n                ? UINT64_C(5) : UINT64_C(4);\n        } else {\n            next_fighter = phase_a7 == UINT8_C(0)\n                ? UINT8_C(9)\n                : (uint8_t)(phase_a7 - UINT8_C(1));\n            /* 0x60b50's negative path is three instructions shorter than\n             * idle, leaving net +1 normally and +2 for the 0 -> 9 wrap. */\n            instructions += phase_a7 == UINT8_C(0)\n                ? UINT64_C(2) : UINT64_C(1);\n        }\n        status = vf2_model2a_write(\n            machine, UINT32_C(0x005000a7),\n            &next_fighter, sizeof(next_fighter)\n        );\n    }\n    if (status == VF2_OK) {\n        status = vf2_model2a_write(machine, UINT32_C(0x005ff602), &spill, sizeof(spill));\n    }\n"""
if old not in text:
    raise SystemExit("index6 accounting anchor not found")
text = text.replace(old, new, 1)

old = """    cpu->registers[16] = (phase_a5 & UINT8_C(1)) == 0u\n        ? UINT32_C(0x2e)\n        : (phase_a5 == UINT8_C(1) ? UINT32_C(0x00532d2d)\n            : (phase_a5 == UINT8_C(5) ? UINT32_C(0x00002d2d) : 0u));\n"""
new = """    cpu->registers[16] = fighter_delta != 0\n        ? (fighter_delta < 0 ? UINT32_MAX : UINT32_C(1))\n        : ((phase_a5 & UINT8_C(1)) == 0u\n            ? UINT32_C(0x2e)\n            : (phase_a5 == UINT8_C(1) ? UINT32_C(0x00532d2d)\n                : (phase_a5 == UINT8_C(5) ? UINT32_C(0x00002d2d) : 0u)));\n"""
if old not in text:
    raise SystemExit("index6 g0 poststate anchor not found")
text = text.replace(old, new, 1)

source.write_text(text)

notes = Path("decomp/i960/notes/selector17_index6_bookkeeping.md")
body = notes.read_text()
addition = """

## Input state machine recovered from ROM

The common stable-page input tail at `0x0005cb70` is now recovered as well. The
five displayed pages use odd `a5` states (`1,3,5,7,9`), while page changes target
the even construction state of the destination page. Canonical forward input
`0x00500704 = 0x100` therefore maps `1->2`, `3->4`, `5->6`, `7->8`, and wraps
`9->0`. Canonical reverse input `0x200` maps `3->0`, `5->2`, `7->4`, `9->6`,
and wraps `1->8`. The construction frame then advances that even state to the
next stable odd state exactly as the ROM does.

Page 5 has an additional controller before the common page tail. The helper at
`0x00060b50` interprets canonical `0x1000` as `+1` and `0x2000` as `-1`; the
selected fighter in `a7` wraps over `0..9`. This is the lever-driven `MY CHAR`
selector described on screen. The recovered implementation preserves the ROM's
instruction deltas for normal and wrap transitions: page forward is idle +4
instructions (+5 for `9->0`), page reverse is idle +3 (+4 for `1->8`), fighter
+ is idle +4 (+5 for `9->0`), and fighter - is idle +1 (+2 for `0->9`). These
paths add no procedure calls beyond the already-accounted stable render.

The TEST exit mask in the same ROM tail branches directly to the shared teardown
at `0x0005f140`. That exit remains intentionally outside this cut until its full
caller-visible poststate is measured; the recovered bridge continues to reject
that input rather than substituting an estimated teardown state.
"""
if "## Input state machine recovered from ROM" not in body:
    notes.write_text(body.rstrip() + addition + "\n")
