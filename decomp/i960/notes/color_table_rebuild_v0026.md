# Color table rebuild algorithm (v0.0.26)

`0x00002c38..0x00002de0` is now recovered executable C and ROM-differentially validated.

The routine builds the palette table rooted at `0x00546008`. The table has 28 pages of `0x120` bytes: page 0 is zeroed completely, while pages 1 through 27 each contain 48 RGB triples of three little-endian 16-bit values. The first triple of each generated page is zero and the remaining 47 triples are computed.

Inputs are three `(base, span)` byte pairs at `0x00500234..0x00500239` and three profile scale bytes at `0x005000e0..0x005000e2`.

For page factor `1..27`, each channel step is:

```c
step = factor * span * 28 / 18;
```

Each of the 47 generated samples advances a 32-bit unsigned accumulator and computes:

```c
acc += step;
value = base + (acc >> 8);
if (value >= 256)
    value = UINT32_MAX;
value = (scale * value) >> 7;
write16(value);
```

The `UINT32_MAX` branch is intentional. The ROM instruction is:

```text
subo 1, 0, g1
```

and i960 `subo src1, src2, dst` computes `src2 - src1`, so this produces `0xffffffff`, not `255`. Differential execution exposed and corrected the earlier provisional interpretation.

After page 27, the epilogue writes:

```text
0x00546004 = 0
0x00546000 = 1
g1         = 0
condition  = equal
ret
```

The exact instruction accounting is also differential rather than estimated:

```text
instructions = 38,938 + sentinel_count
```

where `sentinel_count` is the number of channel samples taking the `value >= 256` branch. There are no nested calls.

Two isolated ROM-backed fixtures were checked against the interpreter. A no-sentinel fixture matched exactly at 38,938 instructions. A fixture with 34 sentinel branches matched exactly at 38,972 instructions. In both cases the generated table, CPU poststate, procedure counters, final condition state and mutable memory were identical.

The bridge now dispatches `0x00002c38` directly to `execute_color_table_rebuild()`. This removes the need to represent this procedure as only the synthetic `0x00546000/04` endpoint and prepares `display_color_profile_apply` (`0x0001fffc`) to compose the real recovered procedure.