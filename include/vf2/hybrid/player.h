#ifndef VF2_HYBRID_PLAYER_H
#define VF2_HYBRID_PLAYER_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

/* Recover the measured fa_player 0x29414 type-0/6/8/10 compact paths.
 * The CPU must already be inside the callee (IP == 0x29414, frame pushed).
 * Type 0 stores zero at +0xc50. Types 6/8/10 with state-flag bit 19 clear
 * store (r9 * scale - scale) using the measured constant sets. Bit-19-set
 * siblings fail closed. */
vf2_status vf2_hybrid_player_29414_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover the measured player flag tail at 0x180bc (warm + siblings).
 * IP must be 0x180bc with a pushed frame. Writes +0x5b4, player-flags
 * bit 8 and optionally +0x6d8; rets to the saved return. */
vf2_status vf2_hybrid_player_180bc_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x1441c..0x14428: set player-flags bit 7 and ret the player task. */
vf2_status vf2_hybrid_player_1441c_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover the measured fa_player 0x29598 bit-4 dispatch
 * (selector2_queue_path). IP must be 0x29598 with a pushed frame.
 * Three paths recovered: skip (g0 bit 4 clear), bbc-taken (g1=0),
 * addo-1 g1 (g1 != 15). Path D (g1 == 15, sub-calls) REFUSED
 * (sub-callees not yet recovered). */
vf2_status vf2_hybrid_player_29598_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x439ac: queue dedup-append (selector2 caller path).
 * IP must be 0x439ac with a pushed frame. Reads count at
 * 0x50406a (1B); if count >= 4 returns immediately. Otherwise
 * searches 0x504074[count+1..1] for g0; if found returns
 * (idempotent). Otherwise writes g0 to 0x504078[count] and
 * increments the count. Replaces the i960 search/append with
 * a counted loop. */
vf2_status vf2_hybrid_player_439ac_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x43888: selector2 queue entry (200 B, 6 paths).
 * IP must be 0x43888 with a pushed frame. Gates on
 * (0x50002c & 0xc) and 0x500068 bit 20 and a g0 magic-value
 * check; on accept, writes g0 to the 16-entry ring buffer
 * at 0x504020 indexed by 0x504003, and updates 0x504001 count.
 * Also pokes the video register at 0xe80004 with 33 and 0x421. */
vf2_status vf2_hybrid_player_43888_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0xcf04: post-frame IRQ handler (184 B, 2 paths).
 * IP must be 0xcf04 with a pushed frame. Sets bit 21 of
 * 0x500068, dispatches on bit 15 of 0x500068 (path A: set ->
 * r3 = *(0x50005b) [+1 if bit 21 of 0x500068 clear] mod 11,
 * store to 0x50005b and 0x500064, write 0x50a700 to 0x50a00c;
 * path B: clear -> r3 = *(0x500054), r3 = *(0x12508[r3*2]),
 * store to 0x500064, write 0x50a704 to 0x50a00c), then
 * common tail: clrbit 15 of 0x500068, REFUSE the sub-call to
 * 0x1fcc0 (display_profile_apply, not yet recovered), clrbit
 * 21 of 0x500068, ret. The clrbit 21 is NOT applied; the
 * dispatcher's interpreted fallback runs the call and the
 * clrbit 21 from 0xcfa4 onward. */
vf2_status vf2_hybrid_player_cf04_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x1fee4: trivial init (36 B, 5 blocks).
 * IP must be 0x1fee4 with a pushed frame. Writes the IEEE 754
 * float 1.0 (0x3f800000) to 26 consecutive 4-byte locations
 * starting at 0x50a0e0 (covering 0x50a0e0..0x50a144). This is
 * the initializer called by 0x1ff0c. */
vf2_status vf2_hybrid_player_1fee4_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x1ff0c: display_profile_mode_constants (240 B, 5
 * blocks, 3 paths). IP must be 0x1ff0c with a pushed frame.
 * REFUSES the sub-call to 0x1fee4 (the dispatcher's interpreted
 * fallback will run the 26-iter init); then dispatches on
 * 0x500064: == 10 -> write 2 floats; == 6 -> write 10 floats;
 * default -> ret. */
vf2_status vf2_hybrid_player_1ff0c_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x1fffc: display_color_profile_apply (88 B, 3
 * blocks, 2 paths). IP must be 0x1fffc with a pushed frame.
 * Reads 0x500064 and 0x500068; if bit 21 of 0x500068 clear,
 * uses the 0x500064 value; else uses 3. Shifts left by 8 to
 * get an index, reads 3 bytes from a ROM-resident table at
 * 0x6eeb8 + index, and stores them to 0x5000e0/0x5000e1/
 * 0x5000e2. REFUSES the sub-call to 0x2c38 (color_table_rebuild,
 * not yet recovered). */
