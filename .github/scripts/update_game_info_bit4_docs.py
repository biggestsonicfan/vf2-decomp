from pathlib import Path

note = Path("decomp/i960/notes/game_info_18644_mode_bit6.md")
text = note.read_text()
old = '''The deliberately unproved fringes remain fail-closed: state bit 8 + bit 4 follows a distinct fast path, mixed state8+bit1 combinations are not inferred from the isolated corridor, and state bit 8 present on both fighters changes fighter-order accounting. Those cases must be recovered independently.'''
new = '''The isolated state-bit-8 + bit-4 fast path is also recovered for zero countdown. At `0x18778..0x18784`, the ROM masks `r8` with `0x0401c010`; bit 4 therefore branches directly to the shared `0x18890` tail instead of entering the signed-distance tree. The mode-clear task already matched this path at 671 instructions. With mode bit 6 set, the first fighter order uses the same recovered fast path and the swapped second order (`r7 = 0x00000110`, `r8 = 0`) rejoins eight instructions earlier than the generic mode-bit-6 fallback accounting. The resulting full task matches the ROM exactly at 676 instructions, with the same 12 calls and 13 returns.

The bit-4 recovery is deliberately bounded to that measured zero-countdown state. Mode-bit-6 + bit8+bit4 with a nonzero countdown, bit8+bit4+bit14, and bit8+bit4 on both fighters remain fail-closed. Mixed state8+bit1 combinations and other two-sided bit-8 states are likewise not inferred from the isolated corridors and must be recovered independently.'''
if old not in text:
    raise SystemExit("note anchor not found")
note.write_text(text.replace(old, new, 1))

changelog = Path("CHANGELOG.md")
text = changelog.read_text()
old = '''- expanded `fa_game_info` mode-bit-6 + fighter1 state-bit-8 recovery at
  `0x00018644` across a controlled ROM-backed state matrix: isolated bit 8 and
  bit 8 combined with bits 6, 14, 15, 16, 21, 26, 29 or 30, plus priority pairs
  14+15 and 15+16, all match the original i960 exactly; isolated mode-bit-6 is
  689 instructions versus 684 with mode bit 6 clear, while bit 8 + bit 4,
  bit 8 + bit 1 and two-sided bit 8 remain explicitly fail-closed;
'''
new = '''- expanded `fa_game_info` mode-bit-6 + fighter state-bit-8 recovery at
  `0x00018644`: the controlled state matrix remains exact, the isolated
  bit8+bit1 `0x188cc..0x18978` subtree is recovered, and the isolated
  zero-countdown bit8+bit4 fast path now matches at 676 instructions with
  mode bit 6 set versus 671 with it clear; bit8+bit4 countdown/mixed states
  and other two-sided bit-8 combinations remain explicitly fail-closed;
'''
if old not in text:
    raise SystemExit("changelog anchor not found")
changelog.write_text(text.replace(old, new, 1))
