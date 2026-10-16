# E1p: 0xf50 reaper-found recovered, wired into the setup author (9/10 arc)

Status: reaper scan + found body in C, exhausted tail keeps
the E1o 0xf50 refusal, new `test_reaper_found` differential
green in-file (whole binaries, 0 FAIL). Full suite in the
commit validation.

## What 0xf50 actually is

Not (only) a reaper stub. ROM layout from the disassembly:

- `0xf12`: cursor scan for +2 == 0. Found: cursor
  write-back, bra 0xfac (trampoline to 0xa8c). Exhausted:
  bra 0xf50.
- `0xf50`: SECOND scan, from the 0x2800 base (not the
  cursor) for +9 == 0. Found (0xf6a): slot key writes,
  voice+2 = 0, ram[0x2000+slot] = 0, age rebalance,
  $151e--, bra 0xa8c into the setup author. Exhausted
  (0xf68): rts, packet held.
- `0xfb0`: +9 == 0x80 variant (+9 = 0x20 on found, no
  voice+2 write). UNREACHABLE: no absolute, word-absolute
  or PC-relative branch in the entire 512KB audio program
  image targets it (Python sweep of epr-17574.30 in true
  byte order; the one 00000FB0 longword at 0x1703 sits
  inside a `btst` encoding). Stays unrecovered with this
  checkable claim, not a vaguer "no path reaches it".

## Wiring (descriptor threading)

The reaper-found path needs the drain descriptor (the
0xa8c author splits HIGH/LOW on desc[2]), so `descriptor`
threads through `run_setup_scan` + `run_allocator_0f12`
into new `run_reaper_0f50`, which calls the existing
`setup_author_at`. No dispatch-entry signature changes;
all statics. The 0xa92 head reloads d4 from the found
voice slot, so the author sees core-fill entry state --
the E1n walk-index verdict (0xf12-found arrivals) is a
different provenance and stands untouched.

## Differential

`test_reaper_found`: 90-HIGH drain (a8c-hi shape) with
Loop-B disarmed (no +9==0xff/+5==0 voice: miss,
$152e 5->0), allocator starved (all +2 = 0x40: exhaust,
cursor 0x2800 untouched), reaper fed (voice0 +9 == 0,
slot 0x0020 even). C and S both run drain -> Loop-B ->
0xf12 -> 0xf50 -> 0xf6a -> 0xa8c -> HIGH author ->
sample tail -> 0xcb2 in 678 steps with full-RAM (drain
exclusions) + full-SCSP equality, zero events, packet
held. Voice0 lands 58/00/ff/80 on both sides
(+2 = 0x58, +5 = 0, +9 = 0xff, +3 = filed $1515);
$151e 0->0xff (wrap, stepper-pinned); $152e ends 1.

## Incidental proof the reaper runs

Wiring it broke the E1o alloc-x leg first: that fixture
left +9 == 0 everywhere, so the reaper correctly found
voice0 and authored it (handler 0xf50 -> 0xcb2) while
silicon still stopped at 0xf50. Staging all +9 = 0x40
restored the true exhaustion (S still 246 steps,
unchanged -- the scan prefix is +9-agnostic). A
differential that catches its own fixture gap is working.

## Remaining sound work

0xf12-found arrivals (E1n refuse, stands); 0xfb0 variant
(unreachable, proven above); 0xf50-exhausted rts return
semantics (beyond the stop -- the pin is arrival-only);
0xb76 LOW fill reachability; live gameplay writers;
DSP-absence sweep.
