# Passive natural sound evidence gate (v0882)

This is an **instrumentation milestone**, not a claim that natural gameplay
audio is already reproduced. The original ROM's i960 UART opcode anchors were
checked against the user's locally supplied VF2.1 set (October 19, 2026).

## New command

```sh
python tools/python/audio_natural_sound_gate.py \
  /path/to/vf2.zip out/real-precontact.jsonl --json
```

The trace must come from **genuine reference i960 execution** using
`vf2probe --trace --memory-trace` and a *valid*, frame-advancing
pre-contact or other game-event snapshot. A synthetic sound packet, forced
selector, fabricated trace or boot setup is **not** a gameplay-hit witness.

The script reuses existing **ROM-anchored** tools; it does not infer gameplay
semantics from an address name:

1. `audio_uart_transport.verify_uart_opcodes` validates the original i960
   instructions/operands against the local ROM.
2. `audio_uart_transport.upstream_writes` chains original selector choices to
   the deduplicator and an **exact** queue write in this same trace.
3. `observe_transport` requires queue-slot identity, original filer PC,
   vector-15 ISR, pop, staging, all **three** UART bytes and cursor advances,
   with contiguous steps and invalidation on overlapping/alien writes.
4. `audio_natural_sound_gate.evaluate_transport` refuses inconsistent
   packet bytes, unaligned slot, wrong producer, duplicate producer identity
   or impossible witness counts. It emits **only** samples whose prior
   selector witness was verified; samples are capped and visibly marked
   truncated when the total exceeds that cap.
5. The existing gate-0x50028 causality observer is run independently, and
   its counts are included. **It is not automatically joined** to the
   UART packet: proximity is not proof of event causality.

Output gates separate `selector_to_i960_uart` from
`natural_fighter_event_proven`, `m68k_l3_receive_proven`,
`m68k_c_differential_proven`, `scsp_keyon_from_live_gameplay_proven` and
`audible_live_gameplay_proven`. **Only the first may become true** in
this tool. The latter five remain false until independently verified.

## Regression and evidence

```sh
python tools/python/test_audio_natural_sound_gate.py -v
ctest --test-dir build -R vf2_python_audio_natural_sound_gate --output-on-failure
```

Eleven ROM-free cases cover a verified AD-family transport candidate, missing
selector, mismatched byte, wrong producer/alignment, duplicate producer,
count corruption, capped/empty samples, booleans, unrepresentable commands,
and a non-AD packet.

The first test was run independently as a pure Python test (11/11 passed).
No genuine gameplay trace was available in this change, so **there is no
positive claim** that the new end-to-end CLI observed a fight event.
The original `audio_uart_transport` and native C test suites were not
rerun as part of this instrumentation slice.

## Next required proof

1. Generate and retain **locally** a genuine frame-advancing pre-contact trace.
2. Obtain one verified `selector_to_i960_uart` candidate, plus a separate
   trace proving its game-event origin (not just boot or test-menu traffic).
3. Carry the **exact observed** three-byte packet into the original
   Motorola 68000 L3 `0x526` and dispatcher `0x7a6` timeline; compare
   the recovered C state and stepper RAM/SCSP state at controlled boundaries.
4. Only after original-driver KEYON plus nonzero SCSP output *caused by that
   same session event* can we call this a naturally emitted sound.

ROMs, snapshots, full traces and generated WAVs stay on the owner's
machine, never in Git.
