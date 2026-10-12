# Slice-1 verdict: Loop-B + body chain are dead code — P1b evaporates (v0804)

## Verdict

The Loop-B voice allocator (0x902) and the entire downstream body
chain (0xa8e voice-record fill through 0xc6a voice-start trigger and
KEYON at 0xc8e) **cannot execute on a stock 68000 and have no live
caller**. The P1b recovery scope (200-400 LOC, v0735 plan) evaporates:
there is nothing to recover. The C's refuse-before-Loop-B is COMPLETE,
not a frontier. No `src/hardware/sound_board.c` dispatch change in
this slice.

This supersedes `p1b_body_chain_boundary_v0802.md`'s "future slice"
framing and `audio_voice_alloc_map_v0799c.md`'s "90/80-body chain
recipe (post-refusal, for a future slice)": the recipe correctly
*decodes* the bytes but the code is unreachable, so the recipe must
not become C.

## Evidence chain (four independent legs)

### 1. Stepper trap at the first dbf (ctest #96 `vf2_m68k_loopb_probe`)

`tests/m68k/test_loopb_probe.c`, wired in `CMakeLists.txt`:

- F1 (direct scan entry 0x92a, voice[9]=0): 6 insns execute
  (lea, move.w, cmpi, bne-taken, ori.b #$10,d0 sled, dbf-taken),
  then `VF2_ERROR_UNSUPPORTED` at pc `0x934` — the imm word of
  `cmpi.b #$ff,$0009(a1)` (`0x00FF`: ORI.L with an invalid extended
  ea, illegal on a stock 68000).
- F3 (full Loop-B head 0x902, `$150e&0xf`=3, `$152e`=5): head runs
  clean (9 insns), scan traps identically (15 insns, pc `0x934`).
- Gate proven: planted pc `0x935` fails; reverted green.

### 2. Silicon would run its own crash handler, and VF2 audio works

Exception vectors 2+ all point at `moveq #N,d0; bra $4fe` stubs;
vector 4 (illegal instruction) -> `0x6004d4`. The `0x4fe` common
handler disables interrupts, writes the error code to the hardware
port, and spins forever. Executing `0x00FF` halts the sound CPU, and
shipped VF2 plays sound. These bytes never execute. (Disassembly via
`vf2m68k disasm roms/vf2 0x4d4/0x4fe`; file offsets, word-swap
`load16_word_swap` accounted for.)

### 3. No-caller xref over the whole image

Byte scan of the audio region (CPU view) for every word-branch
(bra/bsr/bcc.w), `jmp (d16,PC)`, and absolute jsr/jmp with targets
in `0x880..0x1100`: every even-source hit is an internal Loop-B/body
edge already mapped; the only external hits are `0x82e->0x10d2` and
`0x868->0x10e4` (both the 80 path). Two odd-source hits (`0x6593`,
`0x8c7`, `0x8c9`-class) are data false positives. No `0x4EFB`
(indexed jump) exists anywhere in the image. The measured B0 table
has 18 singles, none in range (per `audio_b0_table_map_v0799.md`).
Nothing calls `0x884/0x8e0/0x902/0x92a/0x95a/0xa8e/0xba0-0xc6a`
or the `0xf8a/0xfb0/0xfee` sibling scans.

### 4. The HIGH arm is unmatchable from every modeled state

Loop-B's head is reachable only via the 90-HIGH arm, which needs
`desc[2] >= 0xf0`. The drain matches the word `desc[1..2]` against
`channel <= 0x0f`, so a drain-matched descriptor always has
`desc[2] <= 0x0f`. C0 mutation writes `desc[2] = cmd|0x80 <= 0xCF`.
Boot images zero the descriptors. No modeled writer produces
`desc[2] >= 0xf0`. The 90-LOW arm exits at `0xf14` without touching
Loop-B.

### Corroborating (decode-inferred, second source: none yet)

- The scan's beq arm (`0x942->0x95c`) lands mid-move and sleds
  through `ori.b #$bc,d0` into `ori.b #$0b,(a7)+`, which misaligns
  the supervisor stack: death by address error at the next word
  stack op. The scan is dead on all three exits (bne/dbf via the
  `0x00FF` trap, beq via stack corruption).
- Bcc.w/dbf/bra prints are CORRECT in the current decoder
  (raw-verified: `bpl $2262`, `bra $0f14`, `dbf $0934` all match
  hand-computed targets). The mid-instruction entries are real
  bytes, not print skew. Contrast the jsr/jmp-(d16,PC) -2 below,
  which was print/exec skew and is now fixed.

## Instrument fixes made along the way (all gated)

1. **jsr/jmp-(d16,PC) base pc+2 -> pc+4** (`stepper.c:step_jsr_jmp`,
   `decoder.c` 4EBA/4EFA arm). Raw proof: `0x2248` disp `-6820`
   lands `0x7A8` on silicon; both tools said `0x7A6` ("mirrors the
   decoder" comment removed). Drain entry print label is now
   `$7a8`; the C model is unaffected (it never executes). Repinned
   fixtures: `test_decoder.c:145` (`jsr $00000002`), the
   `test_stepper.c` jsr disp (`+4` -> `+2`, stale offset comments
   fixed), probe F4 stop `0x7a8`. Triangle 7/7 unaffected (no 4EFA
   on its paths).
2. **ori/eori mode-1 ((An)) admitted** (decoder arm + `step_imm_op`
   allow-list + `ea_rmw_addr`). Needed by the walk sled
   (`ori.b #$40,(a2)` at `0x2256`). andi/subi/addi/cmpi stay
   refused until a path executes them (documented in code).
3. **ROR-register immediate admitted** (`step_ror_reg`; ROXR stays
   refused). Needed by the walk sled (`ror.l #8,d1` x10, raw
   verified `e0 99` x10). X untouched, V cleared, C per rotation.
4. **Env-gated pc trace** (`VF2_M68K_TRACE`) was added to the run
   loop, used to map every path below, then REVERTED before
   commit. The committed probe keeps pins only.

## Walk status (P2 lead, mapped this slice)

Stop-pc sweeps (same ctest) with an ACTIVE stream via the head:

- `0x224e` HIT/2, `0x2260` HIT/7, `0x227c` HIT/19, `0x2282` HIT/20,
  `0x265e` HIT/21. The timer check (`0x2254/0x2258/0x225c`) is
  bypassed by the ori.b sled (never executes); the ror chain, the
  `$0409(a5)` btst/beq (SCSP zeroed, exits), and the `0x265e`
  epilogue (`ori.b #$8,(a6)`, beq-never-taken, `adda.w #$20,a0`,
  `jmp $2244` slot-stride loop) all execute.
- F4 pins the N=0 arm (mid-head entry `0x2244`, fixture-zero flags):
  bmi not taken, `jmp $7a8`, stops clean in 2 steps.
- Iteration 2+ always takes the drain arm: the bmi tests epilogue
  `ori.b` residue (N=0 stable), not the stream byte. Multi-slot
  interior processing does not happen; each L2 tick processes at
  most the entry slot, then drains.
- The twin tails (`0x24d2/0x260c`) were NOT visited: they need the
  `0x2260`-bne-taken arm (`desc[1]!=0`, fixture has 0). Open,
  owned by the P2 slice.
- Residue coupling (documented, unmodelable without the worker):
  `0x224e` writes `(a4)+`, the sled RMWs `(a2)`, the bmi/d2 paths
  read worker residue. The P2 "preempted register state" boundary
  stands, now with stepper evidence instead of assertion.
- v0797's "+0x0c countdown dead" verdict is a DIFFERENT field from
  the walk timer and does not imply the walk is dead. Do not cite
  one for the other.

## Fixture-hygiene catches (kept as warnings)

- Entering at `0x2244` (mid-head, as L2 does) without the `0x2242`
  `move.b` leaves fixture-zero flags and silently re-tests the N=0
  arm. The first F5 measured the wrong arm; the sweep exposed it
  (identical trap state on every stop). F4's comment now states it
  pins the N=0 arm only.
- The `0x7a6` stop pin passed against the buggy stepper (both
  wrong the same way). Raw-verified to `0x7a8` after the fix.

## Bar consequences

- P1b (body chain) and P1e-via-KEYON are CLOSED as boundaries:
  staged-silence is the terminal correct state for drain-driven
  flow, not a stepping stone.
- P2 twin tails + `desc[1]!=0` walk arm become the lead recovery
  slice (liveness partially proven: head/interior/epilogue run).
- Audit queue (same stepper-trap-or-sled + xref + matchability
  method): 80-LOW mid-lea arm, 90 `beq $10e4` mid-80 entry, A0
  computed-jump bounds, move-src mode-1 gap (open, blocks drain
  tracing past `0x7bc`).
- DSP ROM scan and platform adapter slices unchanged.
- Doc updates (`README.md` bar text, `STATUS.md`, `ROADMAP.md`
  v0.4.0, `UNCOVERED_BRANCHES.md` section 5) deferred until P2
  resolves, to avoid churning twice.

## File map

- `src/m68k/stepper.c`: 4EBA/4EFA base fix, mode-1 RMW address +
  imm-op admission, `step_ror_reg` + gate, trace added+reverted.
- `src/m68k/decoder.c`: 4EBA/4EFA print fix, ori/eori mode-1 arm.
- `tests/m68k/test_loopb_probe.c` (new) + `CMakeLists.txt` wiring:
  ctest #96 (`vf2_m68k_loopb_probe`).
- `tests/m68k/test_decoder.c`, `tests/m68k/test_stepper.c`:
  fixtures repinned to corrected semantics (each fails pre-fix).
- `src/hardware/sound_board.c`, `tests/hardware/test_sound_board.c`:
  slice-1 report-publish fix + sentinel fixture (prior turn).
