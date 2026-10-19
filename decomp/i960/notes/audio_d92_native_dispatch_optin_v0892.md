# v0892 — Opt-in recovered-C D92 dispatch retires through DF8 upload and ED6 KEYON

Date: 2026-10-19. Base: original VF2.1 owner-ROM differentials v0888–v0891.

## What changed

Previously `vf2_sound_board_dispatch_next()` produced the correct D92
pre-copy channel/stream state and returned `VF2_ERROR_UNSUPPORTED` at
`0xDF8`. The only route to final sample upload and SCSP KEYON was
a **manual test-only continuation** after normalizing the original
68000 RAM snapshot.

The **opt-in** `vf2_sound_board.allow_verified_d92_copy` (default
zero) now lets the normal recovered-C dispatcher use the exact
original-opcode fragments already independently verified against
the owner's sound ROM:

1. D92 slot, voice and stream setup (existing recovered C).
2. Original A0 = big-endian sample pointer in selected table's +0
   field (original `0xC74 MOVEA.L D5,A0`).
3. Original A2 = big-endian RAM destination in voice+0x0C
   (original `0xD9C MOVEA.L 0xC(A1),A2`).
4. `vf2_sound_board_copy_engine_page_512`: 48 instruction-equivalent
   MOVEM+ADDA body `DF8..EB8` (512 sample bytes).
5. `vf2_sound_board_finalize_engine_copy_eb8`: seven-instruction
   `EB8..ED6` tail, registers A0/A2 in stream, KEYON bits `0x18`,
   stream flag `0x81`.
6. Returns `VF2_OK` at the measured `0xED6` boundary, so the
   **ordinary** `dispatch_next` ring-epilogue retires the packet
   exactly once (`handled=1`, cursor +=4, count--).

Preflight verifies an attached original sample-ROM window, even A0/A2,
fully mapped 512-byte transfer, valid voice index, valid aligned
SCSP slot matching the selected voice, and available SCSP sample
attachment. Failures preserve the previous `0xDF8` refusal and
keep the packet pending **without writing any copied samples**.
No other engine arm has been enabled: `CB2`, unmeasured interrupt
timing and other dispatcher frontiers remain fail-closed.

## Explicit session / SDL3 opt-in

The public session API now includes:

```c
vf2_session_set_verified_d92_copy(session, 1); /* enable */
vf2_session_set_verified_d92_copy(session, 0); /* disable */
```

With the SDL3 host executable, an operator who already has a locally
available *valid original-ROM directory and session snapshot* can run:

```sh
vf2_host --rom-dir /path/to/vf2 --snapshot /path/to/valid-snapshot \
  --verified-audio-copy
```

It does not manufacture queue events or force a hit effect. Only
commands naturally filed by the i960/UART session pump are eligible,
and only if they happen to take the guarded D92 branch. **No native
gameplay trace has yet shown this scenario occurring during a real
fighter hit**; the command currently used in the test was
deliberately staged.

## Regression acceptance

`tests/m68k/test_engine_copy_page.c` retains the four isolated
55-instruction ROM differentials and v0891's continuous original
68000 `7A6->DF8->ED6` 389-instruction staged scenario.

The extension takes the SAME start state into three native-C
recovered-dispatcher runs:

- **default OFF:** old `0xDF8` refusal, packet still present;
- **ON but no sample ROM:** refusal, packet still present and
  512-byte target unmodified;
- **ON with valid ROM:** one `vf2_sound_board_dispatch_next` call
  returns `VF2_OK`, `handler_address=0xED6`,
  `handled=1`, ring `count=0`, cursor `0->4`.

The positive path compares its actual native-C 512-byte copy,
32-byte stream record, voice record, and full modeled SCSP state
against the independently executed *original* 68000 state. It
also compares **all 4096 rendered stereo frames** between the
opt-in native-C dispatch and the original-68000-programmed SCSP
using the identical deterministic software PCM renderer. Prior
v0891 test-only normalized C continuation remains a separate
control/oracle.

Important: the original 68000 is stopped **before its RTS at
0xED6**, while native C has already retired the ring entry.
Do not call the full original and C ring bookkeeping equal at
this asymmetric checkpoint. Nor does model PCM equality prove
arcade hardware/DSP fidelity.

To run against the original ROM:

```sh
build/vf2_m68k_engine_copy_page_tests /path/to/owner/vf2
```

No owner ROM, snapshot or WAV is committed.

## What still blocks truly live combat audio

- A frame-progressing gameplay snapshot that naturally produces a
  fighting sound command, proven from the i960 input/state change.
- C/68000 ring-timing equivalence through RTS and later interrupts
  (the existing pump is a shadow-i960 transport model).
- Full coverage of alternate sound engine arms, voice maintenance
  and hardware SCSP DSP/PCM timing.

The opt-in is a measured **engine dispatch recovery milestone**,
not a claim that gameplay hit sounds are now complete.
