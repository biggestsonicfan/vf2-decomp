#ifndef VF2_GEO_DISPLAY_H
#define VF2_GEO_DISPLAY_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/status.h"

/* Geometriser display-list traversal (Step 1 of the geometriser pipeline, see
 * decomp/i960/notes/geo_pipeline_step0_sm2emu_review_v0836.md).
 *
 * The Sega Model 2 geometriser reads a display list from its RAM9 buffer
 * (0x900000-0x91ffff, 0x20000 bytes = 0x8000 u32 words) once per frame, starting
 * at a read-start byte offset the game sets. Each word is either a JUMP (bit 31
 * set -> continue at byte (word & 0x1ffff)) or a command whose five-bit opcode
 * is (word >> 23) & 0x1f. This walker follows jumps and sizes every command
 * EXACTLY as the hardware consumes it (reimplemented clean-room from the opcode
 * table in third_party/mame-model2-ref/model2_v.cpp geo_process_command), so
 * the read pointer always lands on the next command. It records each object
 * submission (opcode 0x01/0x11) together with the transform matrix current at
 * that point (set by matrix-write 0x0b/0x1b and translate 0x0c/0x1c).
 *
 * NOTE: this reads a COMPLETE buffer (e.g. a RAM9 end-of-frame dump that already
 * contains what the geometry coprocessor wrote); it does not reconstruct the
 * buffer from i960 writes. The separate vf2_geo_builder below rebuilds a buffer
 * from geometriser control/data writes when only writes are available, honouring
 * the prg-port microcode-upload gate. */

/* Exact number of u32 operand words a command consumes AFTER its opcode word,
 * for the fixed- and count-sized commands. Variable commands (direct-data 0x02/
 * 0x12, test 0x0e) are sized by the walker itself. End (0x0f/0x1f) stops. */

typedef struct vf2_geo_object {
    uint32_t tpa;        /* w0: texture-point (UV) table address */
    uint32_t tha;        /* w1: texture-header table address */
    uint32_t oba;        /* w2: polygon-record address */
    uint32_t obc;        /* w3: primitive count */
    float    matrix[12]; /* transform current at submission (raw 12-float order
                          * of geo opcode 0x0b: 3x3 by columns, then translate) */
} vf2_geo_object;

typedef enum vf2_geo_stop {
    VF2_GEO_STOP_END = 0,       /* reached an end opcode (0x0f/0x1f) */
    VF2_GEO_STOP_FULL,          /* out[] filled to max_objects */
    VF2_GEO_STOP_BOUNDS,        /* read/jump went outside the buffer */
    VF2_GEO_STOP_OPCODE_LIMIT   /* opcode guard tripped (runaway list) */
} vf2_geo_stop;

/* Walk `buffer` (`words` u32s) from byte offset `read_start`, following jumps
 * and sizing commands exactly, recording object submissions (with their current
 * matrix) into out[] up to max_objects. On success *out_count = objects
 * recorded, *stop = why the walk ended, *opcodes = commands executed (both
 * out params optional / may be NULL). Returns VF2_ERROR_INVALID_ARGUMENT for a
 * null buffer/out or zero words, else VF2_OK. A malformed list never reads out
 * of bounds: it stops with VF2_GEO_STOP_BOUNDS / _OPCODE_LIMIT. */
vf2_status vf2_geo_walk(const uint32_t *buffer, size_t words,
                        uint32_t read_start,
                        vf2_geo_object *out, size_t max_objects,
                        size_t *out_count, vf2_geo_stop *stop,
                        size_t *opcodes);

/* --- buffer builder: rebuild a display list from geometriser writes ----------
 *
 * When a capture carries geometriser writes rather than a full RAM9 dump, replay
 * them through this to rebuild the buffer with the hardware's semantics:
 *   - geo-function writes (port offset < 0x1000): the function number (bits of
 *     the address) is folded into the pushed word, and the write advances the
 *     write pointer (push_geo_data);
 *   - port offset 0x1008 sets the write pointer, 0x3008 the read pointer;
 *   - prg-port writes (0x804000-0x807fff) push to the buffer ONLY when not in
 *     microcode-upload mode -- the prg gate: geoctl bit 31 set => the words are
 *     microcode for the coprocessor, not display-list data, and are skipped. */

typedef struct vf2_geo_builder {
    uint32_t *buffer;
    size_t    words;
    uint32_t  write_ptr; /* byte offset of the next push_geo_data */
    uint32_t  read_ptr;  /* byte offset the geometriser will read from */
    uint32_t  geoctl;    /* last geoctl value; bit 31 = microcode-upload mode */
} vf2_geo_builder;

void vf2_geo_builder_init(vf2_geo_builder *b, uint32_t *buffer, size_t words);

/* A write to the geo function/control port. `port_offset` is the address minus
 * the geo port base (0x00800000), i.e. 0x0000-0x3fff. */
void vf2_geo_builder_geo_write(vf2_geo_builder *b, uint32_t port_offset,
                               uint32_t value);

/* A write to the prg port (0x00804000-0x00807fff). Pushed to the buffer unless
 * the prg gate (geoctl bit 31) is set. */
void vf2_geo_builder_prg_write(vf2_geo_builder *b, uint32_t value);

/* Set the geoctl register (bit 31 opens/closes the prg gate). */
void vf2_geo_builder_set_geoctl(vf2_geo_builder *b, uint32_t value);

#endif
