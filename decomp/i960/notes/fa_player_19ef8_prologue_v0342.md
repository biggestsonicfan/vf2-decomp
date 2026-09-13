# v0342: fa_player `0x19ef8` bit-5 prologue — measured, recovery deferred

## Verdict

Measurement-only. The selector-bit14 clrbit block is fully
measured and the mutated path rejoins warm EXACTLY (985-step
identity diff, +5 insert only). But the parked snapshot's floats
are degenerate: warm faults identically at `0x2705C cvtri`
after ~980 steps, so the corridor tail (~670 steps) and final
counts/state are unprovable from any available park. Shipping
the (small, known) recovery would accept 670 unmeasured steps.
Keep fail-closed until a live-valid player-entry park exists.

## v0309 drive reproduced

`punch-b5` (player word `0x20`) under `vf2cycles --input 16`:
0 compared blocks, fail at `0x16464`, checkpoint
`out/v0342/fail.vf2snap`. NOTE: that drive sets the `+0x0`-word
bit 5. No clearing mechanism for `+0`-word bits was found anywhere
in the prologue (it clears `+0x1a4` bits in r7 only), so the
v0309 drive shape itself remains unwitnessed and correctly
fail-closed. The productive target is `+0x1a4` bits 5/6/21.

## Prologue mechanism (reference, `out/v0342/clrbit*.jsonl`)

From `player-14288-rt` (parked at `0x19ef8`, fighters
`0x510980`/`0x512980`, g7 = F0) with `--set-ip 0x14288`,
`g0 = 0x4505` (synthetic selector: `0x505 | 0x4000`),
`F0+0x1a4 = 0x20`, 20 steps to `0x19f44`:

```text
call → prologue stores → clrbit 9 (+0 word) → ld +0x1a4 (0x20)
→ cmpobe → setbit 11 (+0 word) → bbc 14 nt (bit set)
→ clrbit 6/5/21 (r7, register-only, NO store-back)
→ mov 1 → stib g7+0xbe4 → 0x19f44
```

Memory verified: `+0x5cc = 0`, `+0x60c = 0`,
`+0 = 0x80000002 → 0x80000802`, `+0xbe4 = 1`.

## 985-step identity (warm vs mutated)

`warm-full` (980 steps, same park, no mutations) vs
`clrbit` (20) + `fault` (965): difflib shows EXACTLY ONE
difference — the 5-step clrbit insert
(`0x19f30/34/38/3c/40`). Every branch (six-bbc `0x19f98`
chain testing g0-13/r7-6-5-23-21, `0x1a018` g0-15,
`0x1a210`/`0x1a270` g0 compares, `0x1a0b8` r7-5) takes the
same direction. r7/g0-bit14/`+0xbe4`/sticky-`+0x1a4` cause
zero divergence over 985 steps.

## Downstream-equality findings

- `0x1a034 and 0x1fff, g0, g0`: ROM masks the selector itself,
  so downstream (setup call `0x1a044`, `+0x1a8` store, table
  walks) operates on `0x505` exactly. Native would need one
  masking line; the C `+0x1a8` raw-selector store and raw-index
  table reads otherwise mismatch (`0x4505` vs `0x505`).
- `+0x1a4` memory bit stays set, but `0x1a234` copies it to
  `+0xbd4` (no branch — the C setup already models this copy
  generically via `old_state`), and `0x1a33c` overwrites it
  with record-derived `0x200` (same computation both paths).
  Later re-reads see identical values.
- `+0xbe4` stib-1 is transient (C writes final 0; ROM tail
  past the fault unobserved — see blocker).

## Blocker: park degeneracy at `0x2705C cvtri`

Both traces fault identically (`out of bounds`/`memory fault`,
float-to-int on garbage from the parked scratch state).
v0294 only confirmed fighter bases from this park, never a full
corridor. No other snapshot parks at player entry (fifth/sixth/
punch/v296 parks are all wait-loop parks). A live-valid park
(PUNCH-state floats at `0x14288`) is required to measure the
tail, lock counts (`1652 + 5 = 1657` predicted, UNVERIFIED),
and prove final registers.

## Frontier ranking (fresh traces)

`frontier.py` on `clrbit-fwd` + `fault`: shared-path edges
dominate (`wit=2`), hot loop `0x26f20-0x26f74` (0x26ef0 scratch
expansion), calls `0x1a044→0x1a1e4`, `0x1a0c4→0x26ef0`,
`0x1a0c8→0x27130`. The top unsupported-final candidate is the
`0x2705C` fault itself — park artifact, not game logic.

## Exact native recipe (queued, do NOT implement yet)

1. Entry gate: admit `selector == 0x4505` (measured only).
2. `+0x1a4` gate: if bit14, require `⊆ {5,6,21}` instead of `== 0`.
3. Mask a local `selector &= 0x1fff` before setup/`+0x1a8`/counts.
4. Prologue `+5` (bbc executes in both directions; the insert is
   3×clrbit + mov + stib).
5. Prove from a valid park: full count, final state, unit.

## Proof of correct fail-closed

- Wrapper still rejects everything unmeasured (v0309 drive fails
  identically before and after this slice — no code changed).
- PUNCH / input-17 / ctest untouched.
