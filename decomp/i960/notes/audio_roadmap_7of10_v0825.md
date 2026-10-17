# Audio 7/10 plan: ROM-driven keyed voices to WAV

Current bar (6/10): boot attaches on real ROM; voice-tick/upload
re-recovered; first render audible; Loop-B/walk/L2 live circuits
mapped; voice-start/trigger open.

## 7/10 exit criteria (all must hold)

1. Loop-B's live caller(s) identified by xref + stepper (no
   "unmatchable feeder" fiction).
2. Loop-B head+scan+match arm (0x902/0x932/0x95a) recovered in C
   with a stepper differential (RAM + SCSP bitwise, step-pinned).
3. Setup arm (0x9ba scan + 0xa8c voice authoring incl. the
   mulu/lsr/not volume math) recovered with a differential.
4. A ROM-driven sequence (no synthetic staging except the sample
   bytes themselves) renders keyed voices to a non-silent WAV.
5. Full suite green, no new WILL_FAILs, no proprietary artifacts.

Explicitly NOT required for 7/10: game-loop-driven music
in-session (that's 8/10), the $1526 dance (unless it gates
audibility -- see risk 3), DSP, FM (proven absent).

## Slices (dependency order)

### S1. Loop-B caller hunt (oracle-only, no C)
Static xref of 0x902 over the image + stepper entry from each
candidate caller with production-shaped registers. Question:
who loads d4 (0x60c3e6 index) and files $150e, and when?
Suspects: the 0x90 dispatch arm, the L2 worker paths, the drain
switch fall-through. Output: caller address(es) + entry-state
recipe, or an honest "unmapped" (blocks S2's fixture, not S2).
Gate: stop-pc/step pins like the V3a legs.

### S2. Loop-B C recovery (head + scan + 0x95a arm)
New C entry (NOT inside the maintain_streams no-op -- that
stub's "never sequences" claim is now suspect and gets
revisited here, not silently extended). Differential vs
stepper from the S1 entry recipe: $1515 write, 0xf gate,
$152e gate, scan match/miss, SCSP key-off writes, +5 = 5
authoring, +5 rebalance, $152e--, exit to 0xa8c vs 0xf12.
Gate: triangle-style RAM+SCSP differential.

### S3. Setup-arm C recovery (0x9ba + 0xa8c)
Descriptor authoring ($1515/$1510/$150e bytes into the voice),
$1516 tempo word, +9/+5 seeding per 0xf gate, $152e/$151e
bumps, volume math (mulu.w/lsr/not chain -- pin the d5/d6
formula byte-for-byte against the stepper, it's pure
arithmetic). Gate: differential from the S2 exit state.

### S4. ROM-driven WAV (control-complete)
Chain S2+S3 C with the V2 uploader and V3c render on real ROM
bytes; samples from an attached window (zeros where banks are
absent -- the WAV proves CONTROL: keyed slots, envelopes,
mixing; timbre stays pending). Gate: non-silence + voice-count
+ determinism (render twice, identical).

## Risks

1. **Sample banks absent locally** (mpr-17570..73 not in this
   checkout). Real-data timbre can't be proven here; S4 runs
   zero-filled windows and says so. Falsifies nothing, blocks
   only timbre claims.
2. **S1 may dead-end** (caller via unmodeled interrupt or
   main-CPU event). Then S2/S3 fixtures use hand-built
   entry states (like every V3a leg) and 7/10 criterion 1
   becomes "entry recipe documented, caller open" -- the bar
   still advances on 2-4.
3. **$1526 dance** (0x49e: ror.b, writes $1526 + $400001 port).
   If S2/S3 differentials show an unexplained $1526 delta, the
   dance gets pulled into S3; otherwise it stays open past 7/10.
4. **maintain_streams no-op** may be wrong (its "68k never
   sequences" rationale predates the live-circuit findings).
   S2 re-examines it; if the 0x902 flow IS the sequencer, the
   stub is replaced, not patched.
   (v0852: **resolved** — the stub is replaced by measured C:
   0x1f16 head pokes + prescaler/worker countdown decrements +
   clean refusal on the two unmeasured live bodies (beat emit /
   stream-end, worker applier). The "never sequences" rationale
   was right about reachable states (boot never arms 0x3000) and
   wrong as fail-closed behavior (armed records were silently
   accepted). See `audio_l2_stream_tick_v0852.md`.)

## 8/10 sketch (not this plan)

Session audio_ready with real banks, game-loop packet flow
(main-CPU -> drain -> switch -> Loop-B) proven in-session,
host plays recognizable music. Needs: trigger path (S1's
production answer), real banks, V3b naming.
