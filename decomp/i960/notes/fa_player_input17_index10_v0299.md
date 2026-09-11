# v0299/Fase 6A: phase17 index10 latch under input-17

## Verdict

The cycle-2 input-17 stop at `frame_dispatch_tick` (`0xa6c0`) is **not** an
unknown `callx` family. It is the known selector-17 / phase `0x8a` /
`bit7_index10` handler. The body at ROM target `0x0005f234` is the same
measured state0 path already recovered (`1650` instructions / `31` calls).
Native fails closed only because `execute_frame_phase17_bit7_index10`
overfits three latch equalities to the idle PUNCH pattern `0x0ff7f700`.

## Parks

| File | Role |
|------|------|
| `out/v300/in17-c1.vf2snap` | after input-17 cycle 1 MATCH, ip `0x1645c` |
| `out/v300/in17-at-9ff8.vf2snap` | same cycle, parked at `main_final_cluster` `0x9ff8` |

## Latch dump at the `0x9ff8` boundary (before dispatch)

| Address | Field | Value |
|---------|-------|-------|
| `0x0050002a` | selector | `0x11` (17) |
| `0x005000a4..a7` | phase_index / a5 / a6 / a7 | `0x8a` / `0x00` / `0xff` / `0xff` |
| `0x00500700` | input | `0x0f000000` |
| `0x00500704` | navigation | `0x00000000` |
| `0x00500708` | released | `0x00002100` |
| `0x0050070c` | previous | `0x0f002100` |
| `0x0050002c` | mask | `0x00020000` |
| `0x0005fef8` | target | `0x0005f234` |
| `0x00508000` | board | `0x00008a00` (bit 5 clear) |

At cycle-1 end (`in17-c1`, ip `0x1645c`) the same fields are mid-rotate:

| Field | `c1` (`0x1645c`) | `0x9ff8` (dispatch entry) |
|-------|------------------|---------------------------|
| input | `0x0f002100` | `0x0f000000` |
| navigation | `0x00002100` | `0x00000000` |
| released | `0x00000000` | `0x00002100` |
| previous | `0x0f000000` | `0x0f002100` |

`0x2100` rotates between navigation and released; input/previous swap the
`0x0f000000` / `0x0f002100` pair. This is the match-held latch shape, not
the idle service pattern `0x0ff7f700`.

## Reference corridor `0x9ff8 → 0xa010` (park + `--until 0xa010`)

| Measure | Value |
|---------|-------|
| instructions | **1909** |
| calls / returns | 38 / 39 |
| `callx` at `0xa6f0` | target `0x00010b5c` (selector 17) |
| `0x10b5c` wrapper body | 1668 insns / 32 calls / 33 rets |
| `0x0005f234` body | **1650** insns / **31** calls / 32 rets |
| `0x5f234` call targets | `0x7fc0` (25x), `0x8440` (6x) |
| a5 after return | `0x01` (state0 write) |

The `1650/31` figure matches `execute_frame_phase17_bit7_index10` state0
accounting exactly. Inputs 16, 17 and 18 from the **same** `0x9ff8` park
produce byte-identical traces: the latch is already in RAM, so held host
input does not change this particular frame.

## Why native fails

`execute_frame_phase17_bit7_index10` requires:

```text
input    == 0x0ff7f700
previous == 0x0ff7f700
released == 0
```

Measured siblings:

```text
park (no held input at 0x9ff8):
  input=0x0f000000 previous=0x0f002100 released=0x00002100

vf2cycles --input 17 (the pin path; latch kept held):
  input=previous=0x0f002100 released=0
  navigation==0, a5==0, a6==a7==0xff
  target==0x0005f234, mask==0x00020000
```

The held-input tuple is the one native actually sees on the endurance
drive. Both execute the same `0x5f234` state0 body (1650/31).

Guards that still hold: `target`, `mask`, `a5<=1`, `a6`, `a7`.
`navigation==0` still selects state0.

## Native endurance (pre-recovery)

```text
vf2cycles --snapshot out/v300/in17-c1.vf2snap --input 17 --cycles 2
→ unsupported operation
  34 blocks / 1666 insns
  ref 0x00009ff8 / native 0x0000a6c0
```

## Recovery applied (v0299/v0300)

`execute_frame_phase17_bit7_index10` now admits a second measured latch:

```text
idle  (unchanged): input=previous=0x0ff7f700, released=0
match (new):       input=previous=0x0f002100, released=0
```

state1 (`a5==1`) with the match latch stays fail-closed.

Match state0 accounting and poststate, proven by the live per-block
compare:

| Field | Match path |
|-------|------------|
| instructions | **1677** (1650 `0x5f234` + 27 shell/wrapper) |
| calls | **33** (31 body + 2 wrapper/callx) |
| r14 | 6 |
| AC low bits / CC | `...001` / GREATER |
| header text | `LOSE(%)` at row 13 col 10 (not `LOSES(%)`) |

`set_main_final_cluster_condition` no longer forces EQUAL when
`phase_index==0x8a`; the bridge poststate (GREATER) is left intact.

## Pin (closed)

```text
vf2cycles --snapshot out/v300/in17-c1.vf2snap --input 17 --cycles 1
→ 1/1 MATCH
  37 blocks / 3810=3810 insns
  both at 0x0001645c
```

`in17-c1` is the park after input-17 cycle 1 from `sixth-regen`. This
closes the **second endurance pin**, complementary to PUNCH.

A second cycle from the same park still fails closed at the next
`main_final_cluster` (now `a5==1` / index10 state1 with the match
latch) — the next measured sibling.

## Pins observed

- PUNCH **320/320 MATCH** / 12,946 blocks / 14,962,620 insns
- **input-17 cycle from `in17-c1`: 1/1 MATCH** / 37 blocks / 3,810 insns
- ctest Debug **56/56**
- No snapshot/trace/ROM data committed.
