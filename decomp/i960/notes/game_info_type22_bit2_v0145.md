# Game-info type-22 bit-2 recovery (v0145)

The remaining native `fa_game_info` rejection for a coherent type-22 descriptor was isolated with the supported VF2 v2.2 ROM set and recovered without embedding ROM data or snapshots.

At `0x00018c64`, fighter flag bit 2 selects a 46-instruction child path (47 when the final integer increment executes). The recovered path preserves the ROM float operations and Model 2 command-port traffic through commands `0x0d001a1a`, `0x0c001818`, and `0x0b801717`, including the resulting `g1` value and final floating comparison.

The later `0x00018b58` helper has two measured bit-2-set directions: 30 instructions when fighter `+0x840` bit 0 is clear and 14 instructions when it is set. The clear direction issues command `0x2d805b5b`, applies the returned offsets to fighter X/Z, and both directions update `+0x84`, clear fighter flag bit 2, and clear state bit 4 at `+0x1a4`.

Controlled whole-task differentials from `0x0001645c` to scheduler return `0x00010dcc` now match exactly for both `+0x840` directions: 806 instructions for bit 0 clear and 790 instructions for bit 0 set, with identical CPU state, condition code, registers, mutable memory, procedure-call counts, and procedure-return counts.

The dispatcher tail also preserves the child condition across `CMPobe` at `0x000164f0`; compare-and-branch does not publish a condition code. When the first `0x18644` pass consumes the type-22 bit-2 helper, the later neutral pass must not replace its `NONE` condition with the generic neutral-path synthetic condition.
