# Passive natural sound evidence gate (v0882)

This is an **instrumentation milestone**, not a claim that natural gameplay
audio is already reproduced. The original ROM's i960 UART opcode anchors were
checked against the user's locally supplied VF2.1 set (October 19, 2026).

## New command

```sh
python tools/python/audio_natural_sound_gate.py \
  /path/to/vf2.zip out/real-precontact.jsonl --json
```

The trace must come from **genuine reference i960 execution** using
`vf2probe --trace --memory-trace` and a *valid*, frame-advancing
pre-contact or other game-event snapshot. A synthetic sound packet, forced
selector, fabricated trace or boot setup is **not** a gameplay-hit witness.

The script reuses existing **ROM-anchored** tools; it does not infer gameplay
semantics from an address name:

1. `audio_uart_transport.verify_uart_opcodes` validates the original i960
   instructions/operands against the local ROM.
2. `audio_uart_transport.upstream_writes` chains original selector choices to
   the deduplicator and an **exact** queue write in this same trace.
3. `observe_transport` requires queue-slot identity, original filer PC,
   vector-15 ISR, pop, staging, all **three** UART bytes and cursor advances,
   with contiguous steps and invalidation on overlapping/alien writes.
4. `audio_natural_sound_gate.evaluate_transport` refuses inconsistent
   packet bytes, unaligned slot, wrong producer, duplicate producer identity
   or impossible witness counts. It emits **only** samples whose prior
   selector witness was verified; samples are capped and visibly marked
   truncated when the total exceeds that cap.
5. The existing gate-0x50028 causality observer is run independently, and
   its counts are included. **It is not automatically joined** to the
   UART packet: proximity is not proof of event causality.

Output gates separate `selector_to_i960_uart` from
`natural_fighter_event_proven`, `m68k_l3_receive_proven`,
`m68k_c_differential_proven`, `scsp_keyon_from_live_gameplay_proven` and
`audible_live_gameplay_proven`. **Only the first may become true** in
this tool. The latter five remain false until independently verified.

## Regression and evidence

```sh
python tools/python/test_audio_natural_sound_gate.py -v
ctest --test-dir build -R vf2_python_audio_natural_sound_gate --output-on-failure
```

Eleven ROM-free cases cover a verified AD-family transport candidate, missing
selector, mismatched byte, wrong producer/alignment, duplicate producer,
count corruption, capped/empty samples, booleans, unrepresentable commands,
and a non-AD packet.

The first test was run independently as a pure Python test (11/11 passed).
No genuine gameplay trace was available in this change, so **there is no
positive claim** that the new end-to-end CLI observed a fight event.
The original `audio_uart_transport` and native C test suites were not
rerun as part of this instrumentation slice.

## Next required proof

1. Generate and retain **locally** a genuine frame-advancing pre-contact trace.
2. Obtain one verified `selector_to_i960_uart` candidate, plus a separate
   trace proving its game-event origin (not just boot or test-menu traffic).
3. Carry the **exact observed** three-byte packet into the original
   Motorola 68000 L3 `0x526` and dispatcher `0x7a6` timeline; compare
   the recovered C state and stepper RAM/SCSP state at controlled boundaries.
4. Only after original-driver KEYON plus nonzero SCSP output *caused by that
   same session event* can we call this a naturally emitted sound.

ROMs, snapshots, full traces and generated WAVs stay on the owner's
machine, never in Git.

## v0883 — Observed UART packet -> original Motorola 68000 L3 receiver

The ROM-backed C test `vf2_m68k_l3_selection_tests` previously ran only
the two ROM-table A0/L2 known selectors. It now also accepts an **opt-in**
three-byte payload and runs the identical strict **byte-by-byte C vs
original 68000 L3** differential already used by those two defaults:

```sh
# Tested packet format: exactly six hexadecimal characters, no 0x prefix.
# For example only; this packet was NOT captured from natural gameplay.
build/vf2_m68k_l3_selection_tests roms/vf2 --l3-packet AD2342
```

The command runs `vf2_sound_board_file_uart_byte` against original
68000 code at `0x52E..0x5EA`, compares the entire sound RAM (excluding
the documented stepper-only JSR return stack spill) and SCSP register
state after **each byte**, and asserts ring count and final hardware poke.
An invalid, wrong-length or nonhex packet exits before ROM loading.
Default runs still do the original A0/L2 checks.

To replay **only packets with a verified i960 selector-to-UART witness**
from a locally generated gate JSON:

