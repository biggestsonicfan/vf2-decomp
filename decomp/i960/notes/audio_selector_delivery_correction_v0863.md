# v0863 — Correct false-positive selector→queue proof

The v0862 passive witness accepted an impossible synthetic trace:
queue write attributed to i960 PC 0x4388c. That word is the second
word of an immediate-address instruction, not executable code.

The original i960 ROM proves:
- 0x43924: 0x92803903, indexed 32-bit store to 0x00504020
  (the following 32-bit MEMB extension at 0x43928);
- 0x439f8: 0x0a000000, dedup's return to 0x18ac0;
- 0x4394c: 0x0a000000, filer return to 0x18ac4.
These are instruction positions verified against user-owned ROM,
not evidence that the game reached them in gameplay.

tools/python/audio_selector_delivery.py now fails closed unless:
(1) complete aligned queue word is written by the real 0x43924 store;
(2) the dedup return instruction is the actual 0x439f8 ret;
(3) the filer return is the actual 0x4394c ret;
(4) the code's opcode and extension words match ROM, with completed
queue write address included in the witness record.
Unknown return destinations and writes at non-store PCs do NOT
upgrade to confirmed ring delivery.

The fixture now labels its synthetic steps as execution-shape tests
and uses ROM-valid instruction locations; 5 explicit false-positive
regressions cover fake dedup/filing returns, a write from the MEMB
extension word, corrupt store and corrupt ret opcodes.

Validation: user-local VF2 maincpu.bin / original zip and Python
test suite: 86/86 passed (81 prior plus five new). No natural
pre-contact sound event, complete native C suite or SCSP key-on was
proven in this slice. Never commit ROMs, snapshots or full traces.

Next: real selector witness -> populated queue entry -> interrupt
15 UART byte stream, and only then 68000 L3→SCSP with differential.
