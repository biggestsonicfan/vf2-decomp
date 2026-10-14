# S4a: sample tail + production chain control (v0825)

Status: 0xc1e tail recovered (door 0xc68); production-shape
chain differential C==S over full RAM+SCSP (365 steps, one
documented byte excluded); render determinism + voice-count.
Sound gate green. Audibility (real banks + key staging) is S4b.

## Tail (0xc1e, 10 insns -> 0xc68 door)

d3 = (voice+6 & 0x7ff) * 20; SCSP[slot+8] = long($5000 entry +
0x10); voice+0xb = ROM[0x600c48 + (byte(entry+0x13) & 0x1f)];
refuse at the engine. d3 arithmetic can't leave RAM. Legs roll
forward (a8c-hi/arm 304 steps, synth 94/85).

## Chain (test_chain_production, 365 steps)

Real init (0x80000 audio + 3MB zero window) + real boot
descriptors + real tables/upload; packet {0x9a,0,1} (one-off
survey: 64 door-hits across real descs 10,11,13-18; desc10
pinned); one staged voice (+9) + $152e. Full-512KB sweep:
ONLY 0x2506 differs (count held vs drain-subq'd -- documented,
with the cursor + stack excludes). Zero bus events. $5000 and
rate bytes are real upload data here.

## Render (test_chain_render, C-only)

1 authored voice (+2 = 0x58 absolute), $152e back to 5;
render-twice-identical from snapshots (silence on the zero
window -- expected, S4b owns audibility).

## The 0x60c448 saga (read this before touching ROM addresses)

The tail first shipped with the rate table at 0x60c448 --
legs failed C=02/S=00 on zero tables. What followed was ~2h of
wrong theories (decoder bugs, RAM reads, stale files, T1'/T2
phantoms), each "confirmed" by botched probes (sign-extended
seeds, misaddressed stages, a segfaulting walk, misread dumps).
Findings, in order of realization:
1. tail-c instrumentation (v6/d3/entry/idxbyte/rate/rom in ONE
   run) showed d3 = 0x1900 -- the repeated "0x2800" was bad
   mental hex (0x140*0x14 = 0x1900). Instrument the C FIRST.
2. In-test region dump (the ONLY trustworthy artifact -- out/
   files are NOT ground truth) gave the true table
   `00 a0 96...` at 0x0c48: idx0 -> 0x00, idx7 -> 0x64, both
   matching silicon exactly. The MODEL was right.
3. So the C read the wrong address: 0x60c448 (offset 0xc448,
   a ramp holding 0x02) instead of 0x600c48 (offset 0x0c48).
   ONE DROPPED HEX ZERO. The debugging confusion (0xc448 vs
   0x0c48 dumps) was the same typo mirrored.
4. The oracle was right all along; every exotic theory was
   wrong. Count hex digits against the print (4-digit 0x0c48
   -> 0x600c48, not 0x60c448). The other six ROM bases in this
   file are 6-digit and correct.
Lesson costed properly: evidence (one instrumented run) beat
an hour of speculation. AGENTS.md rule 1 exists for this.
",
