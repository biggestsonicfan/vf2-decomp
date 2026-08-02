# Camera viewport construction

## Boundary

The optional camera block begins at `0x0001d678` and rejoins the recurring
camera body at `0x0001d8e8`. It is selected by input-table bit 3. The supported
first-dispatch state has flags `0x0006`, so this block is validated with private
synthetic entry states rather than claimed as naturally executed.

## Recovered helpers

- `0x0001fbb4` constructs lower/upper bounds around the global camera center and
  stores them at `0x0050a0d0`/`0x0050a0d4`.
- `0x0001eff0` projects both fighter positions into profile multipliers and
  signed weights stored at `0x0050109c`, `0x005010a0`, `0x005010e8` and
  `0x005010ea`.
- `0x0001facc` selects values through ROM pointer tables and fills the task
  arrays.

## Task outputs

- task offset `0x100`: eight entries;
- task offset `0x150`: ten entries;
- calculated first-table path sets task flag bit 0;
- fixed and calculated paths both preserve the original coprocessor scratch
  state.

## Validation

`vf2i960 compare-camera-viewport` compares complete modeled memory for two
states: a fixed-table state and a calculated/interpolated state. Both match the
original i960 block byte for byte. This is a camera-data preparation block, not
a proven geometry submission.
