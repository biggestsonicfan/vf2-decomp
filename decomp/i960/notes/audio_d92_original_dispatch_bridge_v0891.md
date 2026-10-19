# v0891 — Original D92 setup bridges to the 512-byte DF8 copy and ED6 KEYON

Date: 2026-10-19. This is a **controlled ROM-backed differential**,
not a naturally triggered game collision. User ROM, sample bytes, and
sound output remain local.

## Actual original ROM test result

Using original VF2.1 sound ROM and 8 MB sample ROM plus the GCC/Linux
sound-test binary from GitHub Actions on
[commit bcd3391](https://github.com/celsowm/vf2-decomp/commit/bcd3391bdcaffb43988b751afe55468240be2de7)
(workflow [CI #2130](https://github.com/celsowm/vf2-decomp/actions/runs/39319582418)),
the test command:

```sh
vf2_m68k_engine_copy_page_tests /path/to/local-original-vf2-roms
```

returned **exit code 0** and the new assertion:

```text
MATCH original dispatch D92->DF8->ED6 entry_steps=334 copy_tail_steps=55 sample=00e633b2 dest=00010200 flag=81
```

The original 68000 starts from synthetic but ROM-exact command,
descriptor, and channel data. It executes **334 original instructions**
from the ring dispatcher `0x7A6` to `0xDF8`, then **55** original
instructions to `0xED6`, stopping before RTS. The completed branch
copies **512 original-ROM sample bytes** into sound RAM, records the
end pointers, and applies the SCSP KEYON write with stream flag
`0x81`. Its corresponding recovered-C branch is checked against
original voice, stream and SCSP at the `0xDF8` boundary.

The completed 55-instruction original suffix is compared against
`vf2_sound_board_copy_engine_page_512()` and
`vf2_sound_board_finalize_engine_copy_eb8()`, requiring:
- exact original A0/A2 advancement;
- full 512 KiB RAM equality (the local C post-state starts from
  the independently observed original `0xDF8` RAM snapshot to avoid
  incorrectly asserting already known, unretired drain bookkeeping);
- full modeled SCSP equality;
- original end pointers stored in the correct stream record and
  channel flag `0x81`.

This is **one continuous original 68000 execution** spanning
`334 + 55 = 389` instructions, not 389 proven C-native
instructions. Only the verified parts of recovered C are compared
at their respective boundaries.

## Critical real-ROM failure and fix

The first attempt failed closed at `0xDF8` with
`A0=0 A2=0`. That was a genuine unmapped-copy condition in the
synthetic fixture, **not** proof of a faulty MOVEM opcode or a
naturally silent game.

Further disassembly of the user's original `epr-17574.30` identified
the required input provenance:

- Original opcode `0x2045` at `0xC74`:
  `MOVEA.L D5,A0`. D5 came from the first longword in the selected
  table (in this staged case `0x6900`).
- Original opcodes `0x2469 0x000C` at `0xD9C`:
  `MOVEA.L 0xC(A1),A2`. The destination is voice+0x0C.
- The synthetic descriptor reaches selection `0x0140` and
  sample table `0x6900`; the fixture explicitly populates
  `0x6900=0x00E631B2` and voice+0x0C=`0x00010000`.
  It is legitimate test input, **not** a claim that a real fighter
  automatically produces these values.

The two original ROM-selected AD-family addresses have already been
independently checked in prior v0887–v0890 notes; no proprietary
sample bytes or WAVs are in the Git tree.

## PCM regression

The subsequent test amendment
[55feed2](https://github.com/celsowm/vf2-decomp/commit/55feed2ec23e24a477c54bda48ecfd6ab4ee8994)
compares 4,096 stereo frames rendered by the same **software SCSP
model** from the original-68000-programmed SCSP state and the
recovered-C-programmed SCSP state. It reports nonzero frame count
and peak amplitude but does not demand nonzero output without
evidence. This is a model-level parity/determinism check,
**not** arcade hardware PCM/DSP fidelity. Confirm its runtime
result before claiming the PCM extension passed.

## Remaining scope

The i960 gameplay origin of this `0x93` staged command is **not**
verified. UART/L3 simultaneity, naturally triggered fighting sounds,
continuous ISR+dispatch timeline, SCSP DSP runtime effects and
hardware PCM oracle are not closed. The recovered-C production
dispatcher still returns UNSUPPORTED at the established `0xDF8`
boundary; this test uses an explicit, guarded continuation and
does not silently widen that production contract.

## v0891b — PCM extension executed on the original owner ROM

The opt-in SCSP-model PCM extension was compiled and run using
the GitHub Actions GCC sound test binary built from
[commit 55feed2](https://github.com/celsowm/vf2-decomp/commit/55feed2ec23e24a477c54bda48ecfd6ab4ee8994),
workflow [CI #2131](https://github.com/celsowm/vf2-decomp/actions/runs/39319928420),
and the original owner VF2.1 ROM and sample chips, retained locally.

```text
MATCH original dispatch D92->DF8->ED6 entry_steps=334 copy_tail_steps=55 sample=00e633b2 dest=00010200 flag=81 model_pcm_peak=2029 nonzero_frames=3949/4096
MOVEM+KEYON: two original 55-instruction copy/tail paths PASS
```

**Exit code: 0.** PCM left/right sample buffers matched exactly
between the recovered C programming path and the original 68000
programming path when rendered using the **same** deterministic
SCSP software model. This demonstrates non-silent *modeled* output
for a staged D92 input, not measured Saturn/Model 2 silicon or
natural combat audio, and does not include runtime SCSP DSP fidelity.
No WAV, ROM, sample buffer or trace was committed.
