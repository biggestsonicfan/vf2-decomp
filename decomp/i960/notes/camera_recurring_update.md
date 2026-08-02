# Camera first recurring update evidence

The first accepted recurring camera interval begins at continuation
`0x0001d458` and ends at the post-update gate entry `0x0001d660`. The live task record is `0x00515400`.

## Proven first-dispatch inputs

```text
task flags:       0x80000000
runtime flags:    0x00008800
input index:      0
input flags:      0x0006
camera mode:      1
mode target:      0x0001f148
range value:      0x41000000
vertical limit:   0xbe99999a
```

Input flags 3, 4, 5 and 6 are clear. Therefore the optional debug adjustment,
integer conversion, derived-state calculation and mode-specific dispatcher all
take their observed early-return branches.

## Camera mode table

The table begins at `0x0006e2e4`:

| Mode | Target |
|---:|---:|
| 0 | `0x0001f144` |
| 1 | `0x0001f148` |
| 2 | `0x0001f1c0` |
| 3 | `0x0006ad90` |
| 4 | `0x0006aec4` |
| 5 | `0x0006b19c` |
| 6 | `0x0006b27c` |
| 7 | `0x0006b2e8` |

Only mode 1 is accepted by the current recovered prefix.

## Recovered effects

- stores the two observed camera scratch constants at task offsets `0x5c` and
  `0x60`;
- repeats the observed state reset through target `0x0001f148`;
- leaves the arithmetic scratch port at `0x00884000` with the first camera
  component, matching the interpreted memory state;
- evaluates complete range helper `0x000214dc` and writes its byte result to
  task offset `0xfa`;
- takes the early return of `0x00020558` after setting task flag bit 8;
- clears globals `0x00500174` through `0x00500180`;
- derives the two fighter-profile globals at `0x0050109c` and `0x005010a0`;
- clears `0x005010e8` and `0x005010ea`;
- takes the early return of `0x0001fc00` and clears task flag bit 0.

The original and recovered modeled memory match at `0x0001d660`. The complete
range classifier also matches the original helper for nine independent
directional and boundary cases.

## Post-update gate

At `0x0001d660`, input flags are read again. The observed value is `0x0006`, so
bit 3 is clear and the viewport-construction block `0x0001d678`–`0x0001d8e4`
is skipped. Control byte `0x0050009c` is then `0x01`; bit 0 branches directly to
return block `0x0001e524` without further writes.

The recovered helper also covers the complete non-viewport control path through
`0x0001d984`: task flag bits 1 and 2 are derived from control bits 1/2, task byte
`+0xde`, runtime mode/phase bytes and override byte `+0x2d4`. The input-bit-3
viewport path remains explicitly unsupported.

## Boundary clarification

No geometry RAM byte changes during the first `fa_camera` dispatch. Temporary
writes at coprocessor-port offset `0x4000` behave as arithmetic scratch in the
current evidence-bounded model. Actual geometry/TGP submission must be located
later and is not claimed by this release. After all seven tasks return, the
scheduler resumes at stable checkpoint `0x00010dcc`.
