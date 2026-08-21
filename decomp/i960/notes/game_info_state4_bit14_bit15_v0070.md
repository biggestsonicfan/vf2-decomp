# game-info state4 + bit14 + bit15 recovery v0070

The `state4 + bit14 + bit15` corridor was already semantically native through the broad state4/bit15 admission. Differential fixtures derived from the real `0x1645c` entry confirmed exact architectural state for fighter0, fighter1, and both-fighter cases.

Measured ROM/native instruction totals before accounting were `896/892`, `896/892`, and `1083/1075`, while calls, returns, interrupt entries, and interrupt returns already matched. The remaining delta decomposed exactly as `+4` instructions per fighter with state 4 and isolated bits 14+15 set.

The accounting correction is deliberately guarded so bits 6 and 16 remain excluded until their bit15 combinations are measured independently.

Final differential validation:

- fighter0: 896 instructions, exact snapshot, all five counters delta zero;
- fighter1: 896 instructions, exact snapshot, all five counters delta zero;
- both fighters: 1083 instructions, exact snapshot, all five counters delta zero.

The temporary probe PR was closed without merge; the clean candidate was promoted by fast-forward.
