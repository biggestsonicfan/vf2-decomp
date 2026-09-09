# fa_coli `0x225cc` reachability prefix — v0288

Measurement-only. Re-runs the v0282 drive that reaches the previously
unreachable resolver `0x225cc` (174 blocks) and records the actual warm
path. Decides whether a compact prefix leaf is recoverable.

## Drive

From `out/coli-225cc-entry.vf2snap` (parked at `0x225cc`, mutations
from v0282: `g7+0x1a4 = 0x100`, `g7+0x820 = 1`, dest slot
`0x5149cc = 0xffff`):

```text
vf2probe --snapshot out/coli-225cc-entry.vf2snap \
  --until 0x000230b8 --max-steps 500 --trace
```

- **248** instructions from `0x225cc` to `0x230b8`
- **4** procedure calls / **4** returns
- final `ip = 0x230b8`, status ok

## What the path actually does

The v0282 note described a short prefix (`g7+0x1234` counter++,
optional `call 0x18bd4` when `g8+0x19f == 22`). On this drive
`g8+0x19f != 22`, so the `0x18bd4` shortcut is **not** taken. The
measured path is a large multi-branch body:

| range | role |
|-------|------|
| `0x225cc..0x225f0` | counter++, `ldob g8+0x19f`, `cmpobne 22` not taken |
| `0x225f0..0x226f8` | flag tests, `ldis`/`scanbit`/`setbit`/`shlo`/`or`/`mulr` |
| `0x226f8..0x22e3c` | long `bbc`/`bbs` cascade over fighter fields |
| `0x22e3c` | `call 0x230d4` (nested helper) |
| `0x22e40..0x22e90` | second `call 0x23238`, `setbit`/`st` |
| `0x22e90` | `call 0x1ab34` (loop over `ldob`/`cmpobe`) |
| `0x22e94..0x230b8` | float `subr`/`mulr`/`addr`/`divr`, multi-word stores |

Call targets: `0x230d4`, `0x23238` ×2, `0x1ab34`.

## Verdict

**DEFER.** The measured path is not a compact prefix. It is a
multi-block body with four nested calls and dozens of conditional
branches. Recovering it as a single native leaf would mean recovering a
large fraction of the 174-block function at once — explicitly out of
scope per the recovery plan.

The `0x18bd4` shortcut remains unmeasured (requires `g8+0x19f == 22`).
A future drive that forces that byte to 22 could still yield a small
leaf; this drive does not.

`0x225cc` does **not** enter the warm PUNCH pin, so deferring it does
not affect the 9214/18/19 corridor.

## Proof

- Guest trace 248 steps, 4 calls, final `0x230b8`
- No C recovery in this commit
- No snapshot/trace/ROM data is committed
