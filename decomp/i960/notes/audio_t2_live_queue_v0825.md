# T2: live-queue drain chained end to end (trigger plan)

Status: `test_chain_live_queue` green (filing FULL MATCH,
round 1 FULL MATCH under S4 exclusions, a0 rounds pinned
refusals, admitted voice + determinism, `out/chain-live.wav`).
Full suite in the commit validation.

## The pivot: menu replay abandoned, live queue found

Top-menu navigation from park-sixth-a6c0 dead-ended honestly:
25 chained bridge frames with the DOWN bit held never move the
index cell (all 863-insn idle legs; the cursor task doesn't
run in this park's legs). The input IS read (6 sites incl.
0xa748 testing bits 2/26 = TEST/enter), but lever-walking the
top menu from this park goes nowhere.

The replacement find: park-sixth-a6c0 holds a LIVE-filed
queue -- count 3, rcur 0, wcur 3, ring {0x00ae101f,
0x009e0a7f, 0x00ae101f}, bidx 0xff -- filed by real game code
on the boot->TEST-MENU trajectory and never drained (the
drainer has no live callers). 0x00ae101f is TEST stop #5.
Nine reference drainer ticks (`--set-ip 0x43800 --until
0x43884`, 23-insn loads + 11-insn sends) emit ae 10 1f | 9e
0a 7f | ae 10 1f and park count 0 / bidx 0xff (sentinel cycle).

## Round results (all live bytes, real-bank board)

- Filing: 9 bytes C==S full-RAM (jsr-spill exclusion only),
  count 3, ring exact.
- Round 1 ({9e}, 90-class, S4 arming): C runs the full
  90 + setup-author path, refuses at the 0xc68 engine door
  with effects applied; S 389 steps to the same door;
  FULL MATCH (RAM + SCSP, S4 exclusions). Admitted leg:
  the LIVE command authors voice0 (+2 = 0x58);
  determinism holds; peak 0 (correct -- no engine keying).
- Rounds 0/2 ({ae}, a0-class): C refuses at the 0x1e74
  entry BY DESIGN (d2-residue, sound_board_dispatch_a0);
  S exits the drain cleanly in 249 steps both cursors.

## Two harness fallacies killed during the slice

1. 0xc68-arrival is NOT a per-entry contract. It is the
   main-loop funnel: same packet at cursor 8 never arrives
   (spins to budget). Stop a0 rounds at the drain exit
   (jmp 0x265c), not the funnel.
2. Sequential rounds entangle residue: the a0 body spins
   when run after the 90-miss zeroes $152e (this
   corroborates the C's history-dependence assessment of
   the a0 path). Rounds restore a pristine filed snapshot.

## Left open (not this slice)

- Menu-lever replay (index walk from a pre-gating park).
- a0 body recovery (residue-dependent by measurement).
- T3 session bridge wiring on a live trigger; T4 flusher
  bit-setter hunt (0x43abc gates: 0x500f00==1, 0x508000
  bit 15 -- both closed in sixth-fresh and park-sixth).
