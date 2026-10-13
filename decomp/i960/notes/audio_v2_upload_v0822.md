# v0822 - V2: sample uploader re-recovered; boot attaches on real ROM

Status: `sound_board_sample_upload` rewritten to the true
boot/B0-convergent body; T3a upgraded to a real-data OK/OK
full-RAM differential, new T3b synthetic-marker differential
(3 runs), new T4 real-ROM init-attach leg. `initialize()` now
returns OK on the real image -- the session's silent-at-boot
Half of blocker #1 is down (control flow attaches; DATA still
needs the real sample banks, absent locally). No oracle change.

## Init bisect (static, exact)

Every init read is a pure range check, so real-ROM pointers
settle it without running: blob/image1/image2/voice/volume
reads all map; `first_count` = 0x32c7; row = be32(0x608df0) =
0x10060 falls below the audio base -> the single OOB. The old
"silicon traps, translate to UNSUPPORTED" comment misread the
mechanism: boot's real row is picked INDEXED (row 0 = 0x608dfa,
real records), never 0x10060. Scripts: initbisect.py,
bootrow.py, row0.py, row0rec.py (scratch, not committed).

## Boot/B0 convergence (disasm 0x422-0x49c, 0x1ac0-0x1b4c)

Boot 0x422 (first-copy + d0 = 0 + max-gate + indexed pick) and
B0 0x1ac0 (SR + first-copy + 7f arm + max-gate + indexed pick)
converge on one record loop (identical instruction shapes at
+0x356). bgt exits to clean `rts` (0x49c / 0x1b46).

## Six C fixes (all stepper-proven after)

1. First-copy runs d7+1 bytes (`dbf`; old: d7, short one).
2. Record loop runs count+1 records (`dbf`; old: count).
3. Max-gate + indexed pick (`cmp.w d1,d0; bgt`; row = long at
   base+2+d0*4). The old direct-be32(base) read the max word AS
   a pointer; B0 modeled no gate at all.
4. Entry structs at the CONSTANT base 0x605b28 + sel*0x14 (a0).
   The old `table = be32(0x605b28)` mistook entry 0's source
   field for the base.
5. Stored slot word is the FULL `stored - source` long at the
   INDEXED $5008+entry (old: low-byte mask at fixed $6008 --
   collapsed every record onto one address).
6. B0 0x7f arm exits CLEAN via the 0x1b46 epilogue (verified
   beq target; old "mid-dbf trap"/UNSUPPORTED was +4 damage).
7. Sample window follows the ATTACHED size (old 2MB DIRECT cap
   refused real sources at ~0x23e226 past it; the stepper maps
   attached size -- C mirrors it now).

## Legs

- T3a (real row 0, 4 records, 4MB zero samples): OK/OK, 287191
  steps, 2 SR events, full-RAM compare. Control proof with real
  entry structs; the old trap contract retired (row was never
  unmapped, only misread).
- T3b (synthetic dir/rows/entries, real uploader BODY copied
  from the image, marker samples): row1 (entries 1,2 -- stage
  survives, two indexed stored words), overmax (staged only),
  0x7f (epilogue, staged only). 73/21/17 steps, 2 SR events
  each, full-RAM compare + execution-verified C-side marker
  pins. (Lesson: the synthetic ROM must carry the real code --
  first attempt executed zeros.)
- T4: `initialize()` on the real ROM returns OK ($1526 = ef).
- v0792: test_boot_rom carries a sane-minimum upload fixture
  (max 0, row 0, sample-mapped entry 0); B0-7f leg flips to
  OK/consumed; B0-05 leg redesigned (max 5, row 5, entry 0);
  v0794a three-phase leg re-pinned (5th stage byte 0xEE at
  $5004 proves dbf+1 C-side; indexed stored; guard trip kept).

## Self-correction

A long debug detour ("slot=80050004") was my own hex misread:
0x50000's byte 1 IS 0x05 (0x50000 >> 16 = 5). The C was right
throughout; every print confirmed it. Rule restated: compute
multi-byte asserts by execution (vals.py), never mentally.

## Remainder

- 0x49e `$1526` dance (ror.b, writes $1526 + $400001 port) is a
  separate subroutine after the 0x49c rts -- whether boot flows
  into it (changing final $1526) needs a whole-boot probe.
- Loop-B re-pin still open (WILL_FAIL kept).
- V3 (voice-start/KEYON + render + WAV) and the trigger-path
  hunt are next; sample DATA correctness needs the real mpr
  banks (absent from this checkout).
