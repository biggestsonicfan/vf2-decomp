from pathlib import Path

path = Path("CHANGELOG.md")
text = path.read_text()
old = '''- expanded `fa_game_info` mode-bit-6 + fighter state-bit-8 recovery at
  `0x00018644`: the controlled state matrix remains exact, the isolated
  bit8+bit1 `0x188cc..0x18978` subtree is recovered, and the isolated
  zero-countdown bit8+bit4 fast path now matches at 676 instructions with
  mode bit 6 set versus 671 with it clear; bit8+bit4 countdown/mixed states
  and other two-sided bit-8 combinations remain explicitly fail-closed;
'''
new = '''- expanded `fa_game_info` state-bit-8 recovery at `0x00018644`: the
  controlled mode-bit-6 matrix remains exact, isolated bit8+bit1 and
  zero-countdown bit8+bit4 are recovered, and the isolated bilateral bit8
  corridor now matches exactly at 690 instructions with mode bit 6 clear and
  695 with it set; bilateral countdown/mixed states and other unmeasured
  combinations remain explicitly fail-closed;
'''
if old not in text:
    raise SystemExit("changelog anchor not found")
path.write_text(text.replace(old, new, 1))
