# E1j: a0 max-gate micro-admission + join refusal (9/10 arc)

Status: `sound_board_dispatch_a0` head recovery +
`test_dispatch_a0_maxgate` two-round differential + live-leg
repin + two unit-leg conversions green in-file (whole
binaries, 0 FAIL). Full suite in the commit validation.

## Correction: the old residue mechanism was misread

The entry comment claimed the split is `cmp.l d2,d0` with
d2 never loaded (stale-upper mis-split). The ROM bytes say
otherwise: 0x1e90 `move.w (a4)+,d2` loads the row max two
insns before 0x1e92 `cmp.w d2,d0`. d0 arrives via `andi.l
#$ff` (exact), d2's low word via `move.w`, and `cmp.w`
compares low words only -- the stale upper never
participates. The 0x1e74-0x1eac head (10 insns) is fully
deterministic. What stays residue-wild by MEASUREMENT (T2:
history-spins, zeroed-state exits) is the post-join body
(0x1e98+: entry-pointer table into code-as-pointer sled,
v0804) -- the refusal is real, its address and mechanism are
now precise.

## Recovery

dispatch_a0 gains (payload, ring_byte2) -- the S2b 0x90-primer
precedent (a3 sits at ring+2 in class arms). idx =
payload & 0x7f; row = be32(0x608e12 + idx*4); max = row word
(odd/wild rows fail closed: move.w address-error shape).
bgt is SIGNED word: ring_byte2 > max takes the 0x1eac rts
(drain resumes, packet consumes -- handler stays the 0x1e74
head, OK-path precedent); else refuse at the 0x1e98 join,
packet held. The head is register-only (no RAM writes), so
both exits are side-effect-free at the boundary.

## Differentials (synthetic ring, real tables)

idx 0x20 rows max 0x7f:

| round | byte2 | C | S |
|---|---|---|---|
| A 0x80 > 0x7f | OK + consume | 0x265c, 130 steps, full-RAM + SCSP equality |
| B 0x7f = max | refuse @0x1e98, held | 0x265c, 210 steps, NO equality (outcomes differ by design) |

Round B pins the pair of outcomes (T2 style), not an
equality that cannot exist.

## Ripple (all per-evidence, same slice)

- T2 live-queue a0 rounds: pin moves 0x1e74 -> 0x1e98. The
  live ae10-shape reads idx 0x10 / byte2 0x1f against max
  0x1f -- equal, so bgt falls to the join (measured, both
  rounds). Live a0 stays refused; pump pins unchanged.
- Unit legs: synthetic dir+row staged (idx1/idx2 -> max
  0x0002); {0xa0,0x01,0x01} joins (@0x1e98, held),
  {0xa5,0x02,0x03} completes (OK, count 0, cursor 4).
- Table survey (word-swapped ROM, not assumed): idx 0x10 max
  0x1f, 0x1f/0x20 max 0x7f, 0x00 max 3, 0x2e max negative
  (always-rts shape, unlegged), 0x7f dir garbage (OOB,
  fail-closed).

## a0 status

The class is now exactly bounded: over-max completes,
at/under-max refuses at the join, wild rows fail closed.
The join body stays unrecovered (residue-wild). Remaining
sound work: live gameplay writers (all hunts negative) and
whatever the next `go` targets.
