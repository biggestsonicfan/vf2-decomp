# S2a: drain byte-match + e0 primer correction (v0825)

Status: drain routes on `descriptor[1] == command & 0xf` (was
word-match); e0 head reads payload + ring byte 2 (was command +
payload). Both proven by the new drain-entry differential;
S7b/T1 re-proven under the corrected model; all dispatch
fixtures re-staged (`desc[1]` = channel). Sound gate green
(hardware + triangle + cdispatch).

## The byte-match proof

New leg `test_drain_e0_entry` (test_cdispatch.c): desc[1] = 4 /
desc[2] = 0 discriminates (byte-match routes, word-match
doesn't), C `dispatch_next` vs stepper from the drain entry
0x7a6 to the tail exit 0x1354 (233 steps, T1 segments + SCSP).
RED on the old C (no match, handler 0); GREEN after the flip.
Silicon side alone already ran the full path pre-flip.

## The primer correction (caught by the new leg, not assumed)

S7b staged the head-entry stream as {command, payload} and the
C was written to match its own staging. The drain leaves a3 at
ring+1, so the 0x12b4 primer (`move.b (a3)+,d0` / `move.b d0,d2`
/ `move.b (a3),d0`) really loads d2 = PAYLOAD, d0 = ring byte 2.
Measured: shared 0xfff2 (C) vs 0xfff1 (silicon) with everything
else equal. Fix: `combined = (payload&0x7f)|((byte2&0x7f)<<7)`
with byte2 = ring[cursor+2] (new `dispatch_e0` param, mirroring
`dispatch_90`'s `ring_byte2`). S7b/T1 streams re-staged to
{payload, byte2}; both converge on shared 0xfff1 now (S7b:
205 steps).

## Fixture re-staging (mechanical, intent-preserving)

Rule: `desc[1]` = command & 0xf everywhere a match is intended;
`desc[2]` untouched. c0/A0/B0/e0/upload legs, both B0 table-
default legs, both 0xd3 default legs (with a pointer comment:
whether the default arm SHOULD enter the 90 body is the v0819
divergence, owned by S2b). ch-0 legs needed no change. The v0792
e0 absolute pins re-derived under the corrected head (G1 marker
0xFD -> 0xFF; G2 marker 0x0B -> 0xFE with sign flip to -5;
tails: 0x05 marker at 0xFE, picker/timer markers moved to the
0x1B-slide addresses; values verified green).

## Deliberately deferred to S2b

- `dispatch_90` body + byte selection (head = ring+1, second =
  ring+2 per the same primer logic) and the default-arm
  question. The v0792 90 legs keep stale primer asserts until
  S2b replaces them with its differential.
- Per-arm switch-entry audit (B0's d0 handoff in particular:
  T3a/T3b stage d0 = payload past the switch, so they prove the
  uploader body, not the handoff).
- 80/c0/a0 bodies (refused; untouched).
