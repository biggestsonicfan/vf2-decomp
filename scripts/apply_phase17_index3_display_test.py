from pathlib import Path

path = Path("src/recovered/texture_bridge_match.c")
text = path.read_text()
anchor = "static vf2_status execute_frame_phase17_bit7_index11("
if text.count(anchor) != 1:
    raise SystemExit(f"index11 anchor count={text.count(anchor)}")

helper = r'''static vf2_status execute_frame_phase17_bit7_index3(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    uint8_t flagged_phase_index
)
{
    static const uint16_t row_bases[4] = {
        UINT16_C(0x9d80), UINT16_C(0x9d00),
        UINT16_C(0x9c00), UINT16_C(0x9c80)
    };
    static const uint32_t pattern_bases[4] = {
        UINT32_C(0x010b8020), UINT32_C(0x010b9020),
        UINT32_C(0x010ba020), UINT32_C(0x010bb020)
    };
    uint32_t indirect_target = 0u;
    uint32_t input_flags = 0u;
    uint32_t navigation_flags = 0u;
    uint32_t released_flags = 0u;
    uint32_t previous_flags = 0u;
    uint32_t selector_mask = 0u;
    uint8_t phase_a5 = 0u;
    uint8_t phase_a6 = 0u;
    uint32_t row = 0u;
    uint32_t column = 0u;
    uint32_t bank = 0u;
    uint32_t level = 0u;
    uint32_t word = 0u;
    const uint8_t next_a5 = UINT8_C(8);
    const uint8_t spill = UINT8_C(0x56);
    vf2_status status = VF2_OK;

    if (flagged_phase_index != UINT8_C(0x83) ||
        cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0005fec0), &indirect_target
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500700), &input_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500704), &navigation_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500708), &released_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050070c), &previous_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050002c), &selector_mask
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x005000a5), &phase_a5, sizeof(phase_a5)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x005000a6), &phase_a6, sizeof(phase_a6)
        );
    }
    if (status != VF2_OK || indirect_target != UINT32_C(0x00059f34) ||
        input_flags != UINT32_C(0x0ff7f700) || navigation_flags != 0u ||
        released_flags != 0u || previous_flags != UINT32_C(0x0ff7f700) ||
        selector_mask != UINT32_C(0x00020000) || phase_a5 != 0u ||
        phase_a6 != UINT8_C(0xff)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    /* 0x60600 builds four 9-row bands.  Each band contains sixteen
     * intensity tiles and each tile spans three columns. */
    for (row = 0u; status == VF2_OK && row < UINT32_C(36); ++row) {
        const uint16_t base = row_bases[row / UINT32_C(9)];
        for (column = 0u; status == VF2_OK && column < UINT32_C(48); ++column) {
            const uint16_t tile = (uint16_t)(
                base + (uint16_t)(column / UINT32_C(3))
            );
            status = write_u16(
                machine,
                UINT32_C(0x01000000) +
                    (row + UINT32_C(4)) * UINT32_C(0x80) +
                    (column + UINT32_C(7)) * UINT32_C(2),
                tile
            );
        }
    }

    /* Four pattern banks contain identical solid 4-bpp intensity tiles.
     * Tile zero is already blank; levels 1..15 are 8x8 solid nibbles. */
    for (bank = 0u; status == VF2_OK && bank < UINT32_C(4); ++bank) {
        for (level = 1u; status == VF2_OK && level <= UINT32_C(15); ++level) {
            const uint16_t pattern = (uint16_t)(level * UINT32_C(0x1111));
            for (word = 0u; status == VF2_OK && word < UINT32_C(16); ++word) {
                status = write_u16(
                    machine,
                    pattern_bases[bank] +
                        (level - UINT32_C(1)) * UINT32_C(32) +
                        word * UINT32_C(2),
                    pattern
                );
            }
        }
    }

    /* 15 evenly-spaced 5-bit samples plus the full-scale endpoint. */
    for (level = 0u; status == VF2_OK && level < UINT32_C(16); ++level) {
        const uint16_t component = (uint16_t)(
            level == UINT32_C(15) ? UINT32_C(31) : level * UINT32_C(2)
        );
        const uint16_t red = (uint16_t)(UINT16_C(0x8000) | (component << 10u));
        const uint16_t gray = (uint16_t)(
            UINT16_C(0x8000) | (component << 10u) |
            (component << 5u) | component
        );
        const uint16_t green = (uint16_t)(
            UINT16_C(0x8000) | (component << 5u)
        );
        const uint16_t blue = (uint16_t)(UINT16_C(0x8000) | component);
        status = write_u16(
            machine, UINT32_C(0x01800700) + level * UINT32_C(2), red
        );
        if (status == VF2_OK) {
            status = write_u16(
                machine, UINT32_C(0x01800720) + level * UINT32_C(2), gray
            );
        }
        if (status == VF2_OK) {
            status = write_u16(
                machine, UINT32_C(0x01800740) + level * UINT32_C(2), green
            );
        }
        if (status == VF2_OK) {
            status = write_u16(
                machine, UINT32_C(0x01800760) + level * UINT32_C(2), blue
            );
        }
    }

    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(2 * 0x80), UINT32_C(23), "DISPLAY TEST 1/2"
        );
    }
    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(41 * 0x80), UINT32_C(17),
            "COLOR         BIAS  GAIN SCROLL:"
        );
    }
    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(42 * 0x80), UINT32_C(24), "RED"
        );
    }
    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(43 * 0x80), UINT32_C(24), "GREEN"
        );
    }
    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(44 * 0x80), UINT32_C(24), "BLUE"
        );
    }
    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(45 * 0x80), UINT32_C(24), "EXIT"
        );
    }
    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(46 * 0x80), UINT32_C(0),
            "                  SELECT:1P LEVER UP/DOWN"
        );
    }
    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(47 * 0x80), UINT32_C(0),
            "   BIAS SET:1P PUNCH/KICK      GAIN SET:2P PUNCH/KICK"
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x005000a5), &next_a5, sizeof(next_a5)
        );
    }
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x005ff600), UINT16_C(0));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x005ff700), UINT32_C(0x01000f8e)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions += UINT64_C(24451);
    cpu->procedure_calls += UINT64_C(20);
    cpu->procedure_returns += UINT64_C(20);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[0] = 0u;
    cpu->registers[1] = UINT32_C(0x005ff580);
    cpu->registers[2] = UINT32_C(0x0000a010);
    cpu->registers[3] = 0u;
    cpu->registers[4] = UINT32_C(0x00515400);
    cpu->registers[5] = UINT32_C(0x3f800000);
    cpu->registers[6] = 0u;
    cpu->registers[7] = 0u;
    cpu->registers[8] = UINT32_MAX;
    cpu->registers[9] = UINT32_MAX;
    cpu->registers[10] = UINT32_MAX;
    cpu->registers[11] = UINT32_MAX;
    cpu->registers[12] = 0u;
    cpu->registers[13] = 0u;
    cpu->registers[14] = UINT32_C(0x0000010f);
    cpu->registers[15] = UINT32_C(0x00008a00);
    cpu->registers[16] = UINT32_C(0x0000004b);
    cpu->registers[17] = 0u;
    cpu->registers[18] = UINT32_C(0x00000060);
    cpu->registers[19] = 0u;
    cpu->registers[20] = UINT32_C(0x00560000);
    cpu->registers[21] = 0u;
    cpu->registers[22] = UINT32_C(0xffff9c8f);
    cpu->registers[23] = UINT32_C(1);
    cpu->registers[24] = UINT32_C(0x00512980);
    cpu->registers[25] = UINT32_C(0x01001800);
    cpu->registers[26] = UINT32_C(0x00800000);
    cpu->registers[27] = UINT32_C(0x00880000);
    cpu->registers[28] = UINT32_C(0x00004000);
    cpu->registers[29] = UINT32_C(0x00516480);
    cpu->registers[30] = UINT32_C(0x00000220);
    cpu->registers[31] = UINT32_C(0x005ff500);
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
    cpu->compare_result = VF2_I960_COMPARE_GREATER;

    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(2834 + 960 + 64 + 7);
    report->bytes_written = (size_t)UINT32_C(5880);
    report->recovered_instruction_count = UINT64_C(24451);
    report->recovered_procedure_calls = UINT64_C(20);
    report->recovered_procedure_returns = UINT64_C(21);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

'''
text = text.replace(anchor, helper + anchor, 1)
route = """        if (phase_index == UINT8_C(0x82)) {
            return execute_frame_phase17_bit7_index2(
                machine, cpu, report, phase_index
            );
        }
        return execute_frame_phase17_bit7_index11(
"""
route2 = """        if (phase_index == UINT8_C(0x82)) {
            return execute_frame_phase17_bit7_index2(
                machine, cpu, report, phase_index
            );
        }
        if (phase_index == UINT8_C(0x83)) {
            return execute_frame_phase17_bit7_index3(
                machine, cpu, report, phase_index
            );
        }
        return execute_frame_phase17_bit7_index11(
"""
if text.count(route) != 1:
    raise SystemExit(f"route anchor count={text.count(route)}")
text = text.replace(route, route2, 1)
path.write_text(text)
