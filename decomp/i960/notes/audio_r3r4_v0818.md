# v0818 - R3+R4: worker arms measured, walk driver found (boot, forever)

Status: 5 new dispatch legs + FF-into-boot leg green; two executor
gaps closed (CLR Dn, word/long memory ADDQ/SUBQ) with ROM-byte
vectors and able-to-fail proofs. `src/m68k` changed (executor
only, no game semantics). R4 closed structurally; R3 closed
except the twin/third/tail C question, which is answered below
by declaring it moot.

## R4: who drives the walk (closed)

+2-fixed byte rescan (the old script carried the +4 base; fixed
alongside, 5th print-trust instance: it also caught phantom
"0x24ba br16", mid-ori bytes, dropped from the join list):

| entry | referrers (verified) |
|---|---|
| walk-HEAD 0x2242 | boot 0x21e, epilogue 0x266a/0x2672. NOTHING else. |
| drain-HEAD 0x7a6 | walk 0x2248/0x225c. drain-MID 0x7a8: ZERO (orphaned). |
| prescaler-TRUE 0x21ba | L2 0x1f36 jsr (the "bodge" was +4: +2 lands the TRUE entry). |
| epi-write 0x265c | 11 verified (drain exits, screens, arm exits). |
| epi-init 0x266e | 0x2662 beq only ($16-gated, live both ways). |
| desc/arith/twin-B | 0x2264/0x22c6/0x25f6 respectively. twin-A + third by fall-through. |

- Reset vector: SP=$5000, PC=0x100. The L3-FF path rejoins BOOT
  there (soft reset, not halt — v0813/v0817 "fault" language
  corrected: OUR stepper stopped at its CLR gap, silicon walks on).
- The walk NEVER terminates: every arm returns to 0x265c and the
  epilogue loops (advance or re-init). No rts/terminal exit in
  0x2242-0x2680. Main loop = boot -> walk forever; L1/L2/L3
  preempt it. Production driver = boot (only).
- L2 chain decoupled: L2 -> prescaler-TRUE (countdown + ring file
  + rts, legs 4-5) -> $3000 slots. Walk ($2000 records) shares
  NOTHING with it (different window, different driver). The
  a0==a2 question is resolved negatively: a0 is parked by boot/
  epilogue/drain-restore, never by L2.
- Screen polarity (both measured): screen1 (0x227c/0x2282)
  clear->done / set->worker; screen2 (0x22aa/0x22b0)
  clear->worker / set->done. Opposite, both pinned (R3c).

## R3: worker arms + executor gaps

New legs (`vf2_m68k_dispatch_probe`, all green):
- twin-loop fault @0x22e8 @21 steps (writebacks pinned).
- arith-body fault @0x23fa @44 steps (ram[$2001]=$90 pinned).
- screen1-SET -> 0x24be twin join @27 (previously unmeasured).
- screen2-SET -> early 0x265c @21 (previously unmeasured).
- drain-HEAD push path -> 0x265c @99 (count consumed, cursor
  0->4, a4 re-parked, A0<-saved-D7 = 0, stack balanced).
- FF-reset -> boot clear exit 0x17c @278308 (exact loop
  arithmetic 16165 + 4x65536 - 1; two poisoned cells cleared;
  d0-d2 zero; a7=$5000; ev=3).

Executor gaps closed (both on live paths, both ROM-vectored,
both proven able to fail by revert-rebuild):
- CLR Dn (stepper had no handler; decoder CLAIMED support): N=0
  Z=1 V=C=0, X untouched. Needed by FF->boot (0x10c+).
- ADDQ/SUBQ W/L to memory modes 2/3/5/6 (old code byte-only):
  full silicon for admitted modes via existing RMW + flags_add/
  flags_sub. Needed by drain exit (0x804). Mode 4/7 stay refused
  (fail-closed, not on paths).
- Decoder 'b' size letter for CLR (printed 'l'). Non-Dn CLR
  still refused in the stepper while the decoder prints support:
  noted residual, out of slice.

## Headline: the worker region is silicon-unreachable

$48d2 = movem.l R->M to (An): illegal DESTINATION on Motorola
(bit10=0 with ea mode 2 — architecture, re-verified, not a
fixture artefact). Every twin/third/arith copy loop executes a
legal (a1)+ movem and then this form on iteration 1. So EVERY
path through 0x22b4+ traps on real silicon, values
notwithstanding. Third arm / twin-B / tail are unreachable
(their sled traps first). Production envelope, measured:

- byte1 == 0: pad -> screen1 (clear->done; SET->worker->trap).
- byte1 == $80: pad -> screen2 (clear->worker->trap; SET->done).
- byte1 outside {0,$80}: straight to worker->trap.
- i.e. production needs byte1 in {0,$80} AND $0409 CLEAR,
  always; $0409-bit7 SET on an active record traps (vector 4 ->
  0x4d4 code 8 -> $4fe signal loop, byte-verified chain).
- Consequence: twin/third/tail C recovery is MOOT (S10 scope
  cut). The v0808-v0810 "crashes" are structural tripwires for
  inconsistent state, firing only if authored/corrupt records or
  a set $0409 ever reach the walk — a production-state question
  for S1/S8, not a code path to recover.

## Process rules added

- Rebuild after EVERY checkout/apply cycle before running
  anything (3rd stale-binary incident this run: snapshot,
  decoder-unit, and now dispatch-ctest).
- Byte-verify scan hits before citing (0x24ba phantom).
- Register bases before event expectations (the $0408/SCSP
  correction stands: $0408(a5)=$100408, model-silent).

## Remainder (was v0815 S1-S10, revised)

- S1 record authorship: narrowed — only {0,$80}+clear records
  are production-viable; authorship matters less now.
- S7 main dispatch + S8 $0409 semantics + S9 L2 firing: open,
  unchanged. S8 decides whether the screen-SET tripwire can
  ever fire in production (MIDI live?).
- R5 voice-scan C: kazuyette's, untouched.
- S10 C recovery: walk/drain/epilogue/prescaler only; worker
  region explicitly excluded (silicon-unreachable).