vf2_status vf2_hybrid_player_1fffc_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x4b410: video_command_submit (60 B, 1 block).
 * IP must be 0x4b410 with a pushed frame. Writes 1 to
 * 0x550000 (a control word), then writes 3 to 0x5502e0 (a
 * status), and 4 words (g0, g1, g2) to 0x5502e0 + 0..0xc. */
vf2_status vf2_hybrid_player_4b410_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x11704: video_table_expand_128 (64 B, 5 blocks).
 * IP must be 0x11704 with a pushed frame. Reads a 32-bit outer
 * count from 0x78d0c, then for each of those iterations
 * copies 128 bytes from 0x78d10 to 0x12800000. The copy is
 * byte-by-byte (ldob/st) with 4-byte pointer increments. */
vf2_status vf2_hybrid_player_11704_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x2eab8: display_runtime_initialize (364 B, 1 block).
 * IP must be 0x2eab8 with a pushed frame. Performs a long
 * struct init at *0x500814 (offset 0x234..0x2cc) plus 3
 * work-RAM stores at 0x50a160..0x168 and a zero byte at
 * 0x50a14d. The sub-call to 0x31004 is fully inlined (writes
 * to *0x50084c+0x40, +0x54..+0x5c, +0x60..+0x68, +0x70). */
vf2_status vf2_hybrid_player_2eab8_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x2c38: color_table_rebuild (432 B, 11 blocks).
 * IP must be 0x2c38 with a pushed frame. Zeros the color
 * table at 0x546008..0x54612d, then runs a 27x47 nested loop
 * that fills 0x54612e..0x?? with computed color values.
 * The "subo 1, 0, g1" at 0x2d40 is the disasm-vs-executor
 * ambiguity flagged in v0755; the recovery matches the
 * executor's interpretation (g1 = 0xFFFFFFFF when g1 >= 256).
 * See color_table_rebuild_executor_v0755f.md for the design
 * rationale. Stand-alone execute hook, not yet wired. */
