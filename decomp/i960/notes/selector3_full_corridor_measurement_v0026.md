# Selector-3 full corridor measurement (v0.0.26)

This note records the controlled ROM measurement used to replace the selector-3 phase-zero final-cluster correction with an in-place recovered composition. It records measurement and implementation structure before the final full native-snapshot equivalence result.

## Controlled corridor

The measured frame-dispatch corridor starts at `0x0000a6c0` with selector 3 / phase 0 and runs until the framed caller return sentinel. The ROM executes exactly:

- **123,900 instructions**
- **15 procedure calls**
- **16 procedure returns** (the 15 nested returns plus the outer `0xa6c0` return)

The observed call tree is:

1. `0x0000a6f0 -> 0x0000acf8`
2. `0x0000ad24 -> 0x0000ae78`
3. `0x0000aec4 -> 0x00008ef0`
4. `0x0000b120 -> 0x00008f1c`
5. `0x0000b334 -> 0x0000d918`
6. `0x0000b36c -> 0x0001fcc0`
7. `0x0001fdd0 -> 0x0001ff0c`
8. `0x0001ff0c -> 0x0001fee4`
9. `0x0001fe60 -> 0x0001fffc`
10. `0x0002004c -> 0x00002c38`
11. `0x0001fe74 -> 0x0004b410`
12. `0x0001fed8 -> 0x0002eab8`
13. `0x0002ec1c -> 0x00031004`
14. `0x0001fedc -> 0x00011704`
15. `0x0000ad2c -> 0x00001344`

## Exact accounting decomposition

The independently measured child contributions including their caller `call` instruction are:

- `0x00008ef0`: **12,147**
- `0x00008f1c`: **21,187**
- `0x0000d918`: **11**
- `0x0001fcc0` and its eight-child tree: **90,373**
- `0x00001344`: **9**

These sum to **123,727** instructions. The remaining direct/wrapper instructions in `0xa6c0`, `0xacf8`, and `0xae78` are therefore **173**, giving the exact ROM total of **123,900**.

This also explains the former 69-instruction discrepancy around `0x8ef0`: the old `123,638` value was a post-hoc correction layered on top of a semantic shortcut that had already accounted for overlapping phase-zero work. It was not the instruction count of the phase-zero corridor itself.

## Semantic corrections implied by the trace

The ROM descriptor at `0x02a6c15e` is interpreted by `0x8f1c` as signed addend, mode, row count at `+4`, column count at `+8`, and one continuous payload beginning at `+12`. The observed descriptor is mode 1 with **48 rows x 62 columns**, and each destination row advances by `0x80` bytes.

`0x8ef0` likewise fills **48 rows x 62 halfwords**, at a `0x80`-byte row stride; it is not a generic 64-column plane clear.

After the phase-zero worker, `0xacf8` calls `0x1344`. On the measured selector-3 state that helper returns nonzero, causing the ROM to return early from `0xacf8` instead of entering the common selector cleanup. The measured final selector/phase are therefore **selector 3 / phase 3**. This early-return behavior is the reason the old post-final-cluster correction had to undo selector/phase state after the fact.

Implementation commit `f2f39dbfd4f24656bd344318031fceec6734e38f` moves this behavior into the selector-3 phase-zero dispatcher itself. Cleanup commit `085b997fec93e69c66dc6254f3df838c0925f286` removes the now-unreachable selector-3 post-final-cluster correction, including its `123638` instruction delta and residual call/return accounting. The cleanup was validated with the Clang ASan/UBSan configuration and `ctest` before commit.

Full native-vs-ROM snapshot equivalence is validated separately after building the cleaned implementation.
