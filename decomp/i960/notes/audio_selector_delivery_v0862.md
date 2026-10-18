# v0862 — Opcode-anchored selector → dedup → queue-filer witness

Scope: **passive trace instrumentation and ROM opcode proof**. No claim
that a real punch/kick sound has been observed or rendered.

## Recovered static corridor

The supplied VF2.1 Model 2A maincpu ROM verifies these exact opcodes:

- 0x18a98 reads one 32-bit command from a character-indexed row of
  the table at 0x0201e44c into r5 (opcode-checked in v0861).
- 0x18ab8: mov r5, g0; exact opcode 0x5c801605.
- 0x18abc: direct CTRL call to dedup at 0x439ac.
- 0x18ac0: direct CTRL call to queue filer at 0x43888.

This is an instruction-level data/call-flow chain. The branch at
0x18a9c and possible mode-prefix filer at 0x18ab4 still determine
which instructions a live game reaches. Static proximity alone
must not be mistaken for a delivered gameplay command.

## Implemented passive acceptance gate

tools/python/audio_selector_delivery.py first asks the v0861
selector observer to verify a row/column/command read **on the same
JSONL trace**. It then requires the original selected-command
move, direct dedup call, its return to 0x18ac0, direct filer call,
a full aligned 32-bit ring write at 0x00504020..0x0050405f with the
same word, and the filer's return to 0x18ac4.

It rejects partial writes, different ring values, missing calls,
skipped mov, unpaired memory events, discontinuous instruction
steps and absent returns. All rings and calls are observed, never
injected. Bounded event/witness reporting avoids unbounded output.

Invocation (the input trace must be naturally reached from a
pre-contact snapshot; a parked or synthetic trace cannot close 9/10):

    python tools/python/audio_selector_delivery.py /path/to/vf2.zip out/precontact.jsonl --json
    VF2_ROM_ZIP=/path/to/vf2.zip python -m unittest discover -s tools/python -p 'test_audio_*.py'

ROMs, generated audio and traces remain local and uncommitted.

Validation: 81/81 local Python tests, including the new 14 and a
real-ROM opcode check. The test trace is synthetic: it establishes
the observer's refusal and acceptance rules, not natural gameplay
reachability. No complete native C suite, real pre-contact trace,
68000 differential or audible combat SFX was produced in this slice.

Next: capture a real pre-contact trace, require this witness, then
follow the same ring entry through interrupt-15 UART -> 68000 L3,
dispatcher and SCSP key-on. Do not elevate a temporal match to
a gameplay completion claim.