vf2_status vf2_hybrid_player_2c38_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x1fcc0: display_profile_apply (548 B, 15 blocks,
 * 6 sub-calls). IP must be 0x1fcc0 with a pushed frame.
 * Decides a mode byte (0x500064) from a 2+1 / 1+2 fighter-type
 * combo plus the pre-existing 0x500064/0x50004c/0x500068
 * state, and writes 0x500064/0x500068/0x50a000/0x50a004
 * accordingly. Then runs the 5 inlined sub-callees:
 *   0x1ff0c (mode constants, which itself inlines 0x1fee4),
 *   0x1fffc (color profile apply; refuses 0x2c38),
 *   0x4b410 (video command submit),
 *   0x2eab8 (display runtime initialize),
 *   0x11704 (video table expand_128).
 * The refused 0x2c38 sub-call comes from inside the inlined
 * 0x1fffc, so this function returns VF2_ERROR_UNSUPPORTED with
 * cpu->ip == 0x20050 (the 0x1fffc sub-call's post-return slot)
 * and the per-step loop's fallback will run the call+ret. */
vf2_status vf2_hybrid_player_1fcc0_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x323fc: post_cf04_combat_state_clear (164 B, 7
 * blocks). IP must be 0x323fc with a pushed frame. Calls
 * 0xcf04 first (which has its own hybrid recovery v0747),
 * then clears bit 19 of 0x500068, stores 0 to (g13 + 0x47),
 * clears bit 8 of *(g13), writes 0x53f to 0x500024, writes
 * 100 to (g13 + 0x40), then dispatches on bit 0 of *(g13):
 *   - bit 0 set: read 0x500056, invert (toggle 0<->1), set
 *     bits 1 and 3 of *(g13), store 0x324a0 to (g13 + 0xc),
 *     ret to 0x324a0.
 *   - bit 0 clear (and bit 3 clear / bit 3 set): no-op, ret
 *     to 0x3244c.
 * g13 (cpu->registers[16+13]=29) points to a work-RAM struct
 * set up by the caller. Stand-alone execute hook, not yet
 * wired into the per-step hook table. */
vf2_status vf2_hybrid_player_323fc_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x32284: post_cf04_combat_state_clear_v18 sibling of
 * 0x323fc (v0756). Same shape as 0x323fc but clrbits bit 18 of
 * 0x500068 instead of bit 19, then jumps to the shared body at
 * 0x3240c. Stand-alone execute hook. */
vf2_status vf2_hybrid_player_32284_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x4421c: post_init_floats_helper (76 B, 1 block).
 * IP must be 0x4421c with a pushed frame. Calls 0x1fee4
 * (already recovered v0749), then writes 4 fixed float
 * constants: 0xbe99999a to 0x50a148, 0x3fb33333 to
 * (*0x500814 + 0x20c), 0x3eb33333 to (*0x500814 + 0x290), the
 * byte 60 to (*0x500814 + 0x2d1), and 0x3f800000 to
 * (*0x500814 + 0x4c). */
vf2_status vf2_hybrid_player_4421c_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x7ef0: rom_to_wram_triple_copy (36 B, 1 block).
 * IP must be 0x7ef0 with a pushed frame. Copies 24 bytes from
 * ROM at 0x7f64 to work RAM at 0x501400 via two ldt/stt pairs.
 * The test must attach a main_rom for the source reads. */
vf2_status vf2_hybrid_player_7ef0_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0xa154: zero_loop_43_dwords (36 B, 3 blocks).
 * IP must be 0xa154 with a pushed frame. Clears 43 dwords
 * (172 bytes) of work RAM starting at 0x501800 using a
 * cmpdeco-driven loop. */
vf2_status vf2_hybrid_player_a154_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x4ad40: zero_workram_helpers (52 B, 1 block).
 * IP must be 0x4ad40 with a pushed frame. Clears 4 work-RAM
 * fields: short 0 to 0x5502a8, 0x5502b0, 0x5502b8 and
 * word 0 to 0x546000. */
vf2_status vf2_hybrid_player_4ad40_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x6dccc: poly_or_model2a_register_switch (88 B, 4 blocks).
 * IP must be 0x6dccc with a pushed frame. Reads the byte at
 * 0x50002b; if it is 16 or 17, the function is a pure ret
 * (no writes). Otherwise reads the 32-bit word at 0x59c318,
 * increments it by 1, and mirrors the new value to 0x1d03318
 * and 0x59c318. The byte from 0x50002b is mirrored to 0x1d0331c
 * and 0x59c31c. The name is provisional (v0734k-characterised
 * gap-run sub-function; second-source naming not yet confirmed). */
vf2_status vf2_hybrid_model2a_register_switch_6dccc_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x65838: kill_osage_evaluate_record (108 B, 7 blocks).
 * IP must be 0x65838 with a pushed frame. Reads g7 as a worker
 * record pointer; if (*(g7 + 0xc) == 0x6428c) and (*(g7) bit 0
 * clear) and (*(g7) bit 2 set) and (*(g7 + 0x128) + g6) >= 0x4268,
 * the function sets bit 3 of *(g7) and increments the global
 * counter at 0x500164. Otherwise it clears bit 3 of *(g7) and
 * accumulates *(g7 + 0x128) into g6. Two callers per vf2i960
 * xrefs (0x65828 and 0x65830). */
vf2_status vf2_hybrid_kill_osage_evaluate_record_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover task_object 0x6ca64 + cluster (v0767). The task_object
 * dispatcher (20 B) reads the instance-index byte at g13+0x4 and
 * looks up a 4-byte jump table at 0x6ca78; the looked-up entry
 * becomes the new g13+0xc (next-entry pointer). Three init stubs
 * follow (init0 0x6cae0 sets next to 0x6caf0; init1 0x6caf4 sets
 * next to 0x6cb04; init2 0x6cb08 is itself a bare ret). The two
 * "init ret" slots 0x6caf0 and 0x6cb04 are bare rets. All 5
 * entries are fall-through only (no call refs); they exercise as
 * stand-alone execute hooks. */
vf2_status vf2_hybrid_task_object_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);
vf2_status vf2_hybrid_task_object_init0_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);
vf2_status vf2_hybrid_task_object_init0_ret_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);
vf2_status vf2_hybrid_task_object_init1_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);
vf2_status vf2_hybrid_task_object_init1_ret_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);
vf2_status vf2_hybrid_task_object_init2_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x6d10c: slot_increment_helper (56 B, 3 blocks).
 * IP must be 0x6d10c with a pushed frame. Computes the slot
 * index r7 = max(20, g0 / 1920); reads the bus pointer at
 * *(0x50016c); increments the 32-bit slot at
 * *(bus_ptr + 0x3520 + r7*4) by 1; mirrors the result to both
 * the Model 2A pair at 0x59c520[r7*4] and the extended bus at
 * 0x1d03520[r7*4]. Shared-epilogue leaf in the v0734k gap run
 * 0x6cb0c..0x6dcb8; fall-through target only. */
