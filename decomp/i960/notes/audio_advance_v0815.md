# v0815 - sound advance plan (10 slices, dependency-ordered)

Follows v0814. Each slice exits with: stepper legs (new or extended
ctest), a focused note, and a frontier-status line. Standing rules:
HITs first/prints second, ROM-byte verification of every branch
target, fixture hygiene (v0732p: seed windows with distinct values,
let entries load their own registers), no silent admissions.

## S1. $2000 record-window inventory (small; unblocks S6)

v0814 corrected: walk records live at absolute $2000, L2 slots at
absolute $3000 (v0792), same $20 stride. Open: who WRITES the
$2000 records? Candidates: boot corridor (0x1d4+), descriptor
images (ROM 0x6030e6/0x603186, nonzero byte-1 per v0810), the
walk's self-mutation (desc[1] += $10). Method: byte-scan writers
of the $2000 window + boot-corridor stepping from reset. Exit:
record authorship table + production-shaped fixture bytes for S6.

## S2. Exception signal loop 0x4d4-0x524 (small; independent)

All vector stubs (vecs 24-31) funnel to 0x4fe: fault code in d0 is
signalled on $400001 in an infinite loop (`move.b d0` / delay /
`move.b d2` / delay / bra). Stepper-prove one vector-4 pass
(d0=8) + the loop period; pin the two $400001 writes as bus
events. Exit: production-crash observability contract (what the
main CPU sees when the sound CPU dies).

## S3. L2 slot-gate 0x1f7e + file arms (medium; unblocks S4/S6)

Per-slot gate at the TRUE base abs $3000: bit7 active, +2
countdown, +4 stream pointer, then the high-nibble computed jump
(0x2054 1-byte file / 0x2082 2-byte file / 0x1ffe special) into
the $1500 ring with cursor/count. Fixtures: slot-shaped RAM at
$3000 with stream bytes (distinct per slot, per v0732p rule).
Exit: legs for gate-taken/gate-shut/countdown-expiry + one leg
per file arm with exact ring/cursor effects.

## S4. Main-loop a0 convention (medium; unblocks S5/S6/S7 verdicts)

The load-bearing unknown: what parks a0 for the walk at L2-tick
time? Hypotheses: (a) a0==a2 single-record-per-tick (main loop
re-parks each iteration); (b) boot-parked $2000 persists and L2
ticks walk whatever follows. Method: inventory all low-range a0
writers (v0814 list: 0x1d4 boot, 0x7d6 drain-scan, 0x21a8 clear,
main-dispatch readers) + step the boot->main-loop handoff to find
the steady-state a0. Exit: named convention with stepper proof,
or an explicit smaller open joint. NO fiture-pinned addresses.

## S5. Drain body 0x7b0-0x816 (medium; verdict needs S4)

HEAD entry 0x7a6 is dead (no callers) but the body is measurable:
fixture (a4) != 0, enter at HEAD, pin the push/decrement/scan
($3000 base per v0792), the $3000-scan match loop, consume-exit
and the 0x81a class-switch inventory. The MID-entry overlap arm
is already pinned (v0814 leg 6). Exit: full drain path proof +
statement of which entries are production-live (needs S4).

## S6. Active arms from production state (medium-large; needs S1+S4)

Revisit the v0808-v0810 sites (0x22e8/0x23fa twin arms, 0x23b2
arith, 0x25ee third, 0x260c twin-B) with production-shaped
records (S1 bytes, S4 a0): which crash arms are genuinely
reachable vs fixture artifacts of zeroed/desync state? Each arm:
single-entry stepper leg or explicit refusal with the arming
joint named ($0409 / L2-fire / record-content). Exit: per-arm
reachable/refused verdicts replacing the current blanket
"shape proven, fire open" status.

## S7. Main dispatch 0x12b8-0x13ca (large; needs S4)

Command-byte dispatcher: 0x12e0 mid-instruction overlap arms,
0x1326 gate, 0x13ca computed jump + table targets (0x15e0,
0x164e, 0x16bc, ...). Ties to the v0792 C model (dispatch_a0):
differential-audit C against the stepper arm by arm. Exit: per-
target legs + C-model verdicts (uphold/patch/refuse). Largest
slice; split the table if it exceeds one commit.

## S8. $0409-bit7 semantics (research; blocked on model decision)

No driver writer exists (v0813 inventory); value is hardware-
presented in the SCSP MIDI area. Paths: (a) MAME SCSP MIDI
cross-check (hearsay-grade until measured in-repo); (b) live-MIDI
differential (stage FIFO bytes, watch walk screens take set-arms).
Do NOT invent a set rule. Background priority; S6 proceeds with
both screen outcomes pinned as fixtures meanwhile.

## S9. L2 firing conditions (small-medium; mostly independent)

Static SR-discipline audit ($2000 vs $2700 across ISRs/main loop)
+ IPL mask state + L1/L2/L3 vector-state fixtures. Settles v0810
joint 4 (does L2 fire during the drain?). Can run in parallel
with S3-S5. Exit: firing-condition statement + one fixture leg.

## S10. C recovery: walk/drain/epilogue (capstone; gated on S3-S6)

Replace the maintain_streams no-op with measured C, reusing the
v0755b per-step hook pattern for the interpreted ranges. Gated:
no C before S3-S6 legs are green. Exit: native legs + ROM-backed
differential + frontier promotion.

(v0852: the maintain_streams half is **done** — measured C (head
pokes + countdown decrements + clean refusal on the two unmeasured
live bodies) with five C-vs-stepper differential legs and per-frame
pump wiring; see `audio_l2_stream_tick_v0852.md`. The per-step hook
pattern was not needed: the tick is a board-model call, not an
interpreted-range recovery.)

## Non-goals (closed)

FM music (v0807 absence), MultiPCM (v0805 SCSP), decoder Dn->-(An)
+ SCSP range wraps (v0813), walk dispatch map (v0814).
