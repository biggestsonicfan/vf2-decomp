# v0345-A: native `0x7fc0` byte-expand leaf + direct unit

## Verdict

First piece of the `0x22960+` continuation campaign: `coli_7fc0_body`
+ `vf2_hybrid_coli_7fc0_execute` recover the byte-expand leaf called
from all three continuation sites, proven by a direct unit (three
measured shapes + empty-string + guard control). Unwired (callers
still fail closed); wiring comes with the `0x9444`/`0x9450` inline
spans and the site-B prefix next.

## What the code does

Entry with caller globals (`g0` = src byte pointer, `g9` = dst
short pointer). Copies NUL-terminated bytes, ORs each with `0x8000`
and stores shorts to `(g9)+=2`. Real call/ret callee: locals die at
ret, `g0`/`g9` pass through untouched, completion pops the frame
via `hybrid_complete_procedure` (which counts the ret itself and
tallies the return — body ends at the `be`-taken edge, `(body,0,0)`).

## Key findings

- The leaf consumes the `0x502a4` copy output: call 1 reads the
  site-A buffer (`0x503200` = `6f 00`, helper-exit `g0` flows
  straight into `r3`) and writes `6f80` to `0x010007de`.
- `shlo` operand order settled for good: the executor computes
  `operands[1]<<operands[0]`, i.e. second-displayed <<
  first-displayed. `shlo 15, 1` = `1<<15` = `0x8000` (store bytes
  `6f80` force this); `shlo 2, 25` = `25<<2` = 100 (the v0344-B
  negative control forces this — a `0x64` byte takes the first
  `cmpobe`). Both proofs agree; an earlier mid-turn scare was a
  misremembered operand order, not a code bug.
- Class audit: every `shlo` modeling in `hybrid.c` (`shlo 24,17`
  = `17<<24`, `shlo 2,25` = `25<<2`, game-info/SHRO tails proven
  by exact-match units) already uses ROM operand order. No changes
  needed. Future models: always verify shift direction against a
  taken-edge control, since all-nt inputs mask order bugs.
- `g9` changes per call site (`0x010007de` → `0x0100085e` via the
  `0x22968` lda → `0x010006e8` downstream): per-site constants,
  which is why the helper takes `g9` as input rather than baking
  the table in.

## Provenance (`out/v0344/cont.jsonl`)

- Call 1 (trace 198-214): 2 iters, 16 steps, 1 store.
- Call 2 (trace 220-372): 19 iters over link data `0x22978+`
  (19x `0x20` + NUL at `0x2298a`), 152 steps, 18 stores of `2080`.
- Call 3 (trace 423-495): 9 iters over `0x23d50` (`68 69 74 20
  20 20 20 20 00` = "hit     "), 72 steps, 8 stores.

## Unit (`test_coli_7fc0`)

Exact step deltas (16/152/72), return IPs, +0/+1 calls/rets,
exact store halfwords, empty-string shape (8 steps, no store),
300-byte no-NUL guard control (`UNSUPPORTED`).

## Queued

- `0x22964` inline + `0x9444` head + `0x9450` tail (straight-line
  + 5-iter `cmpobl` scan loop + `bx`) in the coli flow.
- `0x2298c` warm-join coverage check (likely native already).
- Site-B prefix (`0x22bd8`→`0x22e04`: `cmpobe`/`bbs`/`bbs`/
  counter/`bbs` + `lda`/`lda`/`st`/`mov` + balx) + helper call +
  fail-after, with a full-path wrapper unit. Note this prefix
  sets `g9 = 0x0100085e` (same constant as the `0x22968` lda).
- The `0x2298c+` code must also account for `g0`/`g9` globals
  (`0x23d50`, `0x010006e8` flow into call 3) — check how (or
  whether) native join code tracks them before wiring call 3.
