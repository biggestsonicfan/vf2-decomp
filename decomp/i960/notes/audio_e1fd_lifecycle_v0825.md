# E1f-d: L1 lifecycle closed on the round-1 voice (9/10 arc)

Status: five-tick C-vs-stepper composition differential green
in-file (whole binary, 0 FAIL). No `src/` change -- the tick
was already proven (triangle S1-S5); this proves the
composition on a natively-produced state. Full suite in the
commit validation.

## Composition

E1f-c round-1 fixture replicated (0x80/0x05, HIGH desc,
voice0 andi-witness, voice1 +2=0x40/+3=0x90/+4=0/+6=0x00ff/
+0xb=5, voice2 early-miss): dispatch keys + arms L1
(+2=0x41). Then five `maintain_voices` ticks, each a fresh
differential on the evolving state (triangle convention:
0x5fe entry, rte stop, a7 balanced, RAM below/above the ISR
stack zone + SCSP equal):

| tick | C post | S steps |
|---|---|---|
| 1-4 | timer 4,3,2,1; +2=0x41; no expiry; SCSP live | 138 |
| 5 | timer 0; expire 1/1 normal; +2=0; SCSP[0+0xb]=0x1f | 343 |

Only voice1 ever ticks (v0 bit0 clear post-andi, v2 bit0
clear); the zero-table rebalance is a no-op both sides.

## Drafting lesson

First draft snapshotted for S AFTER the C tick (then
rationalized rewinding in a comment instead of fixing the
order). Rewritten before running: snapshot pre-tick state,
C advances, S replays. The comment that explains away a
wrong order is how a wrong differential gets committed --
the E1b fixture rule (S runs from the PRE-dispatch image)
already settled this; same rule, per tick.

## Arc status

E1f-a/b/c/d complete: 0x80 row -> scan -> sub (4 arms) ->
L1 countdown -> expiry -> key-off. The full one-shot
lifecycle (setup -> key -> countdown -> release) is now
native end-to-end except audibility of the middle (the 0x80
key arm primes 0x10 without KEYONB; the ed8 RMW keys but
lives on another path -- their combination is unmeasured
and explicitly not claimed).