vf2_status vf2_hybrid_slot_increment_helper_6d10c_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x6d1b4: sibling_slot_increment_helper (56 B, 3 blocks).
 * Structurally identical to 0x6d10c but with array offset
 * 0x393c (vs 0x3520) and mirror pair 0x1d0393c / 0x59c93c (vs
 * 0x1d03520 / 0x59c520). Computes r7 = max(20, g0 / 1920);
 * increments *(bus_ptr + 0x393c + r7*4) by 1; mirrors to the
 * two addresses. Sibling leaf in the same v0734k gap run. */
vf2_status vf2_hybrid_sibling_slot_increment_helper_6d1b4_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x9480: poly_or_crc16_step_hash (80 B, 3 blocks).
 * IP must be 0x9480 with a pushed frame. Reads byte string
 * from g0 (with per-iteration stride 1+g1), runs over
 * g2 iterations, and folds a 256-entry table at VF2_MAIN_DATA_BASE
 * (0x02000000) into a 16-bit accumulator. The result is
 * returned in g0. Four callers per vf2i960 xrefs: 0x6de3c,
 * 0x6de6c, 0x6de90, 0x6dec0. The accumulator is split between
 * r5 (low byte in middle-byte position via r3<<8&0xff00) and
 * r3 (high byte in low-byte position via r3>>8) each iteration,
 * XOR'd with the next input byte and looked up via the table. */
vf2_status vf2_hybrid_crc16_step_hash_9480_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x5ff7c: thin wrapper around the CRC16 step hash
 * (0x9480). 7 instructions, 1 block, 40 B. Loads a 32-bit base
 * pointer from 0x50016c (work_ram), sets up g0 = base+0x33a8
 * (1603-byte input buffer), g1 = 0 (stride), g2 = 0x644
 * (1603 iters), calls 0x9480, then stos the 32-bit result
 * to 0x01d03304 (backup_sram) and returns.
 *
 * 1-to-1 C translation: emits the call to the recovered
 * 0x9480 directly via vf2_hybrid_crc16_step_hash_9480_execute
 * after temporarily setting cpu->ip = 0x9480. Refused if the
 * sub-recovery returns non-OK. */
vf2_status vf2_hybrid_block_hash_apply_5ff7c_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x6d0a0: 27-instruction tick function calling the
 * v0769 queue dedup-append (0x6d1b4) and the v0772 block_hash
 * wrapper (0x5ff7c). 1-to-1 C translation. Reads base ptr
 * from 0x50016c, increments the counter at base+0x33fc (and
 * mirrors it to 0x01d033fc + 0x0059c3fc), masks the bus
 * pointer's low 16 bits and stores to 0x5000e4, then calls
 * 0x5ff7c. The original i960 body uses sp-relative scratch
 * (st r15,sp-4; ld sp-4,r15) to preserve r15 across the
 * 0x6d1b4 sub-call; the recovery reads/writes work_ram at
 * sp-4 via vf2_model2a_read/write_u32. The test must set
 * cpu->registers[3] (sp) to a work_ram address. */
vf2_status vf2_hybrid_frame_end_tick_6d0a0_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x60330: tiny (220 B, 1 block) record-clear.
 * 1-to-1 C translation. Writes 0 to backup_sram 0x01d03320..0x01d03328
 * (4 bytes as u32 + 5 bytes), writes 2 to 0x01d03329..0x01d0332e
 * (6 bytes), and mirrors every write to extended work_ram
 * 0x59c320..0x59c32e. No sub-calls. Single caller is 0x6dea0
 * inside the 0x6de3c function (one of the 0x9480 callers in
 * the v0734k 0x4d2c0..0x4e808 gap run). */
vf2_status vf2_hybrid_poly_record_clear_60330_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x94d0: tiny (88 B, 1 block) 4-timer fuse.
 * 1-to-1 C translation. Reads the running "fuse" state at
 * 0x500098, adds the 4 Model 2A timer values (at 0xF00000,
 * 0xF00004, 0xF00008, 0xF0000C) shifted by 4, 8, 12, 16,
 * stores the accumulated value back to 0x500098, and
 * returns (state >> 4) & 0xffff as the new "next" value
 * (callers use the low 5 bits as a bit position in a
 * 32-bit bitmap at 0x1b780 / 0x5427c). No sub-calls. Two
 * callers: 0x1b780 and 0x542c8 (in the slot-allocator loop
 * of 0x5427c). */
