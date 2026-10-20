# v0900 — Original frame-342 AD 21 00 stream: 12 B/CF commands

Date: 2026-10-20. **Private owner VF2.1 audio ROM executed; no ROM,
snapshot, PCM files or proprietary assets committed.**

## Why the prior command dispatch was insufficient

The fifth real i960 command of frame 342 was `AD 21 00`. Its
original MC68000 dispatcher exits after 248 original interpreted
instructions (PC `0x265C`), creating a stream descriptor,
rather than generating a KEYON itself. The recovered C dispatcher
already matches every meaningful non-stack RAM byte and SCSP
register at that checkpoint (v0899).

The fresh record is stream descriptor 1 at `0x3020`,
status `0x80000001`, next-record pointer `0x0060A6B3`.
The MC68000 ROM-mapped record header at `0x60A6B2` is
`0xCF` and is **not** the `0x8F` AD23/AD24 two-packet grammar.

## Original CPU vector-26 / L2 execution

In the independent owner-ROM test, a new L2 tick was executed
starting from the exact **post-dispatch** audio RAM and SCSP
registers in two distinct runtimes:

- Original MC68000 interpreter: PC `0x1F16` to `0x1F7A`,
  **471 original instructions**, stop status OK.
- Recovered C: `vf2_sound_board_maintain_streams`.

The original emitted **12 command-ring packets**, a single tick,
advancing the write cursor from `0x0014` to `0x0044`
and setting the command count to 12:

```text
B9 2A 00   BA 2A 00   BB 2A 00
BC 2A 00   BD 2A 00   BE 2A 00
B9 07 7F   BA 07 7F   BB 07 7F
BC 07 7F   BD 07 7F   BE 07 7F
```

The original L2 worker writes only 3 bytes of each 4-byte
ring entry; the fourth byte is left untouched. Descriptor #1
retires to `00 FF 00 00` and preserves its pointer `0x60A6B3`.

## Key reverse-engineering discovery: chip versus CPU byte order

The raw chip `epr-17574.30` is word-byte swapped relative
to the memory region assembled by `vf2_romset_build_region`.
For example, bytes at the CPU-accessible `0x60A6B3` read
`B9 2A 00 00`, and the previous byte is the real `CF`
header. The actual end marker is `FF 2F 00`; the following
byte belongs to the *next* record (`CF`), not to this marker.
An earlier strict recognizer incorrectly read chip-oriented
byte order and incorrectly expected a fourth zero terminator
byte, so it failed closed. Both errors were corrected using
the original interpreter observations.

The new `vf2_sound_l2_ad21_expand` recognizes **only**
the original after-header pointer `0x60A6B3`, checks its
header and all 12 validated 4-byte instructions and exact
3-byte terminator, then produces 12 three-byte packets.
It does not admit neighboring stream addresses or unrelated
CF grammars. The L2 C worker preflights alignment, command
counter capacity (at most 243 before adding 12), bounds and
record shape before touching RAM/SCSP. A malformed stream
leaves RAM/SCSP unchanged.

## Owner-ROM exact differential result — PASS

