# v0868 — ROM opcode-anchored A0 0x1e98 to 0x1ed4 descriptor-scan boundary

The original VF2.1 Motorola 68000 ROM's A0 selection joins at
0x1e98. This newly pinned *static* path has 24 checked opcode/extension
words. The code looks up the command-selected record pointer into A2,
reads the first byte at 0x1e9e and checks bit 7 with the immediate
btst at 0x1ea0. The 0x1ea4 bne.w targets 0x1ed4 exactly.

All 256 original AD23/AD24 12-byte records start with 0x8f, so bit
7 is set for both families. For a selected record, this path therefore
branches to 0x1ed4 (conditional on an actual 68000 execution reaching
0x1e98 with those inputs). This is an opcode-and-data deduction, NOT
proof of live gameplay execution.

The code at 0x1ed4 searches six 0x20-byte descriptors starting at
sound RAM 0x3020, testing bit 7 of their first byte. The address of
the first bit-7-clear descriptor can be predicted from six real status
bytes, with a separate exhausted result when none qualifies. After
the scan, the original program calls helper 0x1f04. The predictor
does not simulate that helper, subsequent field assignments,
mutations, 68000 stack behavior, hardware MIDI, or SCSP voice keying.

tools/python/audio_a0_control_flow.py verifies original instruction
words and branch targets against the local ROM, then produces only
a bounded prediction. test_audio_a0_control_flow.py includes
five synthetic refusal/positive cases and one real-ROM opcode/branch
regression. Local: 6/6 module tests and 19/19 combined A0 tests passed.

Reproduce:

    VF2_ROM_ZIP=/path/to/vf2.zip python -m unittest discover -s tools/python -p 'test_audio_a0_*.py'
    python tools/python/audio_a0_control_flow.py /path/to/vf2.zip --command 0x00ad2342 --status 80,80,00,00,00,00

Do not promote 0x1ed4 or the helper to a recovered C runtime path
until the existing C-vs-68000 differential can measure actual
mutations from a controlled pre-event state. No proprietary ROM or
raw dump is versioned.
