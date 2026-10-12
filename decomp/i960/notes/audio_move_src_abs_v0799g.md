# v0799g - move-src abs/predec + SUB/CMP/ADD scan-absent (slices B/C)

## Slice B: move-src handler completed

The general `MOVE -> Dn/An` handler admitted src modes 2-6/7.3
but printed only 2/3/6/7.3: mode 4 (`-(An)`) printed as
`$0000(aN)` (same length by luck, wrong text), and 7.0/7.1/7.2
(abs.W/L, d16-PC) were not admitted at all (`.word`). Fixed with
correct lengths (abs.L +4) and prints; the final else is now a
truly-unreachable fallback. Vectors: ROM `16 20`
(`move.b -(a0),d3` at primer-adjacent 0x854 — the genuine
recovered-path instance) + synthetic abs.W/L and d16-PC.
Stash-gate: old code fails, fixed green (forced object deletion
both ways per the v0799e hygiene rule).

Rescan: zero blanks and zero remaining `$0000(aN)` move prints in
frontier sweeps; the genuine d16-disp-0 at 0x684 still reads
`move.w $0000(a0),d1` as it should.

## Coincidence taxonomy (from verifying every scan hit by bytes)

Raw ea-pattern scans cannot distinguish instruction starts from
extension/data words. This slice's verifications:

- `0x3220`-family (`move -(a0),d0` shape): all are the `$3220`
  displacement word of `lea $3220(a6),Ax` (0xb3c/0xbda/0x11d2 and
  kin). Dismissed by alignment.
- `@0x220`-family (`move.l -(a2),d0` shape): jmp/jsr/lea
  displacement words (`$2020` stream-desc bases). The 0x21e
  `jmp $2242` reads correctly under e+2 (0x220+0x2022 = 0x2242;
  an arithmetic slip briefly suggested otherwise — byte-level
  method caught it).
- `0x2e10+`/`0x2b2c+`/`0x2f46+` runs: data/address tables.
- Genuine: exactly one, 0x854 (`move.b -(a0),d3` in the tiny
  subroutine preceding the 0x858 primer).

## Slice C verdict: scanned-absent, not fixed

Zero SUB/CMP/ADD/TST indexed-ea instances in frontier code
(< 0x3000, entry-agnostic byte scan). Per the agreed rule the
handlers are left untouched and this paragraph is the evidence
of absence. If the frontier ever expands past 0x3000, re-run the
scan before trusting linear disassembly there.
