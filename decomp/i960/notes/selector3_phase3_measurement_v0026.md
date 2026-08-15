# Selector-3 phase-3 measurement (v0.0.26)

This note records the ROM measurements used to recover selector-3 phase 3 (`0x0000b394`) inside the frame dispatcher rooted at `0x0000a6c0`.

Four controlled routes were measured through the `0x0000acf8` wrapper and its `0x00001344` fast exit:

- positive countdown without sound event: **38 instructions / 3 calls / 4 returns**;
- countdown crossing 192, including `0x00043888` sound-event enqueue: **70 / 4 / 5**;
- terminal countdown with free terminal gate, advancing phase 3 -> 4: **43 / 3 / 4**;
- terminal countdown with gate value 1, remaining in phase 3: **40 / 3 / 4**.

At counter 192 the ROM calls `0x43888` with event `0x00ad1001`. The recovered path implements the observed mode/flag gates, sound ring count/index update, ring-slot write, and hardware command writes. The ordinary phase-3 worker also sets bit 16 in `0x00500068`, decrements `0x00500024`, and only consults `0x00550000` once the countdown becomes non-positive.

A full snapshot comparison exposed one wrapper-level translation error: i960 `shlo r3, 1, r4` at `0xacf8` produces `1 << phase`, not `phase << 1`. Therefore `0x00500034` receives the phase mask (phase 0 -> 1, phase 1 -> 2, phase 3 -> 8). Commit `16e1cdf927e7faa8753c30e9c144a5611d88b6b2` corrects this shared wrapper semantic.

Implementation commit `e0f435cd0d47134bb0a3ddbae22e4cb729b616f2` adds the phase-3 sound event and route-dependent frame accounting. The wrapper-mask correction is revalidated against the phase-0, phase-1, and phase-3 controlled vectors using the normal CI analysis binary.
