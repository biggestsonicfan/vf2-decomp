# Camera post-update gate evidence

## Addresses

```text
start:                    0x0001d660
viewport branch:          0x0001d678
normal control boundary:  0x0001d984
observed fast return:     0x0001e524
post-dispatch scheduler:  0x00010dcc
```

## Observed first-dispatch state

```text
input index:    0
input flags:    0x0006
control byte:   0x01 at 0x0050009c
```

Input bit 3 is clear, so the block that creates camera viewport/point tables is
not executed. Control bit 0 is set, so execution branches from `0x0001d8f0` to
the `ret` at `0x0001e524`. The recovered C path performs no memory writes and
matches the original modeled memory at that boundary.

## Recovered alternate control path

When control bit 0 is clear and input bit 3 remains clear, the recovered code:

- derives task flag bit 1 from control bit 1;
- derives task flag bit 2 from task byte `+0xde` and control bit 2;
- applies runtime-mode overrides for modes 8/9 once phase byte `0x00500031` is
  at least 16;
- clears bits 1/2 according to task override byte `+0x2d4`;
- stops at `0x0001d984` before renderer-command construction.

The viewport path selected by input bit 3 is not generalized and returns
`VF2_ERROR_UNSUPPORTED`.
