# P1b: body chain stepwise recovery to KEYON — boundary note (v0802)

## Scope

P1b is the recovery of the 68k audio body chain (0xba0 -> 0xbbc
-> 0xbc0 -> 0xbd4 -> 0xc1e -> 0xc6a) that Loop-B (0x902) feeds
on the $150e&0xf / $152e>=5 arms with V2/V3 keys. The chain
ends with the KEYON trigger at 0xc8e. This is a recovery
slice, not a status audit — it would add C code to
`src/hardware/sound_board.c` (or a new file) implementing
each function in the chain.

This note documents the body chain and the proposed recovery
plan. No C-side changes are made in this checkpoint — the
scope (200-400 LOC per the v0735 advance plan, several
ctest entries) is too large to begin without explicit user
direction.

## Body chain functions (per v0799c)

| addr | function | inputs | outputs | size |
|---|---|---|---|---|
| 0xba0 | voice-record fill | a1=voice, a0=descriptor, d4=slot | SCSP[slot+0x0d] (volume) | ~24 B |
| 0xbbc | jsr $1062 | a1=voice | (not yet mapped) | 4 B |
| 0xbc0 | SCSP slot+4/+0x0c/+0x0e init | d4=slot | SCSP[slot+4]=0x1fff, [0x0c]=0, [0x0e]=0 | ~14 B |
| 0xbd4 | SCSP pitch | a0=descriptor, d1=$150e&0xf, d4=slot | SCSP[slot+0x12] = desc[8] \| (0x3220[$150e&0xf]<<5 & 0xe0) \| (0x3240[$150e&0xf] & 0x7) | ~44 B |
| 0xc02 | SCSP envelope | a0=descriptor, d4=slot | SCSP[slot+0x15] = desc[0x0c], [0x16] = ($1511 & 0x1f) \| 0x60 | ~24 B |
| 0xc1e | sample resolution | a1=voice, a2=$5000, d4=slot | SCSP[slot+8] = sample addr long, [0x0c] attr byte | ~30 B |
| 0xc6a | voice-start trigger | a1=voice, a0=sample source, d4=slot | SCSP[slot+0] = sample \| 0x10300000 (KEYON), ram[0x200a+d4] loop start, [0x2001+d4]=0x10, [0x201a+d4] timer, [0x201c+d4] timer+0xeba, 16+ longs movem copy | ~80 B |

Total: ~220 B of M68K code. The C recovery would be
~200-400 LOC (per the v0735 plan estimate).

## Why the chain is not yet in the C

Per v0797: "Loop B (0x902 primer tail, $150e&0xf/$152e gates)
and the 0x1c0a B0-sub-dispatcher + 0x1c24/0x1d5e scans:
post-refusal or dead-entry territory (only B0 0x3a live;
0x1d5e entered once via beq from the 0x7e arm). Future
90/80-voice targets, not this slice."

The C's current dispatch entries (0x80, 0x90) refuse with
VF2_ERROR_UNSUPPORTED before reaching Loop-B. The body chain
is never reached from the C's covered dispatch paths. To
recover the chain, the C would need:

1. A new dispatch path that reaches Loop-B (e.g., a 0x80
   arm that doesn't refuse at the stale-branch-into-mid-lea
   point)
2. Loop-B modeling in C (currently refused as post-refusal)
3. The body chain functions in C

This is a significant scope expansion. The v0799c note
documents the chain but the dispatch path to reach it is
still "post-refusal or dead-entry territory."

## Proposed recovery plan (for user approval)

1. **Identify the dispatch path** that reaches Loop-B in the
   silicon. Per v0795, the 0x80 body's only matchable shape
   takes the low arm (stale branch into mid-lea). Per v0797,
   the 0x90 body's matchable shape also takes a stale branch.
   Loop-B is reached from a different path — likely the
   0x1c0a B0-sub-dispatcher or the 0x1c24/0x1d5e scans.
   Need to identify which dispatch entry reaches Loop-B.

2. **Build a vf2probe scenario** that drives the dispatch
   entry to Loop-B. This requires a snapshot with specific
   voice state ($150e&0xf, $152e>=5, voice[5]=5) that
   triggers the V2/V3 key path.

3. **Recover the body chain functions** in C, one at a time,
   verifying each against the silicon via the
   factory-chain tools (taint.py, infer_structs.py).

4. **Wire the recovered functions** into the dispatch path,
   replacing the current refusal with a call to the body
   chain.

## Gate

The P1b recovery would add 1-2 ctest entries (per the v0735
plan). The acceptance criterion: native FULL MATCH at the
body chain boundary for at least one new function. The
published percentage would move when the first function
is recovered.

## Why I'm stopping here

The scope is too large to begin without explicit user
direction. The P1b recovery would:
- Touch the dispatch path (currently refused)
- Add ~200-400 LOC of recovered C
- Require a new vf2probe scenario
- Add 1-2 ctest entries

This is a multi-day slice, not a single-session edit. The
working tree is clean (P0c-closed, P1a-audited). Ready to
begin when directed.

## Files

- `decomp/i960/notes/p1b_body_chain_boundary_v0802.md` — this note
