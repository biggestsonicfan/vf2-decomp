#ifndef VF2_MODEL_DECODE_H
#define VF2_MODEL_DECODE_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/status.h"

/* Polygon-ROM model-record decoder (3D-model extraction, slice 4).
 *
 * A model record is a little-endian IEEE-754 float stream, recovered reference-
 * free in decomp/i960/notes/polygon_rom_model_format_v0826.md and
 * fight_character_assembly_v0828.md. The face-assembly (strip/quad/skip-3) follows
 * the in-tree boundary model in src/hardware/tgp.c (geometry_execute_object); the
 * vertex coordinates are MEASURED, the triangulation is the tgp.c HYPOTHESIS decode
 * and is flagged as such. The record is located from the object table at
 * 0x020e0004: entry w0 (the value the i960 writes to geo 0x00800010) selects an
 * entry whose w2 is the record pointer, byte_offset = (w2 & 0x7fffff) * 4. */

typedef struct vf2_model_vertex {
    float x;
    float y;
    float z;
} vf2_model_vertex;

typedef struct vf2_model_triangle {
    vf2_model_vertex v[3];
} vf2_model_triangle;

/* Decode one model record into a triangle list.
 *
 * poly/poly_size: the polygon region bytes (VF2_REGION_POLYGONS). byte_offset: the
 * record start (a p0 vertex). skip3 (nonzero): each attribute is followed by a
 * 3-word normal slot (the observed character/attract record format; geometry_mode
 * & 3 < 2 in tgp.c). out/max_tris: caller buffer; *out_count receives the number
 * written. The walk stops at an attribute with (attr & 3) == 0 (end marker) or
 * when the record would read past poly_size / fill out. Returns VF2_OK, or
 * VF2_ERROR_INVALID_ARGUMENT / VF2_ERROR_OUT_OF_BOUNDS on bad arguments. A record
 * that fills out is not an error (out_count == max_tris; raise max_tris to get the
 * rest). */
vf2_status vf2_model_decode_record(
    const uint8_t *poly,
    size_t poly_size,
    uint32_t byte_offset,
    int skip3,
    vf2_model_triangle *out,
    size_t max_tris,
    size_t *out_count
);

/* Resolve a geo-0x00800010 value (object-table w0) to the record's polygon-ROM
 * byte offset, via the object table at 0x020e0004 in the main_data region. On a
 * match, *byte_offset = (w2 & 0x7fffff) * 4 and *bank = (w2 >> 23) & 1. Returns
 * VF2_OK on a match, VF2_ERROR_UNSUPPORTED if no entry has that w0, or an argument
 * error. Scans object ids 0..max_id. */
vf2_status vf2_model_lookup_w0(
    const uint8_t *main_data,
    size_t main_data_size,
    uint32_t w0,
    uint32_t max_id,
    uint32_t *byte_offset,
    uint32_t *bank,
    uint32_t *object_id
);

#endif
