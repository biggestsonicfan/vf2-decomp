# v0858 — Shared signed16 gate for the Model 2A audio-table dispatcher

Status: **ROM-backed opcode proof plus passive instrumentation**, not a
gameplay-sound completion. The input is the user's own local VF2.1
Model 2A set. ROMs, restored snapshots, WAVs and traces are not committed.

## Proven from populated i960 program (LOAD32_WORD manifest order)

The *only two aligned direct CTRL calls* to 0x18a54 catalogued in v0857
have the same branch predicate, verified against the actual instruction
words and direct-call targets rather than relying on decoded prose:

| Load short signed | Branch | Call | Skip target | Condition for call |
|---|---|---|---|---|
| 0x16150: `ldis 0x00500028, r14` | 0x16158: `cmpibge 0,r14,+8` | 0x1615c -> 0x18a54 | 0x16160 | `int16[0x00500028] < 0` |
| 0x23070: `ldis 0x00500028, r14` | 0x23078: `cmpibge 0,r14,+0x24` | 0x2308c -> 0x18a54 | 0x2309c | `int16[0x00500028] < 0` |

No positive/zero value takes either direct-call path *through this
control-flow edge*. This is a branch proof, not an assertion that a
sound effect plays whenever the field becomes negative.

An aligned, direct, absolute-MEMB census of the populated 0x80000
i960 program found **17 reads and 8 writes** to 0x00500028. The
eight absolute writer instruction PCs are:
`0x0baa4`, `0x0bc3c`, `0x0c214`, `0x0c3a0`,
`0x0db28`, `0x0de68`, `0x0e3c4`, `0x0e4bc`.

Opcode-local facts for the next dynamic drive:

- `0x0bc3c` and `0x0c3a0`: `ldos` from the flag, `subi 1`,
  `stos` back. On an input zero, the stored 16-bit value wraps to
  `0xffff`, which satisfies the negative branch.
- `0x0db28` and `0x0de68`: `mov 0,r15`; `stis` zero the flag.
- `0x0e4bc`: `ldis` from the flag and `lda -1(r15),r15`;
  `stis` writes the decremented value. It is **not** a simple
  constant -1 setter.
- `0x0baa4` and `0x0c214`: left shift by seven of r15 and store
  its low 16 bits; exact source and reachability need a trace.
- `0x0e3c4`: compound bit manipulation from nearby flags; no
  semantic label without differential confirmation.

This static inventory is a **lower bound**, not an exhaustive
analysis of aliasing, indexed MEMB, indirect writes or instruction
reachability.

## Implementation and gate to run

`tools/python/audio_gate_50028.py` loads local `vf2.zip`, rejects
missing/replaced branch and call words, prints gate conditions and
enumerates the eight direct writers. Optionally feed actual
`vf2probe --trace --memory-trace` JSONL to show whether a *live*
branch skipped, fell through, or called the dispatcher. The real
observer emits memory before the matching `step` event; writer PCs
are attributed only on exact step equality. Absent PC stays unknown.
No event is injected or synthesized.

~~~sh
python tools/python/test_audio_gate_50028.py -v
python tools/python/audio_gate_50028.py /path/to/vf2.zip --json
python tools/python/audio_gate_50028.py /path/to/vf2.zip \
  --trace /path/to/legitimate-pre-contact-vf2probe.jsonl --json
~~~

Live trace creation on a machine with an authentic reachable
pre-contact snapshot:

~~~sh
build/Debug/vf2probe.exe --rom-dir roms/vf2 \
  --snapshot out/real-pre-contact.vf2snap --max-steps 500000 \
  --trace --memory-trace > out/sound-gate-pre-contact.jsonl
~~~

A single restored probe can park at the wait-for-vblank loop, so
this may require the already-existing frame/IRQ runner. Do not
replace a missing real contact with a synthesized producer.

Local evidence at this slice: analyzer run against the user-owned
ROM returned **17 reads, 8 writes, 2 exact gates**; 9 new
ROM-free tests passed, as did the 22 preceding audio Python tests
(31/31 combined). **No C suite, gameplay-event trace or
C-vs-68000 differential was run** in this turn.

Next: identify the earliest *reached* writer making the signed
control negative; capture the 0x18a54 call and then queue file
0x43888 on a naturally reached event. Only promote gameplay
behavior to recovered C after that proof.
