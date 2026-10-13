# v0813 - MIDI ring-fill path proof + $0409 inventory + two instrument fixes (SUPERSEDED by v0817 R1: push story retracted, legs re-pinned)

Status: stepper-proven L3 legs (new ctest `vf2_m68k_midi_probe`),
two decoder/stepper fixes with proofs. No `src/recovered/` change.

## L3 ISR (vector 27 -> 0x526): coherent MIDI parser, mapped

TRUE entry (vectored), rte-terminated, no sleds. Reads $0405 MIDI
bytes and files them into the $1500 ring (data $2500, state
$2501-$2503, cursor $2504, count $2506 — shared with the L2 gate
counter): textbook running-status parser ($1503 state; 0xFF
filtered; $F0+ and bit7 arms). Internal branches byte-verified
(0x562/0x582/0x5e4/0x5e8/0x5ee all have in-function sources; the
earlier byte-scan zeros were a scanner bug — short-bcc word forms
use low byte 0x00, not 0xFF — fixed in the audit script).

## Leg 1: MIDI 0xFF takes a reset-ish path, faults in vectors

beq.w $5ee -> push d0 / clear SCSP $401 / reset SP to $5000 /
bra.w $102 (boot vector image as code) -> vector-data walk ->
refuse at $10c. Pinned: UNSUPPORTED @0x10c @10 steps, a7=$5000
(frame abandoned), two pushed d0 copies ($6FFC/$6FF8, both $FF —
the linear 0x5f8-addq print is phantom, mid-lea), ring untouched,
one $400001=0x1f bus event (main-CPU signal). Reading: 0xFF is
MIDI System Reset; the sound CPU parks the stack, quiets the
SCSP, signals $400001, and wanders the vectors to a fault halt.
Preempted-a3/a0 dependence (push/stack targets) is stated open.

## Leg 2: data byte files clean and returns (with shuffle)

$1503 != 0 arm: file d0 -> $2501, push, restore, rte. Pinned:
OK @rte @16 steps, a7 balanced, ring byte + FIFO + cursor/count
exact. BUT the exit restore shuffles registers: save mask $fff8
(D3-D7/A0-A6) vs restore mask $1fff (D0-D7/A0-A4), both
byte-verified, shift every slot by 3 (D0<-D3 .. A2<-A5, A3<-A6,
A4<-A7). Fully pinned (all 16 regs). Either a Sega ISR bug or
benign via main-loop reg hygiene — production impact open (needs
main-loop analysis, not more ISR stepping).

## $0409-bit7 inventory (verdict: no driver writer)

- Reads: the three walk screens (0x227c/0x22aa/0x24c8), all
  HIT-proven TRUE.
- Writes: NONE. The only candidate (epilogue 0x265c -> $0408)
  has no entry (byte-scan: nothing targets 0x2648/0x264e/0x265c)
  — dead code. High-region `.word` hits are data phantoms.
- Value is hardware-presented (SCSP MIDI area); repo model leaves
  it zeroed (screens take crash-ward arms in fixtures). The
  MIDI-status-byte hypothesis (bit7 = status pending) stays a
  hypothesis: consistent with L3's parser and the sibling $0408
  write, proven by nothing. Set-arms unmeasured.

## Instrument fixes (both proven able to fail)

1. **Decoder Dn -> -(An)** (was refused; stepper implemented):
   0x2700-class now prints `move.l d0,-(a3)` etc. Needed by the
   0x5ee push (and 0x5e4). ROM-byte vector; pre-fix binary fails
   exactly the 2 new asserts.
2. **uint32 wraparound in SCSP range checks** (bus_read/bus_write):
   near-$FFFFFFFF addresses wrapped into the SCSP window and failed
   with the wrong code (INVALID_ARGUMENT). uint64 casts matching
   the ROM-mirror checks. Pinned by the push-unmapped control
   (a3=0 -> UNSUPPORTED, not INVALID_ARGUMENT, same pc/steps).
   Found live: the 0xFF path with un-set a3 returned status 1.

## Lesson (third instance)

Linear prints at 0x5f6/0x5f8/0x556 were all phantom (mid-lea,
mid-beq, mid-move). The HIT chain corrected every one, including
killing my own "addq" and "stack reset is phantom" theories mid-
slice. Stop-pc HITs first, prints second — no exceptions.
