# 8h: synthetic chain bridge + 8/10 reassessment (v0825)

Status: `test_chain_uart_bridge` green first run (bytes
`9a 00 01`, ring = S4 packet, voiced = 1, peak 13964,
determinism holds, `out/chain-uart.wav` written). Full suite
in the commit validation.

## What the bridge proves

Every link of queue -> bytes -> ring -> voices -> WAV is now
stepper-proven C AND the composition is gated in one test with
no `emit_command` shortcut:

- filer 0x43888 (v0746, gate-A-open path) files g0 =
  `0x009a0001` -> count 0 -> 1, wcur 0 -> 1, ring[0] staged;
- drainer 0x43800 (8f) 3 ticks (bidx ff -> 2 -> 1 -> 0 -> ff)
  emit staged[2],[1],[0] = `9a 00 01` via 0x01c80000
  (count 1 -> 0, sentinel cycle);
- L3 `file_uart_byte` (8g) assembles ring `9a 00 01 00` --
  exactly the S4 production packet (4th byte untouched zero);
- `dispatch_next` (S4) consumes to the 0xc68 engine door
  (UNSUPPORTED by design); voice-count 1 (+2 = 0x58);
- keyed render (S4b harness, real banks): peak 13964
  (== S4b `chain-voice` peak, as expected -- same packet and
  voicing), meansq 2872160, snapshot re-render bit-identical.

g0 was chosen, not found: LE `0x009a0001` stages bytes
`01 00 9a 00` so drainer emission order [2],[1],[0] delivers
the hi byte first, which is what L3 assembly requires. Any
other byte rotation would file staging without ever taking the
ring arm -- the emission-order gate pins this.

## 8/10 reassessment: NOT closed, and the shape of the gap changed

The bar (8a/8b) is session integration: real gameplay
producing audible sound in-session. The bridge does not move
that bar -- staging g0 in a test is exactly the "test staging
laundered as integration" 8b forbids. Verdict: **control path
complete, live trigger still the entire gap.**

What changed is the gap is now a single unknown: everything
downstream of "a real game event calls the filer with a real
g0" is proven, including composition. The three 8a evidences
stand (no in-session producer, no sound region in model2a,
game frame never touches sound). Next: landed-hit setup or
round-boundary play to find the live writer -- then wire, not
stage.