Actual original-ROM owner run with the GitHub Actions
`vf2-m68k-sound-differential-linux` executable from
[CI #2244](https://github.com/celsowm/vf2-decomp/actions/runs/39401761618),
invoked as:

```sh
vf2_m68k_frame342_dispatch_tests /local/owner/vf2-roms --probe-ad21-l2
```

Observed **exit status 0**:

```text
FRAME342_AD21_C68000_PARITY differences=0 first=80000 C_handler=01f02 S_steps=248
FRAME342_AD21_L2_PROBE C_status=0 C_refusal=0 C_refusal_record=255 C_count=12 C_stream_count=0 M_status=0 M_pc=01f7a M_steps=471 M_count=12 M_stream_count=0
FRAME342_AD21_L2_PARITY diff=0 l2_first=80000 scsp_regs_equal=1 c_packets=12 c_cursor=0044
```

**Zero byte differences** across 512 KiB modeled sound RAM
except the original interpreter's stack scratch region
`0x7E00..0x7FFF`; full modeled SCSP register parity.
No additional KEYON is claimed at the L2 expansion stage.
This verifies the newly generated sound-ring entries,
descriptor retirement and timer/control registers **from
the original CPU**, not just an independently fabricated fixture.

The ROM-free `vf2_m68k_ad21_l2_rom_free` regression also
asserts the 12 packets and cursor movement, three-byte
stores preserving the fourth ring byte, descriptor completion,
timer pokes, and seven malformed-record / rollback scenarios.
In [CI #2244](https://github.com/celsowm/vf2-decomp/actions/runs/39401761618)
the GCC suite passed **130/130 tests**.

The stronger original-ROM CTest
`vf2_m68k_frame342_ad21_l2_owner_rom` is only registered
when the owner provides a local `VF2_ROM_DIR`; public CI
never includes proprietary game ROMs.


## Owner-ROM execution: B0 table index was wrong (October 20, 2026)

The original 36-chip owner's `vf2(20261018-203049).zip` was verified:
the audio EPR `epr-17574.30` has CRC32 `4d4c3a55`,
SHA-1 `b6c0c3f0473bd7fc3ef4f5146110dfcc899a5af9`,
and all four sample MPR checksums match the manifest. The CPU reads the
audio region using `LOAD16_WORD_SWAP`.

**The original 68000 was executed locally** with the owner ROM and the
Linux `vf2_m68k_frame342_dispatch_tests` executable retrieved from
the project's GitHub Actions build artifact, without uploading ROMs.
The AD21 L2 stream executed **471 instructions**, produced **12 packets**
and matched C with **zero RAM/SCSP differences outside the earlier L2
stack**. The post-L2 B0 dispatch first exposed a genuine C bug:
both original commands finish, but the old C spuriously re-staged the
uploader's 13,000 bytes and changed 60 bytes relative to the original
state. Those bytes are from old, relocated state that the 68000 leaves
untouched; the original PCM area and SCSP registers were unchanged.

Single-instruction tracing of the original B0 dispatch at `0x13cc`
identified the bug: the B0 jump table is indexed by
**ring byte 1 & 0x7f**, NOT `command & 0x7f`. Ring byte 2 is
placed in D0 as the selected handler's argument.
The table entries and original stops were:

| AD21 packet | Table index | Table PC | Original handler | Original steps |
| --- | --- | --- | --- | --- |
| `BA 2A 00` | `0x2a` | `0x1486` | `0x194a` | 256 |
| `BA 07 7F` | `0x07` | `0x13fa` | `0x16ba` | 304 |
| `B? 3A XX` | `0x3a` | `0x14c6` | `0x1ac0` | not AD21 |

The owner-ROM handler `0x1ac0` **would** stage 13,000 bytes from
ROM `0x605b28` to sound RAM `0x5000..0x82c7`, then gate sample
rows using the directory maximum of **1**. It is **not entered by**
these two authentic BA packets. Both of their original executions
change only the common drain fields `0x2506`, `0x2509`,
`0x250e` in the isolated post-L2 snapshot. A C test that merely
sees equal SCSP registers or no PCM bytes is insufficient.

A secondary harness bug had initialized A7 to `0x7f00`, inside the
uploader's would-be staging range. The ROM reset vector provides the
correct initial supervisor stack pointer **`0x5000`** and reset PC
`0x600100`. The diagnostic now takes SSP from the ROM and excludes
`0x4f00..0x4fff` for original 68000 stack writes. The earlier L2
tick also left pre-existing differences in its old `0x7f00` stack
footprint. The strict BA check therefore compares **state deltas**
relative to each side's own post-L2 baseline, rather than conflating
unrelated earlier stack differences with new BA writes. It requires
unchanged staging memory, zero PCM upload, identical SCSP registers,
exact single active channel-A dispatch and matching RAM mutation
patterns.

**Owner-ROM differential retest passed (exit code 0).**
The executable was recovered from CI's
`vf2-m68k-sound-differential-linux` artifact for commit `dee6856`
and executed locally against the original 36-chip VF2.1 ROM folder
(the ROM was never uploaded to GitHub):

| Isolated original B0 command | M68000 steps | C/68000 mutation diffs | SCSP diffs | Staging changed | PCM changed |
| --- | ---: | ---: | ---: | ---: | ---: |
| `BA 2A 00` | 256 | 0 | 0 | no | no |
| `BA 07 7F` | 304 | 0 | 0 | no | no |

The absolute RAM comparison still prints **32 differing bytes**
starting at `0x7ec9`, all inherited from the original *preceding*
L2 stack, rather than written by BA. The strict comparison excludes
this false dependence by comparing the **mutations relative to each
post-L2 snapshot**, and checks both BA packets preserve the entire
13,000-byte would-be uploader staging region. In the same run, the
L2 original emitted twelve packets in 471 instructions with zero
RAM/SCSP differences against C.

The implementation and ROM-free fixtures were updated accordingly.
The synthetic `B? 3A 05`, `B? 3A 07` and `B? 3A 2A` cases still
exercise the real uploader entry, using byte 2 as its row argument;
they must **not** be confused with authentic AD21 BA packets.
The script `tools/inspect_ad21_ba_owner_rom.py` verifies checksum,
machine-code jump-table geometry and original data independently.
The `--verify-ad21-ba` owner-ROM gate stays opt-in so public CI does
not silently claim original-ROM execution.

To repeat privately, with the owner ROMs unpacked locally:

```sh
cmake -S . -B build -DVF2_BUILD_TESTS=ON \
  -DVF2_ROM_DIR=/path/to/unpacked/vf2 \
  -DVF2_VERIFY_AD21_BA=ON
cmake --build build --target vf2_m68k_frame342_dispatch_tests
ctest --test-dir build --output-on-failure \
  -R '^vf2_m68k_frame342_ad21_ba_strict_owner_rom$'
```

## Original-ROM AD21 B9..BE strict matrix: 12/12 (October 20, 2026)

The owner-original Motorola 68000 dispatcher was executed with the verified
VF2.1 ROM set, via the Linux differential binary built by GitHub Actions
commit `1d9a9fc24686b57e919302f98696b3289c119934`. The
`--verify-ad21-b-all` run exited **0**, with twelve distinct
`FRAME342_AD21_B_MATRIX_STRICT parity=1` checkpoints. The original L2
tick also generated twelve packets in 471 instructions with zero RAM/SCSP
differences against C, outside its temporary original interpreter stack.

Each B packet was independently dispatched from the corresponding
post-L2 snapshot (isolated queue count=1 and read cursor=packet),
both in recovered C and in the original-ROM MC68000 stepper. The strict
gate asserts the command/byte-1 selector, handler and matched-descriptor
count, command retirement, original instruction count and stop PC
`0x265c`, exactly three non-stack RAM bytes changed on each side,
zero RAM mutation mismatches, identical/unchanged SCSP registers and
slot activity, and zero new PCM sample-area writes.

| AD21 packets | Original MC68000 instructions | Matched descriptor rows | RAM/SCSP mutation mismatch |
| --- | ---: | ---: | ---: |
| B9, BA, BB, BD `2A 00` | 256 each | 1 each | 0 |
| BC, BE `2A 00` | 263 each | 2 each | 0 |
| B9..BE `07 7F` | 304 each | 1 each except BC/BE: 2 | 0 |

The prior L2 tick used temporary stack bytes around `0x7f00`, so the
post-L2 absolute RAM states can include unrelated existing stack residue.
This test correctly compares changes from each side's independent initial
state and excludes only the current 68000 stack at `0x4f00..0x4fff`.
It does not normalize the modeled program's mutations to force a pass.

Run with the owner's ROM, never added to GitHub:

```sh
cmake -S . -B build -DVF2_BUILD_TESTS=ON \
  -DVF2_ROM_DIR=/path/to/vf2-roms \
  -DVF2_VERIFY_AD21_B_ALL=ON
cmake --build build --target vf2_m68k_frame342_dispatch_tests
ctest --test-dir build --output-on-failure \
  -R '^vf2_m68k_frame342_ad21_b_all_strict_owner_rom

The 12 newly filed packets are `B9..BE` B-class requests.
A single B-class dispatcher entry, timer progression and
possible voice programming require further **original-MC68000
per-packet** differentials. Do not infer that AD21 itself
produces audible PCM, nor that the observed KICK input caused
a fighter-to-fighter collision. This remains an isolated
original-audio-ROM differential rather than time-accurate
concurrent original dual-CPU scheduling.

```

For a non-asserting inventory, use `--probe-ad21-b-all` instead.
**This is isolated dispatcher parity, not sequential queue parity,
real arcade timing, or proof of audible PCM.** All twelve original
packets leave the modeled SCSP and PCM sample area unchanged in this
checkpoint. The demonstrated `9E` allocator/KEYON path and scheduling
remain better targets for the sought punch/shout sound.

## Original-ROM **stateful sequential** AD21 B drain (October 20, 2026)

Follow-up to the isolated 12/12 B-class matrix, using the owner's verified
`vf2(20261018-203049).zip` and the actual compiled
`vf2_m68k_frame342_dispatch_tests` artifact. The source and strict
option were added on `master` in commits `f6f975d` (probe),
`07f4daf` (asserting gate), and `81e8218` (CTest registration).
The strict executable was run locally against the owner's unpacked ROM
without adding copyrighted chip bytes to the repository.

**Observed strict run: exit status 0, exactly 12/12**
`FRAME342_AD21_B_SEQUENCE_STRICT packet=N parity=1` checkpoints.
The test DOES NOT restore RAM/command descriptors/SCSP between commands;
only the 68000 call registers and PC are reset to their driver entry
conditions for each request. C also consumes the live queued packets
with `vf2_sound_board_dispatch_next` and never resets its state.

| Phase | Original MC68000 instructions | C/original mutable-state delta difference |
| --- | ---: | ---: |
| B9, BA, BB, BD `2A 00` | 256 each | 0 |
| BC, BE `2A 00` | 263 each | 0 |
| B9–BE `07 7F` | 304 each | 0 |
| **Total** | **3,374** | **0** |

Each command changed exactly three non-stack RAM bytes on each side,
with zero differences in *writes relative to that command's own
initial mutable state*, zero SCSP register differences and zero
PCM-area changes. Every command retired correctly. Final queue:
`C_count=M_count=0`, read cursors `C=M=0x0044`.
The strictly validated upstream original AD21 L2 tick had already
produced the exact 12-packet ring in 471 instructions.

Run manually with the legally obtained ROM set in a local directory:

```sh
cmake -S . -B build -DVF2_BUILD_TESTS=ON \
  -DVF2_ROM_DIR=/local/vf2-roms \
  -DVF2_VERIFY_AD21_B_SEQUENTIAL=ON
cmake --build build --target vf2_m68k_frame342_dispatch_tests
ctest --test-dir build --output-on-failure \
  -R '^vf2_m68k_frame342_ad21_b_sequential_owner_rom

The 12 newly filed packets are `B9..BE` B-class requests.
A single B-class dispatcher entry, timer progression and
possible voice programming require further **original-MC68000
per-packet** differentials. Do not infer that AD21 itself
produces audible PCM, nor that the observed KICK input caused
a fighter-to-fighter collision. This remains an isolated
original-audio-ROM differential rather than time-accurate
concurrent original dual-CPU scheduling.

```

For diagnostics without strict pass/fail checks, use
`vf2_m68k_frame342_dispatch_tests /local/vf2-roms --probe-ad21-b-sequential`.

**Scope limit:** This proves sequential *driver-dispatch* effects
from the post-L2 state in a controlled original-ROM execution.
It does not prove authentic scheduler timing, concurrent i960–68000
execution or that AD21 is itself audible. The direct 9E / allocator
/ original KEYON path remains the next sound-producing lead.

## Next unresolved step

The 12 newly filed packets are `B9..BE` B-class requests.
A single B-class dispatcher entry, timer progression and
possible voice programming require further **original-MC68000
per-packet** differentials. Do not infer that AD21 itself
produces audible PCM, nor that the observed KICK input caused
a fighter-to-fighter collision. This remains an isolated
original-audio-ROM differential rather than time-accurate
concurrent original dual-CPU scheduling.
