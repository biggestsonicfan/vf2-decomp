# E1r — reaper-exhausted completion: the 0xf68 `rts` consumes the command (v0825)

Scope: closes the one open item from the E1p note — the `0xf50` reaper's
dbf-exhaust exit was an **arrival-only pin**. E1o stopped the stepper at the
`0xf50` arrival and the C refused there with the packet held. This slice
measures the return and admits the measured behavior.

## Result in one line

On a fully busy voice table (allocator exhaust, then reaper exhaust), the 90
command **completes consumed and dropped** — no voice authored, no SCSP
traffic — and the C now returns `VF2_OK` so `dispatch_next`'s consume block
models the drain tail (`count` 0, ring cursor 0 -> 4).

## Drain structure (stepper-measured, true ROM addresses)

The drain entry is at `0x7a6` (`move.b (a4),d0` with `a4` = 0x2506):

- `0x7b4 subq.b #1,(a4)` — **the count is decremented at drain entry**,
  before any dispatch, not at completion.
- `0x7d6 lea $3000(a6),a0` — `a6` = 0x1000, so `a0` = **0x4000**: the 20
  command descriptors, stride 0x10 (`moveq #19,d7`).
- Per matching descriptor (`0x7dc btst #7,$0000(a0)` + `0x7e6 cmp.b
  $0001(a0),d0` channel match): `0x7ee movem.l $8190,-(a7)` + `0x7f2 jsr
  $0000081a`, the command-handler switch. **Return point 0x7f6**; the scan
  resumes at `0x7fa` for the remaining descriptors.
- Tail: `0x804 addq.w #4,$1508(a6)` + `0x808 andi.w #$fff` (ring cursor),
  `0x812 movem.l (a7)+` + `0x816 jmp 0x265c` (drain exit).

## The three phases (cdispatch leg `test_allocator_exhausted`)

Fixture: 90-LOW (`93 / 05`, desc[2] = 0x05), ring cursor 0, count 1, all 32
voices `+2` = 0x40 (allocator exhaust) and `+9` = 0x40 (reaper exhaust).

1. **Phase 1** (E1o, unchanged): drain entry to `0xf50` arrival — 246 steps.
   State at arrival: `count` = 0 (the entry `subq`), ring cursor 0, `a7` =
   0x7ee8 (the `jsr 0x81a` return still on the stack).
2. **Phase 2 (E1r)**: reaper scan, 32 x (`+9` != 0), dbf exhausts at
   `0xf64`, `0xf68 rts` pops the `jsr` return — **lands at 0x7f6 in 131
   steps**, `a7` = 0x7eec.
3. **Phase 3 (E1r)**: the descriptor scan resumes with no writes
   (descriptors 1..19 do not match), `0x804` advances the ring cursor
   **0 -> 4**, `0x816 jmp` — **stops at 0x265c in 84 steps**; `count` 0,
   no bus events.

## C admission

`sound_board_run_reaper_0f50`'s exhaust branch: `VF2_ERROR_UNSUPPORTED` ->
`VF2_OK` (handler pin stays `0xf50`). `dispatch_next`'s consume block already
models exactly the drain tail (`count - 1`, `cursor + 4 & 0xfff`) — the same
shape E1c's key-on arm (0xed8) has used since completion. This is not
fail-open: the path was measured to completion first, and the unmeasured
boundaries (allocator-found `0xfac`, found-engine `0xcb2`/`0xdf8`) are
unchanged and still refuse.

The pump therefore counts an exhausted command as dispatched, matching
silicon's silent drop of a 90 command on a fully busy voice table.

## Test strengthening

The alloc-x leg compared C vs S with `count`/`cursor` **excluded** (the E1o
held-packet shape). Both sides now agree on them, so the exclusions are
dropped — full RAM + full SCSP compare, stack region (`>= 0x7e00`) only
excluded (the 68000 frame is not modeled in C).

## Pump cursor accounting (found while wiring the admission)

