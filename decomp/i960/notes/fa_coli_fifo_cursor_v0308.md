# v0308: coli `g8+0x26` FIFO cursor path

## Verdict

`coli_22404_body` recovers the measured cursor path taken when
`g8+0x26 != 0` on the non-empty contact (`g0 = 1`):

```text
ld   0x005001e4, r15        # byte cursor
subi r14, 0, r14            # r14 = -delta
st   r14, 0x0090e000(r15)
addo 4, r15, r15
stob r15, 0x005001e4        # cursor += 4 (byte store)
lda  0x36806d6d, r15
st   r15, (g11)[g12]        # command-port word
# then the shared 0x03000606 header
```

One-hit slot-0 body becomes **79** (72 − 2 skip + 9 cursor+shared).
Measured **80** including ret.

## Drive

From `out/coli-22404-e1.vf2snap`:

```text
--set-u32 0x00510b24=0x100
--set-u8  0x005111a0=0x01
--set-u32 0x005149cc=0xffff
--set-u16 0x005129a6=0x2
--until 0x0002222c
```

Memory: `0x90e030 = 0xfffffffe` (`-2`), cursor byte `0x30 → 0x34`.

## Proof

- unit test cursor `80/0/1`, `-2` at `0x90e010`, cursor `0x10 → 0x14`
- ctest Debug **56/56**
- PUNCH **320/320 MATCH**
- input-17 **64/64 MATCH**
