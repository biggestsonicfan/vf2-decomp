# v0853: the sound-drain interrupt (vector 15) -- live service

Status: closed slice. The real ROM ISR drains the live queue to the
UART data slot; the first live writer event is measured. The 8c
"no live writer event" and the 8f "the drainer has no live callers"
premises are corrected below. New primitive
`vf2_session_service_sound_drain` (`src/app/session.c`), new ctest
`vf2_session_sound_isr` (4 legs), the 8d hunt driver's watch window
fixed to 0x01c80000 plus an opt-in service mode. `src/` beyond the
new primitive and the `stepped_native` guard: unchanged.

## 1. The measured architecture

**Arms**: the queue filer `0x43888` (v0746) pokes `[0xe80004]` with
0x21 then 0x421 when it accepts a command. Bit 10 is the sound line;
0x21 is the off state the ISR's own empty pass leaves behind.

**Services**: external interrupt vector 15 -> handler `0xe10`:

```asm
0xe10  lda      0x00e80000, r14
0xe18  ldl      (r14), r4      # r4 = request, r5 = enable
0xe1c  bbs      10, r5, 0xe24  # enable bit 10 -> service
0xe20  b        0xe30          # unarmed path: call 0x7fc0/0x7ff0, spin 0xe58
0xe24  call     0x000437d0
0xe28  ret
```

**Drainer** `0x437d0` (the handler's single call target; falls
through into `0x43800` after the ack loop):

```asm
0x437d0  lda   0x00e80004, r6
0x437d8  addo  31, 2, r3        # r3 = 0x21
0x437dc  st    r3, (r6)         # disable the line (write x2)
0x437e0  st    r3, (r6)
0x437e4  lda   0x00e80000, r3
0x437ec  lda   0xfffffbff, r4
0x437f4  st    r4, (r3)         # ack: request &= ~0x400 (preserves all other bits)
0x437f8  ldl   (r3), r4         # reload request/enable
0x437fc  bbs   10, r5, 0x437f4  # re-check
0x43800  ldib  0x00504014, r5   # byte index (signed)
0x43808  cmpible 0, r5, 0x43854 # >= 0 -> send path
0x4380c  ldob  0x00504001, r3   # count
0x43814  cmpobe 0, r3, 0x43884  # empty -> ret WITHOUT re-arm (the disarm)
0x43818  subo  1, r3, r3        # count--
0x4381c  stob  r3, 0x00504001
0x43824  ldob  0x00504002, r3   # rcur
0x4382c  ld    0x00504020[r3*4], r4   # ring entry
0x43834  st    r4, 0x00504010   # stage it
0x4383c  mov   15, r4
0x43840  addo  1, r3, r3
0x43844  and   r4, r3, r3       # rcur = (rcur + 1) & 15
0x43848  stob  r3, 0x00504002
0x43850  mov   2, r5
0x43854  lda   0x00504010, r3
0x4385c  ldob  (r3)[r5], r4     # byte = staged[r5]
0x43860  lda   0x01c80000, r3   # UART data slot -- the ROM-immediate address
0x43868  stos  r4, (r3)         # 16-bit store {byte, 0}
0x4386c  subi  1, r5, r5        # 2 -> 1 -> 0 -> 0xff
0x43870  stib  r5, 0x00504014
0x43878  lda   0x00000421, r3
0x4387c  st    r3, (r6)         # re-arm (write x2)
0x43880  st    r3, (r6)
0x43884  ret
```

So: exactly one byte per service, three per popped ring entry
(`staged[2]`, `staged[1]`, `staged[0]`), re-arm only after a store.
The empty pass (`count == 0` with index 0xff) returns without a
re-arm: the line self-disarms at 0x21. The oracle never runs any of
this: the frame loop injects vector 12 only.

## 2. Measurement A -- one probe service (vf2probe, park-sixth-a6c0)

Before: count 3 / rcur 0 / wcur 3 / byte index 0xff / enable 0x421
(armed) / request 0 / ring `[0x00ae101f, 0x009e0a7f, 0x00ae101f]`.