Admitting the exhaust as a completed dispatch exposed a **latent pump bug**
from T3. `vf2_session_pump_audio` dispatches each filed entry with
`vf2_sound_board_dispatch_next` and then advanced the board ring cursor by
+4 **unconditionally** ("consume even refused packets"). But the consume block
inside `dispatch_next` already advances it on every OK return, so an OK entry
advanced the cursor **twice** — silently skipping the next filed entry.
Latent since T3 because no live snapshot had ever produced an OK dispatch
(the park pin is 0 dispatched / 3 refused).

Fix: the pump advances only when `dispatch.handled == 0` (the report field set
by `dispatch_next`'s consume block, so it already discriminates). Refused
entries keep the T3 skip rule; consumed entries keep exactly the one advance
the consume block performed. Net contract: **exactly one +4 per processed
entry either way**.

## Staged pump differential (`vf2_session_audio_pump_staged`)

A queue staged by `vf2probe` with **zero steps run** (`--until` = the parked
IP 0xa6c0, so the restore is pure mutation) on top of park-sixth-a6c0:

    vf2probe --rom-dir <roms/vf2> --snapshot out/park-sixth-a6c0.vf2snap \
      --until 0x0000a6c0 \
      --set-u8  0x00504001=2          # i960 queue count
      --set-u8  0x00504002=0          # rcur
      --set-u8  0x00504014=0xff       # parked byte index
      --set-u32 0x00504020=0x00be0000 # ring[0]: packet BE 00 00
      --set-u32 0x00504024=0x009e0a7f # ring[1]: the live 90 packet
      --output-snapshot out/park-staged-2entries.vf2snap

- Packet `BE 00 00`: b0-class, entry `0x3e` != `0x3a` → the 0x13cc table
  default consumes with **no side effects** (guaranteed OK); channel 0xE
  matches ROM descriptors B04/B05 either way.
- Packet `9E 0A 7F`: the park ledger's 90-class entry, refused on the
  init-state board (the T3 pin's shape).

Expected drain: 6 ticks / 6 bytes / 2 filed. Outcome pins: **1 dispatched +
1 refused**. The failure mode is sharp: with the old unconditional advance
the 0x9e entry is skipped, the zeroed ring at +8 is read as class-0x00
(consume), and the report flips to **2 dispatched / 0 refused** — verified on
the unfixed build before the guard was applied (CHECK failed on exactly those
two pins; the park leg stayed 3/0/3 throughout).

The leg skips (77) where `out/park-staged-2entries.vf2snap` is absent — the
same local-artifact rule as the park leg itself; regenerate it with the probe
command above.

## Accounting note for the remaining held paths

The drain decrements the count at entry (`0x7b4`) on the real CPU, so for the
still-held paths (`0xcb2`/`0xdf8` dead loops) silicon's count is one lower
than the C's held-packet model, and the ring cursor stays put on both sides.
That divergence is deliberate: the C keeps the packet for a retry/decision
while the original hangs in a loop it never exits in shipped gameplay. Do not
"fix" it by decrementing on refusal — a later retry would then double-consume.
The consume-on-completion paths (key-on, gate-hit, and now reaper-exhaust)
all match exactly.

## Validation

- `vf2_m68k_cdispatch_tests` (ctest #24 / #113): green with the new phase-2/3
  pins and the strengthened compare — `s-rts pc=0x007f6 steps=131 a7=0x07eec`,
  `s-tail pc=0x0265c steps=84 count=0 cursor=0004`.
- `vf2_session_audio_pump` (ctest #21): unchanged live pin — 9 bytes / 3 filed
  / 0 dispatched / 3 refused (the guard is behaviour-preserving on refusals).
- `vf2_session_audio_pump_staged` (ctest #22): 6 bytes / 2 filed /
  1 dispatched / 1 refused; the pre-fix build reported 2 dispatched /
  0 refused.
- Full suite: **191/191 green** (1701.60 s), including the new
  `vf2_session_audio_pump_staged` entry; a targeted post-build re-run of the
  18 affected/churn entries (session_audio, m68k, tile, endurance, block_hash,
  probe_help) is also green.
