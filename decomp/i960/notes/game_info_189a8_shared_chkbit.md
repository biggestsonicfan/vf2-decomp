# fa_game_info shared CHKBIT/ALTERBIT tail (`0x189a8..0x189bc`)

Differential validation of the type-22 corridors exposed a previously implicit condition-code mismatch. The shared `0x18644` tail computes `(r10 >> 5) ^ fighter0.flags`, executes `CHKBIT 10`, and feeds that condition directly to `ALTERBIT 3,r10`. Later `bbs`/`bbc`/`cmpob*` branches do not replace the arithmetic condition in the reference executor, so this CHKBIT remains observable at the task boundary unless `0x18bd4` executes its later `CHKBIT 10`.

The native bridge now reproduces both the bit-3 accumulation and the condition state instead of forcing `COMPARE_NONE` at return. This is shared semantics for both `0x164ac -> 0x18644` and `0x164c0 -> 0x18644` call sites.
