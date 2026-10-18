# v0859 — Overlap-safe audio-gate event provenance

Scope: passive trace evidence, no gameplay or 68000 behavior changes.

The i960 low halfword at 0x00500028 is shared phase/selector state,
also polled by the attract scheduler (see
decomp/i960/notes/attract_interrupt_injection_vs_mame.md).
Its negative signed-16 value gates the two ROM-confirmed direct
calls to 0x18a54, but is not independently proven to be an SFX flag.

## Bug fixed

The previous passive gate tool saw only 16-bit bus accesses whose
starting address equaled 0x00500028. It ignored writes to the full
32-bit word and partial byte updates, which could produce falsely
stale values. The new audio_gate_causality.py consumes genuine
vf2probe --trace --memory-trace JSONL and:

- recognizes full little-endian 16-bit values in covering bus
  accesses (including 32-bit writes);
- reports partial writes as unknown instead of retaining stale
  halfword values;
- attributes a write to the instruction with the EXACT same
  step, not a neighboring instruction;
- matches the load to the immediately executed signed gate branch;
- checks actual branch outcome against the loaded value and
  detects contradictions;
- joins a fall-through to 0x1615c/0x2308c only across a continuous
  straight-line trace; missing events remain unverified.

Nine new ROM-free unit tests cover both success and refusal paths.

## Related ROM evidence

The 17-read / 8-write absolute-reference census from v0858 was
rerun against the supplied VF2 ROM. Adjacent to the 0xde68
phase-field reset is the 0xde5c filer call, which stages
0x00ad220c from an immediate. These instructions do not prove a
naturally emitted gameplay event: a branch between them can select
different paths. Do not collapse phase and sound semantics.

## Reproduce

    python tools/python/test_audio_gate_causality.py -v
    python tools/python/audio_gate_causality.py out/real-precontact.jsonl --json

The input must be a valid live pre-contact trace with frame
interrupts. Forced flags and synthetic commands can test the tool
but cannot establish original gameplay audio.

Local validation: 40/40 Python audio-tool tests passed (31 old,
9 new). The sound-board C test suite and a natural gameplay
C-vs-68000 differential remain unrun in this slice.