```sh
python tools/python/audio_natural_sound_gate.py \
  /path/to/vf2.zip out/real-precontact.jsonl --json \
  > out/local-audio-gate.json

# Linux/macOS, build after current master:
python tools/python/audio_l3_packet_replay.py \
  out/local-audio-gate.json build/vf2_m68k_l3_selection_tests \
  --rom-zip /path/to/vf2.zip --limit 4

# Native Windows/MSVC:
python tools/python/audio_l3_packet_replay.py \
  out/local-audio-gate.json build/Debug/vf2_m68k_l3_selection_tests.exe \
  --rom-directory roms/vf2
```

The opt-in utility extracts a user-provided ZIP only into an automatically
removed temporary directory, validates file names/count/size, invokes the
existing strict binary with argv (not a shell), requires BOTH zero exit
and the precise L3 receiver match marker, and reports error/timeout as
negative evidence. It never publishes samples, ROM content or raw trace.
The caller is responsible for the integrity of the local gate JSON.
A forged or synthetic report cannot certify natural gameplay.

**Crucial scope boundary:** the observed i960 UART packet and C-vs-68000
L3 test are still **two separately initialized contexts**. They do not
demonstrate correct UART-to-L3 IRQ timing, dynamic gameplay event origin,
full dispatcher behavior, KEYON, or audible PCM in the live game.
One real frame-progressing trace plus contiguous original-68000 state
is still required before promoting an end-to-end claim.

ROM-free regressions:

```sh
python tools/python/test_audio_l3_packet_replay.py -v
ctest --test-dir build -R vf2_python_audio_l3_packet_replay --output-on-failure
```


## v0884 — 98 original sound-command selections, isolated L3 differential

The supported original VF2.1 main-data ROM's **18 profiles x 11 columns**
contain 107 populated cells, 91 empty cells, and **98 distinct command
words**. The two observed table families are AD23 (64 populated cells) and
AD24 (43 populated cells). These are *ROM table entries*, not measurements
of naturally triggered hit sounds.

In addition to the existing two full A0/L2 sample tests and the opt-in
single `--l3-packet` test, the ROM-backed
`vf2_m68k_l3_selection_tests` has an **opt-in matrix mode**:

```sh
# After a strict C17 build using the owner's verified ROM directory:
build/vf2_m68k_l3_selection_tests /path/to/vf2 --matrix-l3

# Native Windows/MSVC:
build\Debug\vf2_m68k_l3_selection_tests.exe roms\vf2 --matrix-l3
```

The matrix enumerates the actual 18x11 sound-command table, refuses
unexpected nonempty foreign command families, deduplicates command words
and tests each unique command against the *original Motorola 68000 L3*
receiver and recovered C after each byte. For the supported set the
completion gate requires **107 occupied, 98 unique, all 98 L3
differentials matching**. If any mismatch occurs the binary exits nonzero
and prints the exact command and profile/column.

This is **not** 98 proven hit sounds. L3 receives exactly three bytes in a
controlled, separately ROM-initialized sound-board fixture for each
selection. Natural event provenance, contemporaneous IRQ timing, full
AD-family dispatcher-to-SCSP behavior and original hardware DAC fidelity
are still open. The suite cannot be certified as passing until the user
ROM is actually exercised with the compiled binary; a successful
ROM-independent GitHub CI build alone does not make this ROM-backed
matrix green.

### Run the 98-command matrix directly from the original owner's ZIP

Instead of permanently extracting the ROM files, use the temporary ZIP
runner (all files are deleted after the process terminates):

```sh
python tools/python/audio_rom_matrix_l3.py \
  build/vf2_m68k_l3_selection_tests \
  --rom-zip /path/to/owner/vf2.zip

# Windows/MSVC binary:
python tools/python/audio_rom_matrix_l3.py \
  build/Debug/vf2_m68k_l3_selection_tests.exe \
  --rom-zip C:/path/to/vf2.zip
```

A zero exit code **and** an exact `107 occupied, 98 unique, 98 passed`
summary from the C executable are required. The wrapper rejects missing,
partial, truncated or contradictory evidence, and reports negative status
on process failures or timeouts. This still tests the **isolated original
ROM L3 receiver against recovered C**, not naturally occurring fighter
sound or hardware-level PCM equality.

ROM-free unit gate:

```sh
python tools/python/test_audio_rom_matrix_l3.py -v
ctest --test-dir build -R vf2_python_audio_rom_matrix_l3 --output-on-failure
```

## v0893 — Frame-progressing original i960 session audio evidence (opt-in)

This is a passive **frame-level hunt**, not an original instruction-PC
trace and **not** proof of a genuine fighter-hit sound. It supplements
the existing ROM-anchored `vf2probe --trace --memory-trace` chain
without weakening its acceptance criteria.

