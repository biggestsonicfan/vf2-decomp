# v0870 — AD23/AD24 A0 descriptor reaches L2 worker at 0x1f92

The newly recovered A0 `0x1f04` descriptor initialisation creates
a live 0x20-byte stream descriptor at sound-RAM 0x3060, flagged
with bit7 and initialized to BE16 countdown 1. The L2 stream tick
receives this state immediately after the C dispatcher has
consumed the command-ring packet.

The C runtime's existing `vf2_sound_board_maintain_streams`
rejects this arm *before mutation*: `VF2_ERROR_UNSUPPORTED`,
refused_stage=2 (worker), refused_record=3, timers_expired=1.
This is the **next true functional blocker to audible sound**:
the 68000 code at 0x1f92 starts reading the stream opcode from
the selected original AD23/AD24 record.

A ROM-backed differential test now explicitly:
1. Files original AD23/AD24 UART bytes and proves C==68000 L3 RAM.
2. Sets up one free A0 record descriptor and compares all 32 bytes
   of native C vs original 68000 at the 0x1f02 RTS boundary.
3. Runs the recovered C L2 tick and verifies the transactional refusal,
   with unchanged full RAM and SCSP.
4. From the same sound-RAM prestate, independently starts the 68000
   stepper at 0x1f7c (A2=0x3060) and stops *before* the unmodeled
   worker opcode at 0x1f92. The expected micro-path is:
   BTST bit7, BNE, SUBQ.W #1,+2, BEQ → 0x1f92.
   Therefore the 68000 countdown is 0 while C keeps it 1,
   explicitly exposing their intentional divergence.

Controlled setup only; no claim of natural i960 combat trigger or
SCSP voice keying. The new L2 ROM-dependent differential was
committed but cannot be executed locally without the entire checkout.
It must pass CTest on the owner machine to promote 0x1f92 to
measured execution evidence.

Next implementation step: recover the 68000 worker 0x1f92..0x1fba
for AD23/AD24 record bytes (0xbb/0xba, high-nibble B), then
the selected class handler rather than inventing audio.
