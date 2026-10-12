#ifndef VF2_M68K_STEP_H
#define VF2_M68K_STEP_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/scsp.h"
#include "vf2/status.h"

/* Minimal 68000 stepper for the recovered sound-corridor subset.
 *
 * This is a measurement oracle, not a CPU model: it executes only the
 * opcodes on already-recovered paths and fails closed
 * (VF2_ERROR_UNSUPPORTED) on everything else. Instruction admission
 * and length come from vf2_m68k_decode, so stepper and disassembler
 * agree by construction; a length mismatch trips UNSUPPORTED.
 *
 * Address map (mirrors sound_board.c's access-kind split):
 *   fetch and PC-relative data  -> rom (code lives at low addresses)
 *   0x600000 + rom_size         -> rom mirror (tables)
 *   0x800000 + sample size      -> sample_rom reads (writes refuse)
 *   0x100000 + 0x1000           -> scsp model (with its semantics)
 *   below ram_size              -> ram
 *   $400000/$400001             -> event log (mapped port, unmodeled
 *                                  behavior; reads give 0, writes are
 *                                  recorded) and execution continues
 *   anywhere else               -> VF2_ERROR_UNSUPPORTED (silicon
 *                                  traps unmapped accesses: fail closed).
 * Triangle tests assert the event log they expect (usually empty).
 */

enum {
    VF2_M68K_STEP_MAX_EVENTS = 64u
};

typedef struct vf2_m68k_regs {
    uint32_t d[8];
    uint32_t a[8];
    uint32_t pc;
    uint8_t x;
    uint8_t n;
    uint8_t z;
    uint8_t v;
    uint8_t c;
} vf2_m68k_regs;

typedef struct vf2_m68k_bus_event {
    uint32_t address;
    uint32_t value;
    uint8_t size;
    uint8_t is_write;
} vf2_m68k_bus_event;

typedef struct vf2_m68k_bus {
    uint8_t *ram;
    size_t ram_size;
    const uint8_t *rom;
    size_t rom_size;
    const uint8_t *sample_rom;
    size_t sample_rom_size;
    vf2_scsp *scsp;
    vf2_m68k_bus_event events[VF2_M68K_STEP_MAX_EVENTS];
    size_t event_count;
} vf2_m68k_bus;

void vf2_m68k_regs_reset(vf2_m68k_regs *regs);

/* Run until pc == stop_pc (checked before fetch), a return with
 * stop_on_return set (rts/rte halt OK), step exhaustion, or an
 * unsupported/out-of-bounds condition. steps_done counts the
 * instructions actually executed. */
vf2_status vf2_m68k_run(
    vf2_m68k_bus *bus,
    vf2_m68k_regs *regs,
    uint32_t stop_pc,
    int stop_on_return,
    uint32_t max_steps,
    uint32_t *steps_done
);

#endif