The SDL3 host now offers an explicit evidence output, only with
original i960 **oracle** frame execution:

```sh
# Use a valid, real pre-contact gameplay snapshot; do not inject
# sound-ring commands or synthetic collision state.
mkdir -p out
vf2_host --rom-dir /local/owner/vf2 \
  --snapshot /local/owner/precontact.vf2snap \
  --oracle --fast --frames 900 \
  --verified-audio-copy \
  --audio-evidence-jsonl out/precontact-audio-frames.jsonl

python tools/python/audio_frame_evidence_report.py \
  out/precontact-audio-frames.jsonl
```

The optional `--verified-audio-copy` switch only enables the
previously validated native C `D92->DF8->ED6` sound-engine branch.
The tool **never injects** a command, forces an IRQ, or edits a
snapshot. If none is observed, a valid capture is simply silent.

One record is written for **each completed oracle frame**, after the
normal shadow-i960 sound pump and SCSP software render. Fields include:

- Frame index and raw input mask.
- i960 sound-queue **count immediately before and after the
  original oracle frame**, prior to pump service. Queue growth is
  a *temporal candidate*, not causal proof of a hit or specific PC.
- Exact UART bytes that the pump's **shadow** i960 drainer forwarded
  and that the modeled L3 receiver accepted, in order, retained
  **un-grouped as hex** because partial packets can cross frames.
  `uart_count`, `bytes_moved` and `uart_truncated` make any
  loss of the retained byte sample detectable.
- Sound-ring entries filed/dispatched/refused, number of guarded
  `D92->ED6` completions and drain/stream/voice status.
- Stereo SCSP **model** nonzero-frame count and PCM peak for the
  current frame, not original arcade DAC measurements.

The `origin` string is deliberately
`oracle_frame_shadow_uart_pump`. Original i960 game code stepped
the frame, **but UART transfer is performed by the shadow drainer,
not a captured original vector-15 ISR instruction trace**. The
record explicitly sets `natural_fighter_hit_proven`,
`original_uart_irq_proven` and `hardware_pcm_proven` to false.

The companion analyzer:

```sh
python tools/python/test_audio_frame_evidence_report.py -v
ctest --test-dir build \
  -R vf2_python_audio_frame_evidence --output-on-failure
```

validates monotonically contiguous frame indices, transport byte
accounting, queue/dispatcher/PCM fields, unsupported truth claims
and the real-frame provenance marker. It reports candidate frames
when queue count **increased** during the oracle frame and
shadow UART bytes were forwarded. A preexisting queue or continued
PCM sound is not promoted into a newly authored game event.

**Crucial next gate:** use a promising candidate's original frame
state to capture original i960 PC+memory trace proving selector,
queue writer `0x43924`, and true vector-15 UART ISR/byte staging.
Then carry the *exact packet* through original 68000 L3 and native
C / SCSP differential. The independent existing strict selectors
and UART transport gates remain authoritative; even an observed
model KEYON plus nonzero PCM alone does **not** prove a natural hit.

No user ROM, snapshot, raw trace, PCM audio buffer or WAV is
committed. The JSONL itself should remain local; generated output
goes to a caller-specified path, not the project tree by default.


## v0894 — Preserve the frame BEFORE an original i960 queue write

The previous v0893 frame-level evidence JSONL is useful only when its
*starting snapshot is available*. It does not save that state, and an
event can be gone by the time somebody tries to reproduce it. The
SDL3 oracle host now has an **opt-in original CPU/machine checkpoint**
saved at the beginning of the first frame in which the **original
interpreter itself** performs an aligned 32-bit bus write to
`0x00504020..0x0050405F` (the 16-entry sound-command ring).

```sh
# ROM, starting snapshot, per-frame evidence and output stay local.
mkdir -p out
vf2_host --rom-dir /local/vf2-roms \\
  --snapshot /local/starting.vf2snap \\
  --oracle --fast --frames 900 --verified-audio-copy \\
  --audio-evidence-jsonl out/audio-frames.jsonl \\
  --audio-pre-event-snapshot out/pre-event.vf2snap

# The snapshot is written ONCE, and only if the original i960
# really wrote a full aligned sound-ring word during an oracle frame.
# No write => explicit diagnostic and NO new snapshot file.
python tools/python/audio_frame_evidence_report.py out/audio-frames.jsonl
```

**What the snapshot contains:** the exact i960 CPU registers plus the
Model 2A machine RAM/IO state **at the start of that frame**, from
`vf2_i960_snapshot_capture`; this is the same proven `.vf2snap`
serializer already used by the developer tools. It is captured
*before* the original frame advances, then retained only if the
sound writer is observed. The memory observer is **active exclusively
during original interpreter stepping**, not during the later
shadow UART sound pump. Saving does not stage a ring write or change
the CPU/machine being stepped.

