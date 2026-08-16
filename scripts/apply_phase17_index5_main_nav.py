from pathlib import Path

p = Path("src/recovered/texture_bridge_match.c")
s = p.read_text()
a = s.index("static vf2_status execute_frame_phase17_bit7_index5(")
b = s.index("static vf2_status execute_frame_phase17_bit7_index11(", a)
f = s[a:b]


def one(old: str, new: str, label: str) -> None:
    global f
    count = f.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected one occurrence, got {count}")
    f = f.replace(old, new, 1)


one(
    """    static const uint32_t cursor_addresses[5] = {
        UINT32_C(0x01001320), UINT32_C(0x010002a0),
        UINT32_C(0x01000320), UINT32_C(0x01000420),
        UINT32_C(0x010005a0)
    };""",
    """    static const uint32_t cursor_addresses[6] = {
        UINT32_C(0x01001320), UINT32_C(0x010002a0),
        UINT32_C(0x01000320), UINT32_C(0x01000420),
        UINT32_C(0x010005a0), UINT32_C(0x010011a0)
    };""",
    "cursor array",
)

one(
    """    int edit_delta = 0;
    int manual_navigation_delta = 0;""",
    """    int edit_delta = 0;
    int main_navigation_delta = 0;
    int manual_navigation_delta = 0;""",
    "navigation variable",
)

one(
    """        (phase_a5 == UINT8_C(5) && phase_a7 > UINT8_C(4))) {""",
    """        (phase_a5 == UINT8_C(5) &&
         phase_a7 != UINT8_C(0xff) && phase_a7 > UINT8_C(4))) {""",
    "validation",
)

one(
    """    else if (phase_a5 == UINT8_C(5) && navigation_flags == UINT32_C(0x1000))
        manual_navigation_delta = 1;
    else if (phase_a5 == UINT8_C(5) && navigation_flags == UINT32_C(0x2000))
        manual_navigation_delta = -1;
    else if (navigation_flags != 0u) return VF2_ERROR_UNSUPPORTED;""",
    """    else if (phase_a5 == UINT8_C(5) && phase_a7 != UINT8_C(0xff) &&
             navigation_flags == UINT32_C(0x1000))
        manual_navigation_delta = 1;
    else if (phase_a5 == UINT8_C(5) && phase_a7 != UINT8_C(0xff) &&
             navigation_flags == UINT32_C(0x2000))
        manual_navigation_delta = -1;
    else if (phase_a7 == UINT8_C(0xff) && navigation_flags == UINT32_C(0x1000))
        main_navigation_delta = 1;
    else if (phase_a7 == UINT8_C(0xff) && navigation_flags == UINT32_C(0x2000))
        main_navigation_delta = -1;
    else if (navigation_flags != 0u) return VF2_ERROR_UNSUPPORTED;""",
    "input routing",
)

one(
    "    if (phase_a5 == UINT8_C(5)) {",
    "    if (phase_a5 == UINT8_C(5) && phase_a7 != UINT8_C(0xff)) {",
    "manual branch guard",
)

one(
    "        status = write_u16(machine, cursor_addresses[phase_a5], UINT16_C(0x801c));",
    """        status = write_u16(
            machine, cursor_addresses[phase_a5],
            main_navigation_delta == 0 ? UINT16_C(0x801c) : UINT16_C(0x8020)
        );""",
    "cursor rendering",
)

one(
    """    if (status == VF2_OK && edit_delta != 0) {
        if (phase_a5 == UINT8_C(1)) {""",
    """    if (status == VF2_OK && main_navigation_delta != 0) {
        int next = (int)phase_a5 + main_navigation_delta;
        uint8_t next_selection = 0u;
        if (next < 0) next = 5;
        else if (next > 5) next = 0;
        next_selection = (uint8_t)next;
        status = vf2_model2a_write(
            machine, UINT32_C(0x005000a5), &next_selection, sizeof(next_selection)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005ff680),
                main_navigation_delta < 0 ? UINT32_MAX : UINT32_C(1)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005ff684), UINT32_C(0x0007ae10)
            );
        }
        instructions = main_navigation_delta > 0
            ? (phase_a5 == UINT8_C(5) ? UINT64_C(4195) : UINT64_C(4194))
            : (phase_a5 == UINT8_C(0) ? UINT64_C(4192) : UINT64_C(4191));
        calls = UINT64_C(34);
    } else if (status == VF2_OK && edit_delta != 0) {
        if (phase_a5 == UINT8_C(1)) {""",
    "navigation action",
)

one(
    "    cpu->registers[16] = edit_delta == 0 ? 0u : (uint32_t)checksum;",
    """    cpu->registers[16] = main_navigation_delta != 0
        ? (main_navigation_delta < 0 ? UINT32_MAX : UINT32_C(1))
        : (edit_delta == 0 ? 0u : (uint32_t)checksum);""",
    "r16",
)

one(
    "    cpu->registers[25] = UINT32_C(0x010005d4);",
    """    cpu->registers[25] = main_navigation_delta != 0
        ? cursor_addresses[phase_a5] : UINT32_C(0x010005d4);""",
    "r25",
)

one(
    """    if (phase_a5 == 0u && edit_delta == 0) {
        cpu->arithmetic_control = (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
        cpu->compare_result = VF2_I960_COMPARE_GREATER;
    } else {
        cpu->arithmetic_control = (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    }""",
    """    if (main_navigation_delta != 0) {
        static const uint32_t nav_cc_forward[6] = {1u, 1u, 1u, 1u, 2u, 4u};
        static const uint32_t nav_cc_back[6] = {2u, 1u, 1u, 1u, 1u, 1u};
        const uint32_t cc = main_navigation_delta > 0
            ? nav_cc_forward[phase_a5] : nav_cc_back[phase_a5];
        cpu->arithmetic_control = (cpu->arithmetic_control & ~UINT32_C(7)) | cc;
        cpu->compare_result = cc == UINT32_C(1)
            ? VF2_I960_COMPARE_GREATER
            : (cc == UINT32_C(2) ? VF2_I960_COMPARE_EQUAL : VF2_I960_COMPARE_LESS);
    } else if (phase_a5 == 0u && edit_delta == 0) {
        cpu->arithmetic_control = (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
        cpu->compare_result = VF2_I960_COMPARE_GREATER;
    } else {
        cpu->arithmetic_control = (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    }""",
    "condition codes",
)

s = s[:a] + f + s[b:]
p.write_text(s)
