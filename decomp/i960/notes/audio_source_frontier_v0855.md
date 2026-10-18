# v0855 — audio source correction and i960 field-writer census

Status: **ROM-backed static investigation; no behavioral C admission**. This
corrects the v0854 suggestion that fighter/entity `+0x1b1` is a sound-event
identifier. The already-measured player logic identifies it as the **fighter
type/character selector** (see `fa_player_drive_base_v0293.md`,
`fa_player_19ef8_29414_defer_v0295.md`, and `fa_player_1fcc0_boundary_v0748.md`).
It may participate in *choosing* a sound table; it is not evidence that
writing the byte triggers a sound.

## ROM check

Using the user's local MAME `vf2` set and
`config/vf2_v22_roms.csv`'s 4-way `LOAD32_WORD` interleave, an
aligned direct-MEMA scan of the 0x80000-byte populated maincpu region
produced the following **candidates** (not reachability claims):

| Field | Aligned reads | Aligned writes | Structural proof |
|---|---:|---:|---|
| fighter `+0x1b1` | 142 (`ldob` 100, `ldib` 42) | 86 (`stib` 82, `stob` 4) | 40 writes immediately copy the prior `+0x1b0` byte via the same register/base |
| fighter `+0x624` | 5 (`ldos`) | 39 (`stis` 36, `stos` 3) | all 36 `stis` have adjacent `mov 0,r15`; one `stos` at `0x54ad4` follows `mov 0,r3; stob r3,+0x2a` |

**Critical ROM anchors:**

- `0x4396c`: `ldob +0x1b1(g7), r14`, in the sound command path.
- `0x43974`: `ldos +0x624(g7), r14`, in the same path.
- `0x14190`: `stos r3,+0x624(g7)`, among a cluster of field stores;
  not yet classified for value or reachability.
- `0x23b44`: `stos g3,+0x624(g7)`, immediately after the
  `call 0x23878` at `0x23b40` inside `fa_coli` (`0x2396c`). The
  earlier **measured** collision/poly-cluster study v0283 already
  captured exactly three bit-remap calls per invocation and this
  post-remap store. It is a dynamic writer, **NOT evidence of a
  sound-event producer**. See `fa_coli_2396c_measure_v0283.md`.
- `0x54ad4`: `stos r3,+0x624(g7)` immediately after a local
  zeroing sequence (reset, not an event trigger).

The static scanner deliberately excludes MEMB/indexed accesses, indirect
writes, and real machine reachability. Alignment alone can include data.
The 37 demonstrable zeroing shapes do not prove the other two reachability
or nonzero behavior. Do not turn this into a 9/10 audibility claim.

## Reproduce (without committing ROMs)

```sh
python tools/python/test_audio_field_xrefs.py -v
python tools/python/audio_field_xrefs.py /path/to/vf2.zip
python tools/python/audio_field_xrefs.py /path/to/vf2.zip --json > out/audio-field-xrefs.json
```

Five ROM-free regression cases cover MEMA decoding/refusals, byte-copy
classification, adjacent zeroing, `LOAD32_WORD` layout, and bad-input
refusal. The locally supplied VF2 ROM produced the counts above. The
tool neither emits ROM bytes nor writes extracted files.

## Next recovery gate

1. Reuse the already-measured `fa_coli` `0x2396c`/`0x23878`
   differential to attribute the actual `+0x624` values to the
   poly-cluster bit-remap, **without treating them as audio events**;
   separately audit `0x14190` and unmodeled MEMB/indirect writers.
2. In a *valid live* pre-action fighter snapshot, passively watch writes
   to `fighter_base + 0x624`, then `0x43950` / `0x43888`
   and the existing vector-15 UART drain. Record producer PC, actual
   short value, fighter base and frame; do not stage a packet.
3. Pair the observed producer with the sound-board C-vs-68000-stepper
   differential and a non-silent WAV from the natural session.
4. Promote recovered C only after state/SCSP equality at a controlled
   boundary. Unknown paths remain `VF2_ERROR_UNSUPPORTED`.

This note **supersedes only the v0854 `+0x1b1` sound-ID wording**.
The independently measured queue, L3, L1/L2, dispatcher and key-on
work is unchanged.
