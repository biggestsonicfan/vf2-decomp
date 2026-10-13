# v0809 - P2 bulk arm: same illegal store at 0x22e8 (twin crash sites) (+4-era: crash-site pcs reached through word branches; R3 re-visit pending)

Status: stepper-proven. No `src/` change (decoder fix was v0808).
Twin tails still unreached.

## Measured

With desc[0x0a] in {0x0400, 0x0201} (above the 0x22c6 `bls` gate),
entry 0x2242, desc[0] = 0x80, desc[1] = 1, zeroed tables, SCSP
cleared: 34 true instructions, then UNSUPPORTED at **0x22e8**.
Pinned in loopb F7: 8 prefix HITs (steps 27-34) + crash verdict on
both countdown values.

0x22e8 holds byte-identical content to the v0808 site 0x23fa
(`48 d2 00 ff`, verified by direct ROM read at both addresses):
movem.l reg->mem with (a2) dest — illegal ea, vector-4 class.
The v0808 decoder fix prints both honestly (`.word $48d2`).

## Twin crash sites, one form

| arm | gate | trap | steps |
|---|---|---|---|
| d5 = desc[0x0a] > 0x200 (bulk) | 0x22c6 bls not taken | 0x22e8 | 34 |
| d5 <= 0x200 (arithmetic) | 0x22c6 bls taken, 0x23b6 beq not taken | 0x23fa | 45 |

The walk's "movem bulk-copy pairs" (v0799i interior) are built on an
unexecutable store: every iteration's second movem is illegal. The
first (`movem.l (a1)+,$00ff`, mode 3, legal) executes fine —
0x22e4 @ 33 and 0x23f6 @ 44 both commit 8 longs from the (zeroed)
pointer before the crash.

## Reachability: explicitly open, not claimed

Arming needs all of: walk invocation (L2 worker -> 0x2244, whose own
liveness is a documented boundary), a descriptor with bit7 set in
desc[0] and desc[1] outside {0, 0x80}, $0409-bit7 clear at screen
time. Descriptor authors (drain Austin paths, boot inits) have not
been audited for desc[1] contents — that audit plus L2-driver
liveness is the remaining P2 work. The zeroed-table fixture proves
the crash shape, not production reachability. No "game crashes"
claim is made.
