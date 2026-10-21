# Full frame-342 audio chain: original-ROM parity (v0901)

## Reproducible scope

This check uses the owner's checksum-matched VF2.1 arcade ROMs.
Nothing from the original ROM or sample MPR chips is committed.
Starting from the 15 original i960 vector-15 UART bytes, the
existing C `file_stream` receives 5 commands:

```text
9E 0A 7F  CD 74 F8  CA 7E F8  CB 7F F8  AD 21 00
```

`vf2_m68k_frame342_dispatch_tests --verify-frame342-chain`
does NOT reset sound RAM, command descriptors, or SCSP between
drain callbacks. Both the recovered C driver and the restricted
original-ROM Motorola 68000 stepper run the 5 commands in sequence.
The 68000 CPU registers/PC are initialized for each drain callback;
this is **not** a time-accurate co-simulation of i960/68000/SCSP.

The C dispatcher has the explicit existing `allow_verified_90_allocator`
enabled **only in this owner-ROM test fixture** for the verified
`9E 0A 7F` frame-342 path. Production defaults remain fail-closed.

## Measurements on original chip data

| Stage | Original 68000 instructions | RAM delta disagreement | SCSP register disagreement |
| --- | ---: | ---: | ---: |
| `9E 0A 7F` | 367 | 0 | 0 |
| `CD 74 00` | 305 | 0 | 0 |
| `CA 7E 00` | 305 | 0 | 0 |
| `CB 7F 00` | 305 | 0 | 0 |
| `AD 21 00` | 248 | 0 | 0 |
| Original L2 tick at `0x1f16` | 471 | 0 | 0 |
| 12 B9–BE commands, sequential | 3,410 | 0 | 0 |

The three `Cx` ring entries are observed with third byte `00`
after the preceding `9E` call, even though their incoming UART
triples originally have `F8` there. Both C and original 68000
agree on this intermediate state. The effect must not be confused
with independent isolated-command tests that reset the queue.

After the whole stateful chain, the original-ROM-driven modeled
SCSP still has **one active voice**. Both C and MC68000 states
rendered 4096 identical stereo PCM frames (920 non-silent frames).
The queue is drained (`count=0`, read cursor `0x0044`).

The strict test returned exit code 0 against the owner's ROM.
No private ROM bytes were committed or used by public CI.

## Run locally

```sh
cmake -S . -B build -DVF2_BUILD_TESTS=ON \
  -DVF2_ROM_DIR=/path/to/unpacked/vf2 \
  -DVF2_VERIFY_FRAME342_CHAIN=ON
cmake --build build --target vf2_m68k_frame342_dispatch_tests
ctest --test-dir build --output-on-failure \
  -R '^vf2_m68k_frame342_stateful_chain_owner_rom$'
```

To inspect without strict assertions:

```sh
build/vf2_m68k_frame342_dispatch_tests /path/to/vf2 --probe-frame342-chain
```

To export audio locally only after exact original/C PCM agreement:

```sh
mkdir -p out
build/vf2_m68k_frame342_dispatch_tests /path/to/vf2 --wav-dir out
build/vf2_m68k_frame342_dispatch_tests /path/to/vf2 --chain-wav-dir out
```

The owner-ROM `--chain-wav-dir` run (commit `44c86a4`) also
exited **0**: 5/5 UART, L2, 12/12 B, and 22,050/22,050 C/original
stereo PCM frames matched. The final clip had 1,840 non-silent
frames, peak 22,568. The isolated-`9E` clip and the **complete-chain**
clip are **byte-identical**, both SHA-256
`728811fe6463a26f773edd58a28790bf6cb2e928de1cc7f2475e45a7b5da50e5`.
This establishes that the intervening Cx/AD21/B **did not change
the software-rendered sound of the already-keyed 9E voice in this
controlled driver state**. It does not generalize to concurrently
running gameplay or other audio tick schedules.

The first file contains PCM rendered after the isolated original
`9E 0A 7F` KEYON; the second contains PCM rendered **after the
whole chain**. Both are 22,050 frames, 44,100 Hz, stereo 16-bit,
written only on explicit request and only after comparison against
the original-MC68000-driven model. Public CI never exports them.

## Boundaries and remaining work

- Original-ROM-backed **driver dispatch parity** does not imply
  real-time simultaneous dual-CPU scheduling or arcade-board audio
  accuracy. The PCM samples come from the project's SCSP renderer.
- The exact user-observed punch/shout timbre is not yet identified
  with an in-game collision. It is premature to label the WAV as
  a confirmed fighter collision sound.
- Runtime session playback remains independent of this opt-in
  owner-ROM 9E driver harness and requires its own causal producer
  verification and timing tests.
