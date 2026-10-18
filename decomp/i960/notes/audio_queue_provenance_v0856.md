# v0856 — Real sound-queue provenance instrumentation

Scope: **passive trace-analysis tool only**, no game or hardware behavior changes.
This is the next proof gate for a natural gameplay sound-event producer.

## Confirmed ROM findings

- The v0855 static scan found 39 aligned direct writes to fighter `+0x624`.
- 36 are directly preceded by `mov 0,r15`, and `0x54ad4` is locally preceded
  by `mov 0,r3; stob r3,+0x2a`.
- `0x14190` is now a **38th proven zero write**: the straight store-only
  path starts with `mov 0,r3` at `0x14120`; the bound includes two
  unindexed two-word MEMB stores and has no register mutation/branch.
- The only other direct writer, `0x23b44`, is already measured by v0283 as
  a `fa_coli` bit-remap result from `0x23878`. This is **not** an
  observed SFX event; indexed/indirect writers are not covered.

## Correlate the real writer — no packet staging

The new `tools/python/audio_queue_provenance.py` parses the *actual*
`vf2probe --trace --memory-trace` JSONL contract. It joins memory access
`step` to the instruction event with that **same** `step` and uses
`ip_before` as writer PC. Without a paired step event it reports
`pc = null` rather than fabricating provenance.

It watches writes to:

- i960 queue ring `0x504020..0x50405f`, counters/cursors and stage;
- actual Model 2A UART `0x01c80000` (not the incorrect `0x009c0000`);
- optional fighter `base+0x1b1` and `base+0x624` when explicitly supplied.

Run it against a valid user-local, naturally reached pre-event snapshot:

~~~sh
# Native Windows build example (the repository's canonical workflow)
build/Debug/vf2probe.exe --rom-dir roms/vf2 \
  --snapshot out/real-pre-action.vf2snap --max-steps 500000 \
  --trace --memory-trace > out/real-action-audio.jsonl

python tools/python/audio_queue_provenance.py \
  out/real-action-audio.jsonl --fighter-base 0x00510980 --json

python tools/python/test_audio_queue_provenance.py -v
~~~

**Important**: the fighter base must come from that exact snapshot (e.g.
read `0x500804/0x500808`), not from the example. The probe by itself
may park on a wait-for-vblank loop; use the live frame driver and a
valid before-contact checkpoint. A queue write may just be boot traffic.
No natural gameplay SFX or audible voice is claimed here.

Six ROM-free parser tests pass locally; the updated field-census suite
also passes its six tests. The C full-suite and a naturally reached
pre-contact game trace have **not** run in this environment.

## Recovery acceptance

1. Identify a real live `queue_entry` writer with PC, data and frame.
2. Prove it is a gameplay event and not boot/init traffic.
3. Follow IRQ-15 UART, L3, 68000 dispatch and SCSP key-on in both C and
   ROM-stepped oracle, matching state.
4. Only then promote recovered C; refuse unsupported siblings.
