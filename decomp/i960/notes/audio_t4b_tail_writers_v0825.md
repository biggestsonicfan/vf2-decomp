# T4b: gate-bypass tail writers close entries 1-2 (trigger plan)

Status: all three park-queue entries have named filers.
Hunt driver gained argv[6] custom stop-IP. Full suite in the
commit validation.

## Correction to T1: the census was 80 + 8

The filer-common tail 0x438ec (queue write WITHOUT the
0x43888 gate checks) has EIGHT direct callers: seven boot
sites (0x13e8, 0x1548, 0x1c54, 0x1e34, 0x201c, 0x207c,
0x20d8) + the 0x437b4 trampoline (itself caller-less,
reached by fall-through). Entries filed here never execute
0x43888, which is why the frames 0-17 filer hunt was clean
while count sat at 2.

## Entry ledger (park-sixth-a6c0 queue)

- Entry 1 (0x00ae101f): boot display/uart init chain.
  0x97xx (color-profile stores) -> `call 0x4372c` (uart
  init: serial-port pokes + queue zeroing) -> falls through
  0x437b4 -> tail. Hunt: stop 0x437b4 hits frame 0
  (242,567 steps, g0 preset ae101f upstream, ret 0x97dc).
- Entry 2 (0x009e0a7f): the 0x009e0a7f boot-init cluster
  (0x1c54/0x1e34/0x201c/0x207c/0x20d8, SRAM-checksum
  neighbors), frames 1-17. Exact site among the five open
  (all file the identical value through the identical tail
  -- immaterial to every downstream claim).
- Entry 3 (0x00ae101f): `0x58eac` immediate filer, frame 18
  (T4 main note).
- Never fired here: the 0x009e0b7f pair (0x13e8/0x1548,
  alternate boot path) and all 80 gated-entry sites.

## Tool note

argv[6] custom stop-IP reuses the full fixture printer
(count/rcur/wcur/staged/bidx/ring0). Stopping AT a call
site (not the callee) captures the already-staged g0 --
used to verify the five 0a7f immediates without running.
