# Selector-3 rectangular helpers (v0.0.26)

The selector-3 phase-zero branch at `0x0000ae78` contains two large rectangular display helpers before the already recovered `display_profile_apply` call.

ROM measurement with the real selector-3 dimensions (`48 x 62`) establishes:

```text
0x00008ef0  display_rect_fill_16     12,146 instructions including ret
0x00008f1c  display_descriptor_blit  21,186 instructions including ret
```

The real descriptor at `0x02a6c15e` decodes to:

```text
signed addend = -32768
mode          = 1
rows          = 48
columns       = 62
```

`0x8f1c` is now executed as an architecturally-accounted recovered child of the selector-3 bridge. Including its caller instruction, it peels 21,187 instructions and one call/return from the synthetic aggregate.

Combined with the previously composed `display_profile_apply`, the controlled-vector opaque residual changes from:

```text
33,265 instructions
6 calls
6 returns
```

to:

```text
12,078 instructions
5 calls
5 returns
```

The residual remains calculated dynamically from the recovered child reports; the total externally observed selector-3 accounting remains unchanged.

`0x8ef0` is also bounded and measured, but is not yet debited from the aggregate. Its 12,146-instruction body (12,147 including the caller) is 69 instructions larger than the current 12,078 residual. The surrounding direct instructions and exact phase-zero accounting boundary therefore need to be reconciled before composing that final large peel instead of forcing the numbers to fit.