vf2_status vf2_hybrid_timer_fuse_94d0_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x5427c: 156 B / 8 blocks polygon record loader.
 * 1-to-1 C translation. Zero-fills 256 bytes of dst at
 * 0x01d03100 (a 16-byte x 16-slot sprite descriptor
 * table), then runs an 18-iteration loop (r10 = 0..17)
 * that:
 *   - for r10 < 6: calls 0x94d0 (timer_fuse) via
 *     vf2_hybrid_timer_fuse_94d0_execute to allocate a
 *     free 5-bit slot in a 32-bit bitmap;
 *   - for r10 >= 6: uses r10 directly as the slot index;
 *   - writes 9 bytes per iter to dst[16*r10]:
 *       offset 0:  4 bytes from src+12*r10 (src stride 12)
 *       offset 4:  4 bytes from *(polygon_table[slot])
 *       offset 10: 1 byte from src+12*r10+6
 * src base is 0x0201ea24, polygon pointer table is at
 * 0x0201eafc (32 entries of 4 bytes each).
 *
 * Two callers (per vf2i960 xrefs): 0x6de50 and 0x6dea4,
 * both inside 0x6de3c (one of the v0771 / 0x9480 callers).
 * Refuses (returns VF2_ERROR_UNSUPPORTED) if the v0775
 * sub-recovery returns non-OK. */
vf2_status vf2_hybrid_poly_record_load_5427c_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x1b780: 20 B / 1 block 3-bit slot table lookup.
 * 1-to-1 C translation. Three comparison gates decide between
 * a lookup path (call 0x94d0 timer_fuse, mask to 3 bits,
 * read 16-bit word from table at 0x0201cb8c + g0*2) and a
 * default path returning 0x282. The gates are:
 *   - cmpobe r3, r7 (default if r3 > r7)
 *   - cmpobne r3, r4 (lookup if r3 != r4)
 *   - cmpobl r5, r6 (default if r5 < r6)
 * All three must hold for the lookup path. Calls v0775
 * (0x94d0 timer_fuse) for the lookup; refuses
 * (returns VF2_ERROR_UNSUPPORTED) if the sub-recovery returns
 * non-OK. */
vf2_status vf2_hybrid_slot3_table_lookup_1b780_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x5a460: 176 B / 1 block bus-table snapshot.
 * 1-to-1 C translation. Reads 7 bytes from bus-relative
 * offsets 0x3356, 0x3359, 0x3357, 0x335a, 0x3358, 0x335b,
 * 0x335c (a "shuffled" snapshot of 7 consecutive bytes at
 * 0x3356..0x335c) where bus_ptr = *(0x50016c). Stores them
 * to work_ram 0x500234..0x50023a. Then calls 0x2b4c
 * (color_table_rebuild, deferred per v0751 - returns
 * UNSUPPORTED for the sub-call so the dispatcher falls
 * back to interpretation). */
vf2_status vf2_hybrid_color_table_snapshot_5a460_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Per-step hook instrumentation (v0755c).
 *
 * Read-only access to the per-hook fire counters maintained by
 * the per-step loop in hybrid_execute_interpreted_until. Each
 * counter records how many times the corresponding callee hook
 * was called. Ctest entries use this to verify the hook actually
 * fires (vs. the interpretation fallback being used).
 *
 * out_counts must point to an array of at least 4 uint64_t
 * values. The order is: 0=0x29598, 1=0x439ac, 2=0x43888,
 * 3=0xcf04. out_total (may be NULL) receives the sum of all
 * counters.
 */
void vf2_hybrid_get_callee_hook_counts(
    uint64_t *out_counts,
    uint64_t *out_total);

/* Reset all per-hook fire counters to zero. Use at the start of
 * a test to scope counter measurements. */
void vf2_hybrid_reset_callee_hook_counts(void);

/* Public wrapper (v0755c) for the per-step loop in
 * hybrid_execute_interpreted_until. Same semantics as the
 * internal function: cpu->ip must equal entry_address with a
 * pushed frame, and the loop runs until cpu->ip == stop_address
 * or a registered hook fires (and either returns VF2_OK to
 * continue or VF2_ERROR_UNSUPPORTED for clean refusal). The
 * per-step loop is used only for ranges that have at least one
 * registered callee hook; other ranges use vf2_i960_run.
 */
vf2_status vf2_hybrid_run_interpreted_until(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t entry_address,
    uint32_t stop_address);

#endif
