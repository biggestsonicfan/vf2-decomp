# v0812 - Full L2 tick crashes at record 1 (end-to-end, measured)

Status: stepper-proven (loopb F10). No `src/` change. This slice
closes the L2-liveness question as far as the oracle can take it.

## End-to-end verdict (pinned)

Entry 0x1f16 (vector 26), preempted a0 = 0, zeroed records, count 0:
25 true instructions, UNSUPPORTED @0x7bc, a7 unbalanced (mid-call).
Map pins: 0x1f36@7 (jsr site), 0x21bc@8 (prescaler bodge entry),
0x2242@11 (walk via bodge bne), 0x2248@13 (record 0 empty),
0x265e@16 (epilogue via Z=1 fallthrough), 0x7a8@14-before-16
(drain-mid), 0x7bc@25 (illegal move.b-An via Z=0 re-entry).

## What the tick proves

1. **Prescaler entry is a bodge that always forwards.** The L2
   `jsr 0x21bc` lands mid-instruction (ori.b/ori.l/d0-shaping
   bodge); d0 is forced nonzero so the exit bne to the 0x2242 walk
   is unconditional. The 0x21ba "tst/subq gates" decode is phantom
   alignment (byte scan: no branch in the image targets 0x21ba;
   the gates never execute on this path).
2. **TRUE-head entry is safe, mid-head re-entry is fatal.**
   Record 0 via TRUE 0x2242 loads desc[0] fresh (Z=1 on empty) and
   exits clean to the epilogue. Every later record enters at MID
   0x2244 (both epilogue jumps target it), skipping the load, so
   N/Z are pure residue — deterministically Z=0 from the epilogue
   ori.b (mem|8 is never zero). First empty record after the
   epilogue therefore takes the drain bodge and dies at 0x7bc.
3. **The epilogue reset arm is dead.** 0x2662 `beq` needs Z=1 from
   an ori.b that always sets Z=0. No branch targets 0x266e. The
   walk can only loop records (0x266a) until it crashes.
4. **0x7a8 is mid-drain, flag-coupled.** It bypasses the 0x7a6
   count head; its bne re-tests alien residue. Z=1 (fresh load)
   exits to the epilogue; Z=0 (ori residue) takes the bodge.

## Production paradox (open, bounded)

A boot-state L2 tick halts the sound CPU (fault spin, interrupts
masked). Working hardware must avoid this through at least one of:

- (a) L2 never fires in the live configuration (mask/routing
      unmeasured — but then the whole prescaler/walk is vestigial);
- (b) records populated before the first tick so no empty record
      is met (0x90 allocator authorship + order unmeasured — but
      populated + desc[1]!=0 hits the F6/F7 crash sites instead,
      and populated + desc[1]==0 needs the $0409 screens);
- (c) $0409-bit7 routinely set, diverting every screen to safe
      exits (meaning unmeasured; §4 below);
- (d) the TRUE dispatch/drain entry is elsewhere and 0x7a6 is
      idle-poll only (no in-image caller found; register-indirect
      entry unexcluded).

## $0409-bit7 hypothesis (next slice's target)

Byte 0x409 sits in the SCSP MIDI status/MIBUF area (MAME word 4/5
class): plausibly the top bit of the oldest MIDI input byte, i.e.
"a MIDI STATUS byte (0x80+) is pending" vs idle/data. The walk
screens on it in three places; the epilogue WRITES sibling $0408
(MIDI OUT?). If music traffic keeps it set, every crash arm
diverts safe. Needs: L3 ISR (vector 27 -> 0x526) mapping + the
main-CPU MIDI write path + SCSP $0408/$0409 model semantics.
The repo MIDI model (FIFO at 0x404/0x405 only) does not cover it.

## Byte-scan audit (method note)

The "no caller" claims behind v0811/v0812 were re-proven with a
byte-exact static scan (all 0x6xxx branch forms + jmp/jsr
absolute/PC-relative over the word-swapped image), after finding
the first scanner missed short bcc. Remaining blind spot:
register-indirect calls (jsr (An)) — stated, not hand-waved.
