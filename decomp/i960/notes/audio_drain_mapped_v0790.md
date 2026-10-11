# v0790 - drain mapped: class-default + entry addresses fixed; Tier-2 cluster stated

## Tier 1 done (code + tests, this slice)

- **Drain at 0x7a6** (entered via `jmp 0x7a6` from the sample path):
  count check, count--, a3 = ring + read-cursor (0x1508 & 0xfff),
  `(a3)+` command fetch, `CURRENT_COMMAND = command` (0x150e),
  channel = command & 0xf, per-descriptor channel-word scan,
  `jsr 0x81a` per match, cursor +4 / count consumed on exit.
- **Class switch at 0x81a**, raw-verified targets: 0x90 → **0x858**,
  0x80 → **0x10d0**, 0xe0 → **0x12b4**, 0xb0 → **0x13ca** (fetch
  intact), 0xc0 → **0x1e5c**, 0xa0 → `jmp (d16,PC)` → **0x1e76**,
  **anything else → bne.w fallthrough into 0x858 (0x90 head)**.
  The C `:1730`-style class refusal is removed: unknown classes
  (incl. D0/F0) run the 0x90 path, as the ROM does.
- **handler_address fixes** (all were +2 high): 90: 0x85a → 0x858,
  80: 0x10d2 → 0x10d0, e0: 0x12b6 → 0x12b4, a0: 0x1e74 → 0x1e76.
  B0-table targets and c0's 0x1e5c were already exact.
- **New D0-routing leg** in `test_sound_board` (0xd3 → 0x90 path,
  handler 0x858, consumed cursor/count).
- **Gates proven**: routing plant (guard restored) fails exactly
  the 5 new D0 assertions; address plant (0x858 → 0x859) fails
  exactly the 2 handler-address assertions; both reverted green.

## Decoder findings (methodology, reuse)

- The v0789 "indexed-mode" scare was MY mode-numbering error:
  68000 (d16,An) is mode 5 (0x2E), not mode 6; the decoder was
  right all along (`43 EE 00 00` = lea (0,a6),a1).
- Real decoder bug: **branch-target prints are +2 high** on
  0x60/0x61 word-displacement forms (beq 0x2042 → prints 0x2050,
  raw 0x204E; bra 0x204c → prints 0x211e, raw 0x211C; 0x90-beq
  → prints 0x85a, raw 0x858) and **-2 low** on `jmp (d16,PC)`
  (0x1fdc → prints 0x2052, raw 0x2054). ALWAYS raw-verify
  branch targets in 68k work; the v0788 "B0 → 0x13cc" row and
  "class table → 0x2052" row are corrected to 0x13ca / 0x2054.

## d0-provenance SETTLED for the drain path

Heads begin with `(a3)+` payload fetch (a3 = ring+cursor+1 left
by the drain), so d2 = payload; B0 (0x13ce) and E0 (0x12b8)
then `move.b (a3),d0` — **d0 = ring byte 2**, not payload.
C0 (0x1e5a), 90 (0x858), 80 (0x10d0) use d0 = payload (fetch,
no peek) — those C bodies' payload assumption is correct.

## Tier-2 cluster (documented, NOT changed — next slices)

1. **Descriptor match** (drain 0x7e6 `cmp.w 1(a0),d0`, d0 =
   channel): ROM matches desc[1..2] == channel (desc[1] = 0).
   C `dispatch_next` matches == full command. Every dispatch
   test's desc[2] (= command today) must become channel — and
   see (5) first.
2. **90 base/mode** (0x85c-0x872): ROM base = payload +
   desc[4]-BYTE (no 0x7f mask, desc[5] ignored); C masks and
   adds a 16-bit word. ROM LOOKUP_MODE = table_60c6a6[ring
   byte 2] (skipped if zero); C indexes by payload.
3. **A0 byte shift** (0x1e76): ROM row = command&0x7f, max and
   row-pointer indexed by payload (ring byte 1). C uses
   payload for the row and ring byte 2 for max/ptr. Signature
   change: `dispatch_a0(board, command, payload)`.
4. **80 key** (0x10d0): ROM base = payload&0x7f + desc[4];
   C uses command&0x7f.
5. **High-path paradox**: under (1), desc[2] = channel ≤ 0x0f,
   so 90's selector ≥ 0xf0 high path (real ROM code at 0x884+)
   looks unreachable — yet it exists. Do not "fix" (1) until
   a live descriptor with desc[2] ≥ 0xf0 is explained (writer?
   second match site? the 0x1330-loop matches desc[4..5]).
6. **B0/E0 bodies**: audit every d0-use against ring-byte-2
   (entry-7 formula, 0x29 pack, e0 combined/index) with
   byte2 ≠ payload fixtures.
7. **Sequence appliers** (0x1ffc/0x2054/0x2080/0x20ac + tail
   0x211c/0x211e): C's per-class packet builders (B0 raw-vs-
   masked byte, 0x80 one-fetch-vs-two, stale bytes 2-3) need
   the same raw treatment; 0x2050 4 bytes look dead; 0x2054's
   `move.b d0,(a0)+` targets ROM (no observable effect).

## File map

- `src/hardware/sound_board.c`: default arm + 4 addresses.
- `tests/hardware/test_sound_board.c`: 5 address expectations
  + D0-routing leg.
- Evidence: raw-byte-anchored disassembly of 0x7a6/0x81a-switch/
  0x858/0x10d0/0x12b4/0x13ca/0x1e5c/0x1e76/0x1ffc/0x2050-0x2080/
  0x211c-0x2130 + full-ROM reference scans.
