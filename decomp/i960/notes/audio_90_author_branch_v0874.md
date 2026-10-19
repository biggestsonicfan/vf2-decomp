# v0874 — Original ROM 90 HIGH free-slot branch 0xFAC → 0xA8C

After the AD23/AD24 L2 second packet, the original sound CPU
boot descriptors #11 (9B channel 0B) and #10 (9A channel 0A)
select the 90 HIGH path; both HIGH table entries are non-sentinel
and proceed through Loop-B to voice allocation.

The existing recovered C runtime deliberately stops with
VF2_ERROR_UNSUPPORTED at the first free voice in the allocator,
reporting PC 0xFAC. The *original* 68000 ROM at 0xFAC has
opcode 6000 FADE, a BRA.W displacement relative to 0xFAE.
The computed target is exactly **0xA8C**, the existing
voice authoring path. Its first six ROM bytes are
137C 0040 0002, MOVE.B #0x40, 2(A1), arming the chosen
voice record's status field.

Two ROM-free tests cover the exact branch/first-store signature
and reject a modified displacement. The original-ROM golden
test now checks this signature too. Local:
10/10 test_audio_90_boot_route tests passed including user VF2.zip.

A separate ROM-dependent CTest
tests/m68k/test_90_to_voice.c
starts the *original 68000 stepper* at 0x7A6 with a freshly
initialized sound board and authentic 9B/9A follow-up packets.
It verifies arrival at 0xFAC, actual A1 voice pointer 0x2800,
C/68000 common lookup word values, BRA to 0xA8C and the
voice-status store at 0xA8C. This test is only registered if
VF2_ROM_DIR exists and is NOT executed in the public CI without
the owner's ROMs. It must not be marked passing until run locally.

The recovered C allocator's first-free refusal is intentionally
unchanged pending the above differential. Do not confuse
compiler/ROM-static tests with a real 68000 execution or audible
key-on. No ROM/audio/snapshot content is included in Git.