**Important limitations:** `.vf2snap` does not serialize the
separate SCSP sound-board runtime. When replayed it restores the
original i960 CPU and Model 2A memory, while the sound board is
initialized afresh. It is therefore a checkpoint for tracing the
**i960 producer**, not a time-accurate entire dual-CPU audio
savestate. The observed 32-bit bus write is original interpreter
activity, but the bus observer **does not record the writer's PC**;
and even that write may be menu/music/setup rather than a hit.

Per-frame JSONL adds `original_i960_ring_writes`,
`original_i960_first_ring_slot` and
`original_i960_first_ring_word`, plus
`original_i960_writer_pc_proven=false`. The analyzer:
- validates aligned ring-slot addresses, integers and false PC claims;
- preserves exact first-word evidence and total write-frame count;
- marks `original_i960_ring_write_observed` separately from
  UART delivery, D92 KEYON, natural-hit semantics, and hardware audio.

**Next real proof:** replay the saved `pre-event.vf2snap` under an
instruction-PC+memory tracer, follow the exact ring word from
its authentic writer (expected `0x43924`, to be verified, NOT
assumed) through the original vector-15 ISR, and correlate to the
actual fighter event. No natural combat SFX is certified by this
instrumentation alone.

The mode requires `--oracle`; it refuses native/recovered sessions.
It writes one checkpoint only; additional events remain visible in
JSONL but do not overwrite the first replay point. Tests include
`vf2_host` fail-closed checkpoint requests and a ROM-backed
two-checkpoint determinism check (skips when owner snapshot absent).
`vf2_python_audio_frame_evidence` checks forged addresses and
unsupported IP/hit claims without proprietary ROMs.


## v0895 — Exact instruction-PC witness for original i960 sound-ring writes

The previous frame-capture hook (v0894) could prove an original
`0x00504020..0x0050405F` bus write, but not the particular instruction
that performed it. The session now retains an **active-oracle-step
instruction IP**, snapshotted immediately **before**
`vf2_i960_step` executes and visible only to a Model 2A memory
observer *during that execution*. This matters for branch or ISR
instructions where `cpu.ip` is changed before a post-step observer
would inspect it.

The new read-only API is:

```c
vf2_status vf2_session_current_oracle_instruction_ip(
    const vf2_session *session, uint32_t *instruction_ip);
```

- Outside the active original i960 instruction, this returns
  `VF2_ERROR_UNSUPPORTED`, not a stale or guessed PC.
- NULL arguments are refused. Native/recovered sessions cannot
  report this original-instruction witness.
- During one aligned original i960 4-byte sound-ring bus write,
  `vf2_host` records the address, original 32-bit word, and exact
  pre-instruction IP. The existing pre-frame `.vf2snap` is still
  captured before the original frame and only persisted after
  observing the first write.
- The JSONL now has `original_i960_first_writer_pc` and
  `original_i960_writer_pc_proven`. When the observer actually
  obtains the original instruction's PC, the boolean is true; if
  not, the address is zero and the flag stays false. The analysis
  validates flag/PC alignment, writes and slot coherence.
  It separately counts `frames_with_original_i960_writer_pc`.

Sample output is **illustrative only**, not a captured real hit:

```json
{
  "original_i960_ring_writes": 1,
  "original_i960_first_ring_slot": 5259296,
  "original_i960_first_ring_word": 11346754,
  "original_i960_first_writer_pc": 276772,
  "original_i960_writer_pc_proven": true,
  "natural_fighter_hit_proven": false
}
```

The actual writer PC **must be taken from the recorded frame**,
not a guessed `0x43924`. The JSONL is locally generated evidence,
not authenticated/cryptographically attested; replay a captured
`.vf2snap` under an independent original-PC instruction trace to
confirm the writer before elevating conclusions.

This proves only the writing *instruction* for the first ring word.
It does not on its own establish that the i960 sound was triggered
by a fighter collision, that the original vector-15 ISR transmitted
this exact word, that a real 68000 interrupt reached the L3
receiver, or that modeled PCM matches Model 2 SCSP DSP output.
The separately recovered D92 C branch remains explicitly
opt-in. No original ROM, original snapshot, or generated trace is
added to Git.

Regression gates are the ROM-free Python
`vf2_python_audio_frame_evidence` CTest and `vf2_host` invalid
arguments, plus original-ROM `vf2_session_uart_probe` witness checks
when a local boot snapshot is available. If the snapshot is not
present, the latter test skips rather than fabricating a PC.
