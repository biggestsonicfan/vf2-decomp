# v0811 - Drain-head flag contract: both arms die at 0x7bc (measured) (+4-era: HEAD/MID verdicts flipped by v0816; drain-HEAD is live)

Status: stepper-proven (loopb F9) + decoder vector. No `src/` change.
This slice turns the biggest sound question from a suspicion into a
measured verdict with explicit conditions.

## a4 provenance

Two independent sites point a4 at the command count: boot-delay
prologue `0x1d8 lea $1506(a6),a4` and consume-epilogue `0x80e lea
$1506(a6),a4` ($1506 = COMMAND_COUNT 0x2506). a4 is sticky across
the main loop (set at boot, refreshed at every consume) but heavily
reused as a record pointer elsewhere — so "a4 = count at drain
entry" is the best-supported reading, not a proof.

## Drain entries (image-wide target scan)

- NOTHING branches/calls/jumps to 0x7a6 (head) or 0x700-0x7e0.
  Entry is fallthrough from the 0x7900-0x7a6 sled desert (TRUE
  predecessor unmapped) — or nowhere. v0790's "jmp 0x7a6 from the
  sample path" has no in-image witness and is withdrawn as an
  entry claim (its drain BODY mapping is unaffected).
- The only entries into the drain zone are the walk exits
  0x2248/0x225c `jmp $7a8` (MID-head, flag-coupled).
- The TRUE head (0x7a6 `move.b (a4),d0`, 0x7a8 `bne $7b2`
  exact per HITs, 0x7ac `jmp $265e`) splits on the count byte.

## F9 verdicts (production fixture: a4 = $1506, real ROM)

- count=1: s1 move.b, s2 bne-taken, s3/s4 ori bodge, refuse at the
  illegal move.b-An 0x7bc. UNSUPPORTED @0x7bc @4 steps. Pinned.
- count=0: bne falls to 0x7ac epilogue -> record advance ->
  record-2 empty -> re-enter 0x7a8 (Z=0 residue) -> bodge ->
  refuse at 0x7bc. UNSUPPORTED @0x7bc @12 steps. Pinned.
- 0x1508 ROM bytes pinned as decode-refused (length 2, `.word`;
  no decoder change was needed).

## The paradox, stated plainly

From TRUE 0x7a6 with a4 = count, silicon can NEVER dispatch: both
arms die at 0x7bc (a PRM-illegal byte-size An move; stepper
line 756 encodes the same rule, vector-4 class). The dispatch
machinery the C models (0x7e6 scan, `jsr` class switch, 0x800
consume) sits behind the illegal site with no other entry — and
the C drain has NEVER had an oracle differential (standing
C-vs-C caveat). Two consequences:

1. `dispatch_next`'s drain half needs a ROM differential or a
   region correction; until then it is unproven against silicon.
2. If audio works on hardware (it does), the TRUE dispatch path is
   UNIDENTIFIED. Candidates: a register-indirect caller into the
   interior (unfindable by target scan), or a different dispatch
   region entirely. Finding it is now the sharpest sound task.

## Conditions and non-claims

- Verdict is conditional on entry-at-0x7a6 with a4 = $1506. If
  production enters elsewhere (or a4 points elsewhere live), the
  crash shape moves with it — the ILLEGAL SITES don't move.
- The 0x7a6 fallthrough predecessor, L2-fire-during-drain, and
  $0409-bit7 meaning remain open (same list as v0810).
- No "game crashes" claim: what is proven is that THE MAPPED
  DRAIN PATH cannot dispatch from its mapped head, and that the
  C model of it is oracle-unverified.
