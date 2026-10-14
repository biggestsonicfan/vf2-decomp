# S1: Loop-B caller hunt (v0825) -- FOUND via the 0x90 HIGH path

## Census: zero direct callers

Whole-image scan (branch census script, scratch): every Bcc/BRA/
BSR (byte + word, +2 base), jmp/jsr absolute + (d16,PC) --
**0 targets land on 0x902**. (0x4EFB/0x4EBB indexed forms are
unresolvable statically by construction; no evidence any aims
at Loop-B.)

## The feeder: 0x90-arm HIGH table path, fall-through

The 0x90 arm at 0x858 files $150f/$1510 and splits on desc[2]:
- `< 0xf0` -> 0xa3e path -> files $1515/$1511, `bra $f12`
  (scan-miss exit). NEVER reaches Loop-B.
- `>= 0xf0` -> table path (a2 = be32(0x6032c6 + (d3&0x7f)*4),
  stride d0*6, 0x8a6 gate) -> 0x8b2 -> 0x8c2/0x8d2/0x8e4/0x8f6
  -> **falls through 0x8fe into 0x902**.

New probe leg F11 (`f11-feeder`): entry 0x858 (a0 = RAM desc
with [2] = 0xf5, a3 = RAM stream {0,1}) -> 0x902 arrival,
38 steps OK, zero events. Entry-state recipe for S2: d4 =
$250f & 0x7f (= 0), a3 advanced +2, $250f/$2510/$2511/$250d
filed (00/09/00/01: $2510/$2515 sources are real ROM bytes
0x60c6a7/0x60c3e6, verified by script), $2515 still 0 (filed
by Loop-B's own first block -- the stop fires first).

## Producibility: YES, from boot's own descriptors

Real boot images carry desc[2] >= 0xf0 all over: img1 desc18
(0xf0), img2 desc2/4/6/8/10/12/14/16/18 (0xff/f3/f4/f2/f3/fd/
f6/f7/fa). The +4-era "unmatchable/unproducible" half of the
dead verdict is dead. (Script: descbytes.py, scratch.)

## Two self-corrections in F11

1. `$15xx(a6)` = absolute 0x25xx. First version read 0x15xx
   absolute and pinned zeros -- caught because $150d stayed 0.
   Same trap class as the v0732p synthetic-address fixture.
2. Stop-pc conventions: predicted 37, silicon 38 (done
   excludes the stop insn -- the V3a rule, re-learned once).

## S2 opener (found during S1, NOT fixed here)

Drain descriptor scan at 0x7e6 is `cmp.b $0001(a0),d0` (d0 =
command & 0xf via 0x7d2 `andi.b`) -- a BYTE match on desc[1].
The C (`dispatch_next`) uses `read_be16(descriptor + 1)` word
match. Under word-match NO boot descriptor both matches and
feeds Loop-B; under byte-match img2's 0x0a-0x0e rows route
0x9a-0x9e packets straight into the HIGH path. So the byte/word
question DECIDES Loop-B's production reachability -- S2's first
task, with all dispatch fixtures re-staged behind it (they
currently pin word-match: desc[1] = 0, desc[2] = channel).
Evidence is one measured instruction; the full rule + re-pin
is S2 work, not a drive-by fix here.
