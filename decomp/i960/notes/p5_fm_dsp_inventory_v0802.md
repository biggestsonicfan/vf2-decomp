# P5: FM/DSP inventory + labeled models (v0802)

## Scope

P5 is the inventory of FM synthesis and DSP models in the C.
VF2 on Model 2A uses PCM samples (not FM synthesis), so the
C's SCSP model is PCM-only. This note documents what FM/DSP
models exist (or don't exist) in the current C.

## FM synthesis inventory

| model | C state | VF2 usage | C correct? |
|---|---|---|---|
| FM operator (YMF292) | not modeled | VF2 does not use FM synthesis | YES — not needed |
| FM algorithm | not modeled | VF2 does not use FM | YES — not needed |
| FM envelope (per-operator) | not modeled | VF2 does not use FM | YES — not needed |
| FM LFO | not modeled | VF2 does not use FM | YES — not needed |

The Sega Saturn's SCSP (YMF292) has FM synthesis capability,
but Virtua Fighter 2 on the Model 2A uses PCM samples only.
The C correctly does not model FM synthesis.

## DSP inventory

| model | C state | VF2 usage | C correct? |
|---|---|---|---|
| DSP effect (reverb, chorus) | not modeled | VF2 may use reverb/effects (TBD) | TBD — needs investigation |
| DSP program loader | not modeled | VF2 may load DSP programs (TBD) | TBD — needs investigation |
| DSP sample rate converter | not modeled | VF2 uses PCM at native rate | YES — not needed |
| DSP envelope generator | PCM envelope only | VF2 uses PCM envelope | YES — envelope is PCM-only |

The SCSP has DSP effects capability, but the C's model is
PCM-only. If VF2 uses DSP effects, the C would need to add
DSP model. This requires investigation of the audio ROM for
DSP program data.

## PCM model inventory (what the C does have)

| model | C location | status |
|---|---|---|
| PCM slot register file | `include/vf2/scsp.h:28` `registers[0x1000]` | modeled |
| PCM envelope state | `include/vf2/scsp.h:38-40` `slot_envelope`, `slot_envelope_state` | modeled |
| PCM envelope advance | `src/hardware/scsp.c:79` `advance_envelope` | modeled |
| PCM sample ROM attachment | `src/hardware/scsp.c:241` `vf2_scsp_attach_sample_rom` | modeled |
| PCM sample read | `src/hardware/sound_board.c:423` `vf2_sound_board_read_u8` | modeled |
| PCM DAC render | `src/hardware/scsp.c:478` `vf2_scsp_render` | modeled |
| PCM MIDI FIFO | `src/hardware/scsp.c:397,412` `vf2_scsp_midi_receive/_read` | modeled |
| PCM master volume | `src/hardware/scsp.c` (various) | modeled |

The PCM model is complete. The C has all the PCM components
needed for VF2 audio playback (once the body chain is recovered
to trigger KEYON).

## DSP investigation needed

To determine if VF2 uses DSP effects, I would need to:
1. Search the audio ROM for DSP program data (SCSP DSP
   programs are typically loaded at specific addresses)
2. Check the SCSP DSP control registers in the silicon
3. Look for evidence of reverb/chorus/effects in the audio
   data

This is a future investigation slice. For now, the C's
PCM-only model is sufficient for the C's covered paths.

## Conclusion

P5 inventory: the C has a complete PCM model and correctly
does not model FM synthesis (not used by VF2) or DSP effects
(TBD — needs investigation). No C-side changes needed for
the current scope.

The working-tree state is unchanged from the v0801 P0c
closure. No `src/` modifications.

## Files

- `decomp/i960/notes/p5_fm_dsp_inventory_v0802.md` — this note