```
vf2probe --rom-dir roms/vf2 --snapshot out/park-sixth-a6c0.vf2snap \
  --raise-irq 0x400 --enter-interrupt 15=1 --until 0xe28 \
  --max-steps 4000 --trace --memory-trace
```

37 instructions to the handler's ret; access trace exactly the
listing above (reads 0xe80000/0xe80004, the doubled 0x21 stores,
the ack, the pop, the staged byte fetch at `0x504010+2`, the
`stos` to `0x01c80000`, byte index 1, the doubled 0x421 stores).
After: count 2 / rcur 1 / staged 0x00ae101f / index 1 / enable 0x421
/ request 0x00000000 / `[0x01c80000]` low byte 0xae. **First live
UART writer event.** Register clobber (r4=0x400, r5=0x421, r6=0,
r14=0xe80000) is interrupt-service state; the parked code is
untouched.

## 3. Measurement B -- the full drain (session fixture)

From the same snapshot, servicing while armed until the ROM's own
empty pass disarms: **10 ticks, 9 bytes**, in order:

```
ae 10 1f | 9e 0a 7f | ae 10 1f      = ring entries 1..3, staged[2..0]
```

Bus-level pins (observer, measured): 9 UART stores (size 2, address
0x01c80000); **38 enable stores = 20x 0x21 + 18x 0x421** (each
service disables x2; each byte-sending service re-arms x2; the idle
service only disables); 10 request acks (0xfffffbff); and **289
retired instructions** across the 10 services (38 for a pop+send,
26 for a mid-entry send, 19 for the idle pass). End state: count 0 /
rcur 3 / wcur 3 / staged 0x00ae101f / index 0xff / enable 0x21
(disarmed) / request 0.

## 4. The primitive

`vf2_session_service_sound_drain(session, report)`:

- refuses `VF2_ERROR_UNSUPPORTED` on a session the recovered native
  runtime last advanced (sticky `stepped_native` guard: the service
  runs raw interpreter instructions on the session CPU, verified
  only on interpreter-driven state);
- unarmed line -> `VF2_OK`, zero work (`armed = 0`) -- the measured
  hardware negative;
- while armed: raise request bit 10, enter vector 15 level 1, run to
  the interrupted IP (step cap 4096, tick cap 80); a tick that does
  not return to the interrupted IP, a cap, or a byte-array overflow
  refuse `VF2_ERROR_UNSUPPORTED` fail-closed;
- after each tick, an armed enable readback is exactly "one byte was
  sent" (the drainer re-arms only post-store); the byte is read from
  `0x01c80000`.

Host-driven, never implicit in `step` -- the pump's contract.

## 5. The fixture -- ctest `vf2_session_sound_isr`

- **leg 1 (live service)**: the 9 bytes + 10/3 counts + the 289
  instruction pin + all bus pins above + queue end cells; then the
  disarmed second service is a pinned zero-work no-op (no CPU, no
  bus).
- **leg 2 (pump equivalence)**: `vf2_session_pump_audio` on a fresh
  session from the same snapshot emits the identical bytes and queue
  cells (9/3/0/3); the one measured difference is pinned: the
  shadow-CPU drain stops one tick before the ROM's empty pass and
  leaves enable 0x421 instead of 0x21 (the filer re-arms on the next
  accept either way).
