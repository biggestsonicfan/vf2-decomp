# T3: session bridge wired (trigger plan)

Status: `vf2_session_pump_audio` + host-loop call + pump ctest
leg green. Full suite in the commit validation.

## Design

- Shadow CPU: the drainer runs on a stack-local cpu
  (ip = 0x43800, r6 = live contract) sharing the session
  machine; the session CPU is untouched. 9 ticks drain the
  park queue exactly (cap 48 = full ring).
- Loop while count > 0 or bidx in 0..2 (partial staged word);
  stop on first drain/file error with `drain_status` recorded
  (0xff reboot door holds the rest queued). Internal errors
  never fail the frame -- the report carries them.
- Dispatch: each newly filed entry runs `dispatch_next` once
  at the board read cursor; the cursor advances past EVERY
  outcome (silicon drain-epilogue rule: consume even refused
  packets). 0xc68-door-with-effects counts as dispatched,
  earlier doors as refused.
- Host-driven, never implicit in step: pumping mutates
  IO-control + queue RAM, so auto-pump would perturb oracle
  digests. host_sdl calls it post-step/pre-render and logs
  only when bytes moved (idle = 2 reads + break).

## In-session result (park queue, init-state board)

bytes=9 filed=3 dispatched=0 refused=3 ticks=9. The 90-class
entry misses without a staged voice ($152e door) and both a0
entries refuse by design -- the pump never stages voices, so
0/3 is the CORRECT init-state outcome. The admitted 90 shape
lives in the T2 differential (S4 arming), not here. Render
determinism holds; peak 0 reported.

## Left open

- T4 flusher bit-setter hunt (gameplay filer callers).
- T5 8/10 claim: needs a gameplay trigger + an admitted
  live voice in-session (engine keying still oracle-side).
- L1/L2 maintenance in the host loop (game_update refuses
  sessions by guard design; maintain_* stay unit-proven).
