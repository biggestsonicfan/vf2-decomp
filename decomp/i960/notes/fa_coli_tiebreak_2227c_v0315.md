# v0315: coli mid-body tie-break `0x2227c`

## Verdict

When both contacts hit, both `+0x804` bit 15 are clear, and
`f1+0x822 <= f0+0x822`, the cascade falls through `bl`/`bg` (which do
not inherit the `cmpob*` compare) into the `0x2227c` double-call
tie-break.

Recovered as two body-only `coli_225cc_body` calls:

1. `call 0x22290` → `call 0x225cc` with the swapped assignment
2. restore `r7/r8` → `call 0x225cc` with the original assignment

Compact-both measured **282 / 7 / 8**. Parent 32 + children
`0x22298`×2 + `0x22404`×2 + compact `0x225cc`×2.

## Drive (compact-both)

From `out/coli-midbody-22210.vf2snap` with the v0306 both-hit
mutations, plus:

```text
f0+0x1a4 bits 8+1+3, f1+0x1a4 bits 8+1+3   # both compact
f0+0x804=0, f1+0x804=0                     # both bit15 clear
f0+0x822=0, f1+0x822=0                     # pair equal → tie-break
```

## Proof

- unit test in `test_coli_midbody_both_contact_cascade`: 282/7/8,
  final `g7=fighter0`, `g8=fighter1`
- pair-greater 258/5/6 unchanged
- PUNCH **320/320 MATCH**
- input-17 **64/64 MATCH**
- ctest Debug

## Open

- long-body second call (bit 3 clear on original `g8`) is supported
  by `coli_225cc_body` but not yet a composed midbody unit-test pin
- `0x18bd4` shortcut remains DEFER (v0291 multi-block)
