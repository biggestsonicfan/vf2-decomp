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

## v0883 — Observed UART packet -> original Motorola 68000 L3 receiver

The ROM-backed C test `vf2_m68k_l3_selection_tests` previously ran only
the two ROM-table A0/L2 known selectors. It now also accepts an **opt-in**
three-byte payload and runs the identical strict **byte-by-byte C vs
original 68000 L3** differential already used by those two defaults:

```sh
# Tested packet format: exactly six hexadecimal characters, no 0x prefix.
# For example only; this packet was NOT captured from natural gameplay.
build/vf2_m68k_l3_selection_tests roms/vf2 --l3-packet AD2342
```

The command runs `vf2_sound_board_file_uart_byte` against original
68000 code at `0x52E..0x5EA`, compares the entire sound RAM (excluding
the documented stepper-only JSR return stack spill) and SCSP register
state after **each byte**, and asserts ring count and final hardware poke.
An invalid, wrong-length or nonhex packet exits before ROM loading.
Default runs still do the original A0/L2 checks.

To replay **only packets with a verified i960 selector-to-UART witness**
from a locally generated gate JSON:

```sh
python tools/python/audio_natural_sound_gate.py \
  /path/to/vf2.zip out/real-precontact.jsonl --json \
  > out/local-audio-gate.json

# Linux/macOS, build after current master:
python tools/python/audio_l3_packet_replay.py \
  out/local-audio-gate.json build/vf2_m68k_l3_selection_tests \
  --rom-zip /path/to/vf2.zip --limit 4

# Native Windows/MSVC:
python tools/python/audio_l3_packet_replay.py \
  out/local-audio-gate.json build/Debug/vf2_m68k_l3_selection_tests.exe \
  --rom-directory roms/vf2
```

The opt-in utility extracts a user-provided ZIP only into an automatically
removed temporary directory, validates file names/count/size, invokes the
existing strict binary with argv (not a shell), requires BOTH zero exit
and the precise L3 receiver match marker, and reports error/timeout as
negative evidence. It never publishes samples, ROM content or raw trace.
The caller is responsible for the integrity of the local gate JSON.
A forged or synthetic report cannot certify natural gameplay.

**Crucial scope boundary:** the observed i960 UART packet and C-vs-68000
L3 test are still **two separately initialized contexts**. They do not
demonstrate correct UART-to-L3 IRQ timing, dynamic gameplay event origin,
full dispatcher behavior, KEYON, or audible PCM in the live game.
One real frame-progressing trace plus contiguous original-68000 state
is still required before promoting an end-to-end claim.

ROM-free regressions:

```sh
python tools/python/test_audio_l3_packet_replay.py -v
ctest --test-dir build -R vf2_python_audio_l3_packet_replay --output-on-failure
```
