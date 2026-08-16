from pathlib import Path

path = Path('src/recovered/texture_bridge_match.c')
s = path.read_text()
old = '''    if (status != VF2_OK || mode == UINT8_C(25) ||
        (base_flags & UINT32_C(3)) != 0u ||
        (!first_visit && phase_a5 != UINT8_C(0xff)) ||
        (first_visit && (system_flags & UINT8_C(1)) == 0u)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    expected_instructions =
        first_visit ? UINT64_C(13286) : UINT64_C(626);
    expected_calls = first_visit ? UINT64_C(27) : UINT64_C(25);
    expected_returns = first_visit ? UINT64_C(28) : UINT64_C(26);
'''
new = '''    if (status != VF2_OK || mode == UINT8_C(25) ||
        (base_flags & UINT32_C(3)) != 0u ||
        (!first_visit && phase_a5 != UINT8_C(0xff))) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    if (first_visit && (system_flags & UINT8_C(1)) == 0u) {
        expected_instructions = UINT64_C(13844);
        expected_calls = UINT64_C(31);
        expected_returns = UINT64_C(32);
    } else {
        expected_instructions =
            first_visit ? UINT64_C(13286) : UINT64_C(626);
        expected_calls = first_visit ? UINT64_C(27) : UINT64_C(25);
        expected_returns = first_visit ? UINT64_C(28) : UINT64_C(26);
    }
'''
if old not in s:
    raise SystemExit('precondition/count block not found')
s = s.replace(old, new, 1)
old2 = '''        cpu->registers[6] = system_flags;
    } else {
'''
new2 = '''        cpu->registers[6] = system_flags;
        if (status == VF2_OK && (system_flags & UINT8_C(1)) == 0u) {
            static const char backup_mode_text[] =
                "STATIC RAM IS 'BACK-UP MODE'";
            static const char invalid_changes_text[] =
                "AND YOUR CHANGES ARE INVALID !!";

            status = write_phase17_index0_text(
                machine, UINT32_C(30 * 0x80), UINT32_C(19),
                backup_mode_text
            );
            if (status == VF2_OK) {
                status = write_phase17_index0_text(
                    machine, UINT32_C(33 * 0x80), UINT32_C(18),
                    invalid_changes_text
                );
            }
            if (status == VF2_OK) {
                characters += (uint64_t)(sizeof(backup_mode_text) - 1u);
                characters += (uint64_t)(sizeof(invalid_changes_text) - 1u);
                account_nested_procedure(cpu, UINT64_C(4), UINT64_C(4));
            }
        }
    } else {
'''
if old2 not in s:
    raise SystemExit('first-visit tail marker not found')
s = s.replace(old2, new2, 1)
path.write_text(s)

note = Path('decomp/i960/notes/selector17_index11_exit_test_mode.md')
note.write_text('''# selector17 bit-7 index11 — EXIT TEST MODE\n\nEntry slot `0x0005ff00` targets `0x0005ef60`; the flagged phase index is `0x8b`.\n\nThe ROM handler has two persistent states rather than a conventional menu:\n\n- `a5 == 0`: first visit. It updates the game meter, recomputes the 15-byte coin/config CRC through `0x5ff54 -> 0x9480`, clears the diagnostic plane, renders the exit-mode diagnostic record, stores a 320-frame countdown at `0x00500024`, and changes `a5` to `0xff`.\n- `a5 == 0xff`: decrements the countdown. Positive values return normally. A non-positive value executes the terminal reset path at `0x5f07c` and branches directly to boot entry `0x000000b0`.\n\n## Static-RAM backup-mode warning\n\nThe previously unsupported first-visit branch is selected when bit 0 of `0x00500171` is clear. The ROM does not abort; after the ordinary first-visit screen it renders two inline strings via the `0x9444` text helper:\n\n- `0x5f00c`: `STATIC RAM IS 'BACK-UP MODE'` at tile destination `0x01000f26` (row 30, column 19).\n- `0x5f03c`: `AND YOUR CHANGES ARE INVALID !!` at `0x010010a4` (row 33, column 18).\n\nDirect ROM measurements from `0x5ef60` to the caller return show:\n\n| path | raw instructions | raw calls | full frame bridge accounting |\n| --- | ---: | ---: | ---: |\n| first visit, normal | 13,259 | 25 | 13,286 / 27 / 28 |\n| first visit, backup mode | 13,817 | 29 | 13,844 / 31 / 32 |\n| countdown > 0 | 599 | 23 | 626 / 25 / 26 |\n| terminal countdown | 13,171 to `0xb0` | 25 | 13,194 / 27 / 25 |\n\nThe +558 instruction / +4 call delta on backup mode comes from the two warning-text helper invocations and their nested work. The recovered bridge renders both strings and accounts those four nested procedures explicitly.\n\n## Terminal reset\n\nAt countdown expiry the ROM:\n\n1. writes `0x8000` to `0x00500082`;\n2. clears bit 15 from the two words at `0x0100a00c`;\n3. clears `0x0050009c`;\n4. clears the 64x48 diagnostic tile plane;\n5. clears video-control bits 0 and 1 through the pointer at `0x0050081c`;\n6. zeros the four input words at `0x00500700..0x0050070c`;\n7. writes zero to `0x00e80004`;\n8. calls `0x6116c`, leaves the RESET sentinel words in `0x0059cfe0`, and branches to boot entry `0xb0` rather than returning through the phase wrappers.\n\nNo ROM or snapshot bytes are stored in the repository; measurements were made locally against the owned VF2 v2.1 ROM set.\n''')
