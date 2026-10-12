# P0c/p1d-A0 follow-up — test_sound_board.c:105+ model-test regressions

## Status

**Not a v0803 regression** — this note documents the 15
pre-existing sound_board-test assertions that remain broken
under the v0801-v0803 working tree and gates them as a
follow-up slice.

The triangle differential gate (`vf2_m68k_triangle_differential`,
ctest #95, **7/7 PASS**) is the **silicon oracle** and is
unaffected. The broken tests are unit tests against the
recovered C's behavior, not silicon-vs-C comparisons.

## What's failing

`tests/hardware/test_sound_board.c:87-129` is the
`vf2_sound_board_maintain_voices` unit test. It pins very
specific expected values:

```c
EXPECT_TRUE(maintenance.voices_scanned == VF2_SOUND_VOICE_COUNT);  /* 32 */
EXPECT_TRUE(maintenance.voices_expired == 2u);
EXPECT_TRUE(maintenance.normal_expirations == 1u);
EXPECT_TRUE(maintenance.release_expirations == 1u);
EXPECT_TRUE(maintenance.voices_aged == 2u);
EXPECT_TRUE(maintenance.counter_151e_before == 2u);  /* was 2 */
EXPECT_TRUE(maintenance.counter_151e_after == 1u);   /* decremented */
EXPECT_TRUE(maintenance.counter_152e_before == 3u);  /* was 3 */
EXPECT_TRUE(maintenance.counter_152e_after == 2u);   /* decremented */
EXPECT_TRUE(voices[0][2] == 0u);
EXPECT_TRUE(voices[1][2] == 0u);
EXPECT_TRUE(voices[1][5] == 0u);
/* …plus specific RAM bytes post-key-off writes */
```

These were pinned to the OLD C, which:

1. **Per-iteration indexing**:
   `voice = voices + index * VF2_SOUND_VOICE_BYTES`
2. **Skip-on-key-zero for every iter**: `if ((voice[2] & 1) == 0) continue;`
3. **Inner rebalance** after each expiration: for every other voice,
   if `candidate[9] != 0xff && age < candidate[9]`, decrement and
   increment `voices_aged`.
4. **sound_board_key_off on every path** including release.

## What silicon does (and what P0c recovered)

`src/hardware/sound_board.c:vf2_sound_board_maintain_voices`
(M4 silicon, measured `v0799c`) recovers the silicon's actual
behavior:

- The `adda.w #$20,a0` at the loop tail is jumped over by skip
  bodges (`beq -> mid-adda ori.b`), so `a0` never advances. **All
  32 iterations visit voice 0.** Voices 1-31 are never serviced.
- The loop traps on the inner rebalance at `PC 0x1640`
  (`adda.l #imm32,a1` decode fail). The silicon partial state is:
  `voice[0][9] = 0`, the shared counter decrement commits,
  then the trap fires (or the next iteration begins with stale
  state).
- The release-arm (`voice[2] & 8 != 0`) writes that depend on
  residue `d1` are unknowable from the C's view; the silicon
  reaches them via `bne-at-0x632 -> 0x686`, bypassing the
  `move.w (a0),d1` at `0x684`.

The recovered C mirrors silicon: frozen pointer, skip only on iter
0 (the s3-skip), remove inner rebalance, fail-closed at the loop
body (return UNSUPPORTED after the partial-state commit on the
first expiration it sees).

## What the silicon oracle says

Triangle `s1-clean` and `s3-skip` are pin-cases for exactly this
behavior:

- `voice[0][9]` is decremented once per silicon iter (32
  iterations, all on voice 0).
- Triangle `s3` is the load-bearing case: `voice[2]=0,
  voice[0xb]=0x28` → `0x09` after 31 decrements across iters
  2-32 (with the iter-1 skip). The OLD C would skip iter 0
  entirely (because `voice[2] & 1 == 0`) and fail to match the
  sililcon's `0x09`. The new C matches.

So **the recovered C is silicon-correct**, the unit test fixture
predates this analysis.

## The follow-up

Closing this requires re-deriving the test_sound_board.c:87-129
fixture against the silicon-equivalent `maintain_voices`
contract. Specifically:

1. **`maintenance.voices_scanned`** — change expected value
   from 32 to **1** (silicon: first iter only, then refused).
2. **`maintenance.voices_expired`** — change expected value
   from 2 to **1** (one expiration then UNSUPPORTED).
3. **`maintenance.voices_aged`** — change expected value from
   2 to **0** (inner rebalance removed).
4. **`maintenance.counter_151e_before/after`** — fixture has
   `counter[0x151e] = 2`, voice 0 normal expires with
   `--counter[0x151e]` → after = **1**. The fixture is
   consistent with that. So these pass once the loop doesn't
   refuse before the decrement.
5. **`voices[1][2], voices[1][5], voices[1][9]`** — never
   reached under the frozen-pointer model; these assertions
   either need to be removed or converted to "unchanged" checks.
6. **`return value`** — change expectation from `VF2_OK` to
   `VF2_ERROR_UNSUPPORTED` (silicon fails closed on expiration).

This is a fixture-derive slice, not a code slice. It is bounded
to `tests/hardware/test_sound_board.c:87-129` and does not
require any further sound_board.c changes. Estimated 30-60
minutes of careful work: read the new maintain_voices
contract, then map each assertion to the new expected value.

## Why this is not P6 work

The decoder rotation fix in P6 was a 4-line change with two
fixture shifts in tests/hardware/test_sound_board.c (e0 voice
tail, ~10 lines each). Those shifts reused the same ROM byte
and same lookup offset, keeping the assertions semantically
identical.

The maintain_voices unit test fixture at line 87 has 15
assertions that all change semantically — `voices_scanned`
goes 32→1, `voices_aged` goes 2→0, return code goes OK→UNSUPPORTED.
There is no shift that keeps the assertion suite pinning the
same observable. It must be re-derived.

That scope (fixture re-derivation, 30-60 min) belongs to the
**P0c/p1d-A0 follow-up** slice because the silicon contract
drives the expected values and the silicon evidence is already
in `src/hardware/sound_board.c:vf2_sound_board_maintain_voices`
(the comment block at line 550).

## Tie-ins

- P0c v0801: `p0c_triangle_validation_v0801.md` — s3-skip +
  s5-release restructuring drives triangle 7/7 PASS.
- P6 v0803: `p6_decoder_rotation_fix_v0803.md` — decoder
  rotation fix + e0 fixture shifts; this follow-up is
  referenced from the v0803 file-scope table as "sound_board
  gate still has 15 model-test regressions; P0c/p1d-A0
  follow-up".
- v0733c/v0734a/v0734b: the executor `arch_fix_direct_compare`
  + functions.csv end-column machinery that v0733d used to
  measure `0x1640` as the rebalance trap site. That evidence
  grounds "silicon traps on the rebalance" here.

## Acceptance criteria for closing this follow-up

1. `vf2_sound_board` (`ctest #7`) PASSES (0 fails).
2. Triangle `vf2_m68k_triangle_differential` (`ctest #95`)
   **remains at 7/7 PASS** (no regression).
3. `vf2_m68k` (`ctest #12`) and `vf2_m68k_step` (`ctest #13`)
   **remain PASS** (no regression).
4. All 15+ broken assertions either re-derived to silicon
   semantics or honestly removed with a comment naming the
   removed silicon state.
5. No `src/hardware/sound_board.c` change (the model is
   silicon-correct).
