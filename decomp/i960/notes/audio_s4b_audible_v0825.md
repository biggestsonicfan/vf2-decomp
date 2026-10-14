# S4b: audible chain render (v0825) -- 7/10 bar reached

Status: production-chain-authored slot + real mpr sample banks +
model keying renders a non-silent deterministic WAV
(out/chain-voice.wav, peak 13964, disk-verified). Criterion 4
(ROM-driven keyed voices to WAV) holds on control + audibility;
timbre (bank order, rate mapping, engine keying) stays pending.
Sound gate green.

## Leg (test_chain_audible, experiment-marked, NOT recovery)

Real init with 8MB real-bank window (mpr-17570..73 numeric
concat) -> packet 0x9a -> staged arming -> 0xc68 door (same
chain as S4a). Authored slot 0x0000, control 0x1000, rate 0x00,
bank[0..3] = 00 00 fd fd (real bytes). Key staging: OR 0x18
into the control byte (ROM 0xefe, exact 3-insn shape) + OR
0x0800 KEYONB into the control word (model key gate, same as
V1/V3c) -> slot_active = 1. Render 2048 frames: peak 13964,
meansq ~2.9M; snapshot-restored re-render bit-identical.

## Honesty boundaries (what this does NOT prove)

- KEYING is staged, not recovered: the 0xc68+ engine (which
  really keys voices, via its movem-programmed control words)
  is unrecovered. The leg proves the authored CONFIG renders.
- Play address: the model zeroes slot_address on key-on, so
  the render starts at window offset 0 regardless of the
  programmed SAHI/SALO. Real-bank ORDER therefore conditions
  only timbre here, not audibility.
- Timbre pending: bank order assumed numeric; rate 0x00 is
  whatever the real $5000/rate bytes say; no musical claim.
- maintain_voices is NOT run (it would age/key-off the staged
  voice on its V1 rules); the render starts from the authored
  state directly.

## Side findings

- The "absent banks" premise (v0825 S1 era) is STALE: all four
  mpr-17570..73 (2MB each) are present in this checkout.
- 7/10 exit criteria status: 1 (S1 feeder) ✓, 2 (S2 Loop-B)
  ✓, 3 (S3 setup) ✓, 4 (S4 WAV: control S4a + audible S4b) ✓,
  5 (suite green, no WILL_FAILs, no proprietary artifacts) --
  pending the final full run. 8/10 (session music) untouched.
",