- **leg 3 (drain-point convergence)**: session A drains at the park
  then advances frames; session B advances one frame to the vector-12
  boundary, drains there (nested above the pending frame -- the
  ISR's ack preserves request bit 0), then advances. Completed-body
  work-RAM digests are identical (`d2a == d1b`, `d3a == d2b`;
  0x05ba692d then 0x8c911cd2 on this corpus): the drain point does
  not perturb the frame.
- **leg 4 (native refusal)**: sixth-fresh stepped natively ->
  service is `VF2_ERROR_UNSUPPORTED` with zero instructions and zero
  bus traffic.

## 6. Corrections to the hunt record

- **8c ("no live writer event") is void on both counts**: (a) the 8d
  hunt watched 0x009c0000, a table-resident lead; the drainer's own
  `lda` stages 0x01c80000 (0x43860); (b) the oracle services no
  interrupt but the frame's, so the writer never ran anyplace the
  hunt looked.
- **8f ("the drainer has zero callers")**: the census counted `call`
  instructions. The live caller is an interrupt-vector entry; the
  only static `call 0x437d0` is the handler's own 0xe24.
- The 8d driver's window is fixed to 0x01c80000 and it grows an
  opt-in `service` argv that exercises this path end-to-end across
  frames (frame 0 drains the pre-existing queue; later frames show
  0 new writes in every gameplay state measured so far).

## 7. The gameplay-writers frontier (measured pointers)

The sink side (file -> arm -> drain) is now proven live end-to-end.
The source side is unchanged and remains the 9/10 item. Measured
pointers for the next slice:

- the flusher `0x43abc` (per-frame sound sequencer task body,
  dispatched via `callx (r4)` at 0x10dc8, frame 17 at boot with
  task record 0x515d80) gates on `byte[0x500f00] == 1` first, then
  0x508000 bit 15. At every parked state measured (boot,
  sixth-fresh, park-sixth) `byte[0x500f00]` is **0** and 0x508000 is
  **0x8a00** (bit 15 set) -- so the closed gate is 0x500f00, and the
  T4b "both closed" reading needs correcting to "0x500f00 closed".
- the per-fighter driver `0x43950` always calls the dedup 0x439ac
  then the filer 0x43888 when it runs -- no additional gate. Zero
  filer arrivals were observed in gameplay hunt frames, so the
  unlocated piece is what sets the flusher's event bits (task-record
  `[g13]` bits 30/27/28/29, selecting the 0x0201e940/980/9c0
  tables) or opens `[0x500f00]`. Locating the setter is the next
  slice.

## 8. Reproduce

```powershell
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug -R vf2_session_sound_isr --output-on-failure
# one live service by hand, probe form (measurement A):
build\Debug\vf2probe.exe --rom-dir roms/vf2 --snapshot out\park-sixth-a6c0.vf2snap `
  --raise-irq 0x400 --enter-interrupt 15=1 --until 0xe28 --max-steps 4000 --trace --memory-trace
# live frames with the drain serviced at each boundary (driver form):
build\Debug\vf2_session_uart_tests.exe roms\vf2 out\park-sixth-a6c0.vf2snap 0 0 3 service
```

## 9. Validation performed

Strict native build (`VF2_WARNINGS_AS_ERRORS=ON`) clean. Full ctest
suite green on the final rebased tree: 192/192 in 1387.53 s.
Focused entries `vf2_session_sound_isr` (5.61 s),
`vf2_session_uart_probe`, `vf2_session_audio_plumbing/pump/pump_staged`
and `vf2_session_queue_hunt` all pass. A first run on the same
binaries was aborted by a host-wide stall (~226 s: four unrelated
binaries that started at t=0 all pinned at 241.0-241.3 s, and all
three timeout victims pass solo in 12-13 s) -- environmental, not
code; the clean re-run above is the record. The fixture arms are on
the gitignored snapshot corpus.

ASan (`build-san`, MSVC `/fsanitize=address`): the configure-time
line `VF2_ENABLE_SANITIZERS=ON: MSVC AddressSanitizer
(/fsanitize=address)` and the `clang_rt.asan_dynamic-x86_64.dll`
dependency were both verified on the rebuilt binaries. Full suite
191/192: the one failure is an environmental timeout on
`vf2_native_twelfth_dispatch` (unrelated to this slice -- solo ASan
39.7 s, ctest re-run 38.21 s, its `n=11` sibling passed in-suite at
42.21 s, and the run was ~1.8x off the documented dominating-test
baseline). Every session/audio/board entry passes under ASan,
including `vf2_session_sound_isr` (1.51 s).
