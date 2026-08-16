# Selector 17 index 6 page 1 validation

The recovered BOOKKEEPING 1/5 renderer preserves the ROM tile attribute spans in addition to visible text. The `a5=0` layout and `a5=1` empty/default value phase are validated from clean `0x0000a6c0` snapshots; their measured instruction/call deltas are 15309/25 and 1815/79 respectively, with one additional procedure return at the frame boundary.

The undefined PLAY TIME RATIO field is centered at tile columns 35..38. This is distinct from the highlighted value span (33..38), whose leading two cells remain spaces.
