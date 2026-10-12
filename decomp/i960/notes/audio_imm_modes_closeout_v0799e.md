# v0799e - ANDI/SUBI/CMPI/ADDI mode 3/4/6/7.x + build-hygiene incident (slice A)

## Decoder work

All five line-0 #imm handlers (ORI/ANDI/SUBI/ADDI/CMPI) now cover
modes 0/2/3/4/5/6/7.0-7.3 with correct lengths, DA-correct
brief-ext prints (new shared `format_indexed_operand` helper for
new code; older open-coded copies left untouched), a real
`format_imm_absolute` helper (abs.W/L, d16-PC, idx-PC), and an
explicit unsupported fallback. Gaps closed along the way:

- ANDI-6: silent (length right, no print).
- SUBI/CMPI-6: short length (desync) + no print.
- All five lacked 3/4 (`(An)+`/`-(An)`); the F7-path
  `cmpi.b #$f7,(a3)+` at 0x2100 is real frontier code that read
  blank until now (its twin at 0x210a verified by bytes
  `0c 1b 00 2f`).
- All five lacked abs/pc (desync for abs.L).
- move-An-Dn (v0799c) and move-Dn-dst (v0799b) verified unaffected.

Scan result: zero ANDI/SUBI/CMPI-6 instances in frontier code
(the 0x2918 run and all other hits are data-table coincidences or
mid-instruction words; the 0x670a/0x6712 pair sits inside the
0x6a4 move.b). Fix is prophylactic (synthetic manual-correct
vectors) except CMPI-3 (ROM bytes). 13 new vectors; stash-gate:
old code fails, fixed green.

Rescan: driver/worker/main-loop sweep now reads with zero
unsupported and zero blanks; remaining `.word`s are the known
data runs (0x6f4/0x714 ISR-table tail, 0xC46-0xC68 skipped run).
No recovered-path reading changed (all frontier #imm sites were
already-correct forms).

## Build-hygiene incident (read this before trusting green)

A forced verbose rebuild of decoder.c FAILED (C4013 on an
undefined `format_imm_absolute` call + C2059 brace-cascade) while
incremental builds had reported RC=0. Forensics:

- The undefined call + a dangling duplicate mode-6 block match
  verbatim the newString of an Edit call that reported
  "multiple matches" failure. A reported-failed edit must be
  treated as possibly-applied: always re-read the edited region
  after any failure report.
- Incremental MSBuild RC=0 masked the breakage (stale objects).
  The stash-gate flips earlier in the session prove fresh
  compilation happened at those moments, and every load-bearing
  disasm claim was byte-verified independently of the tool — but
  that is luck, not procedure.

Rules going forward:

1. After ANY decoder.c edit: delete its .obj files, verbose
   rebuild, grep for error/warning (zero tolerated), and only
   then run tests.
2. After ANY failed Edit call: read the target region before
   doing anything else.
3. Re-read the full `git diff` before committing decoder.c.

The excision (dangling block removed, helper defined, mode-7
gated to regs 0-3) plus the forced clean rebuild with zero
warnings is the evidence this note's claims rest on.
