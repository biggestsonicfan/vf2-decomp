#include "vf2/model_decode.h"

#include <string.h>

/* See include/vf2/model_decode.h. The decode mirrors src/hardware/tgp.c
 * geometry_execute_object: read p0 (3 floats) and p1 (3 floats), then for each
 * link read an attribute word (end on (attr & 3) == 0), optionally skip a 3-word
 * normal slot, read p2 (3 floats) and, when (attr & 1), p3 (3 floats); emit one
 * triangle (p0,p1,p2) plus, for a quad, (p0,p2,p3); advance the strip p0/p1 by
 * (attr >> 8) & 3. Reference-free: validated against the pol_test record 0x97d. */

static float model_float(uint32_t raw)
{
    float value = 0.0f;
    memcpy(&value, &raw, sizeof(value));
    return value;
}

/* Model-space coordinates are bounded (characters are tens of units). A vertex
 * outside this range is a sign the strip has derailed (an attribute/descriptor
 * word read where a coordinate was expected), so the record is ended fail-closed. */
static int model_vertex_sane(const vf2_model_vertex *v)
{
    const float limit = 1.0e4f;
    float a = v->x < 0.0f ? -v->x : v->x;
    float b = v->y < 0.0f ? -v->y : v->y;
    float c = v->z < 0.0f ? -v->z : v->z;
    /* reject NaN/Inf: a self-comparison fails for NaN, and Inf exceeds limit */
    if (a != a || b != b || c != c) {
        return 0;
    }
    return a < limit && b < limit && c < limit;
}

static int model_read_u32(
    const uint8_t *poly, size_t poly_size, uint32_t word_index, uint32_t *out
)
{
    const size_t byte = (size_t)word_index * 4u;
    if (byte + 4u > poly_size) {
        return 0;
    }
    *out = (uint32_t)poly[byte] | ((uint32_t)poly[byte + 1u] << 8) |
           ((uint32_t)poly[byte + 2u] << 16) | ((uint32_t)poly[byte + 3u] << 24);
    return 1;
}

static int model_read_vertex(
    const uint8_t *poly, size_t poly_size, uint32_t base_word, uint32_t *cursor,
    vf2_model_vertex *v
)
{
    uint32_t wx = 0u;
    uint32_t wy = 0u;
    uint32_t wz = 0u;
    if (!model_read_u32(poly, poly_size, base_word + *cursor, &wx) ||
        !model_read_u32(poly, poly_size, base_word + *cursor + 1u, &wy) ||
        !model_read_u32(poly, poly_size, base_word + *cursor + 2u, &wz)) {
        return 0;
    }
    v->x = model_float(wx);
    v->y = model_float(wy);
    v->z = model_float(wz);
    *cursor += 3u;
    return 1;
}

vf2_status vf2_model_decode_record(
    const uint8_t *poly,
    size_t poly_size,
    uint32_t byte_offset,
    int skip3,
    vf2_model_triangle *out,
    size_t max_tris,
    size_t *out_count
)
{
    const uint32_t base_word = byte_offset / 4u;
    uint32_t cursor = 0u;
    uint32_t link = 0u;
    const uint32_t link_limit = 0x100000u; /* fail-closed bound */
    vf2_model_vertex p0;
    vf2_model_vertex p1;
    size_t count = 0u;

    if (poly == NULL || out == NULL || out_count == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *out_count = 0u;
    if ((byte_offset & 3u) != 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (!model_read_vertex(poly, poly_size, base_word, &cursor, &p0) ||
        !model_read_vertex(poly, poly_size, base_word, &cursor, &p1)) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    if (!model_vertex_sane(&p0) || !model_vertex_sane(&p1)) {
        return VF2_OK; /* offset is not a plausible record start; empty mesh */
    }

    for (link = 0u; link < link_limit && count < max_tris; ++link) {
        uint32_t attribute = 0u;
        vf2_model_vertex p2;
        vf2_model_vertex p3;
        int quad = 0;

        if (!model_read_u32(poly, poly_size, base_word + cursor, &attribute)) {
            break;
        }
        ++cursor;
        /* Record terminators: a zero/low-bits-clear attribute (tgp.c's
         * (attr & 3) == 0 break) and the measured 0x00040401 end marker that
         * closes a polygon-ROM model record (polygon_rom_model_format_v0826.md).
         * Without the latter the strip would run past the record into the next. */
        if ((attribute & 3u) == 0u || attribute == UINT32_C(0x00040401)) {
            break;
        }
        if (skip3) {
            cursor += 3u; /* normal slot */
        }
        if (!model_read_vertex(poly, poly_size, base_word, &cursor, &p2)) {
            break;
        }
        if (!model_vertex_sane(&p2)) {
            break; /* derailed strip */
        }
        if ((attribute & 1u) != 0u) {
            quad = 1;
            if (!model_read_vertex(poly, poly_size, base_word, &cursor, &p3)) {
                break;
            }
            if (!model_vertex_sane(&p3)) {
                break;
            }
        }

        out[count].v[0] = p0;
        out[count].v[1] = p1;
        out[count].v[2] = p2;
        ++count;
        if (quad && count < max_tris) {
            out[count].v[0] = p0;
            out[count].v[1] = p2;
            out[count].v[2] = p3;
            ++count;
        }

        switch ((attribute >> 8u) & 3u) {
        case 0u:
        case 2u:
            p0 = p2;
            p1 = quad ? p3 : p2;
            break;
        case 1u:
            p1 = p2;
            break;
        case 3u:
            p0 = quad ? p3 : p2;
            break;
        default:
            break;
        }
    }

    *out_count = count;
    return VF2_OK;
}

vf2_status vf2_model_lookup_w0(
    const uint8_t *main_data,
    size_t main_data_size,
    uint32_t w0,
    uint32_t max_id,
    uint32_t *byte_offset,
    uint32_t *bank,
    uint32_t *object_id
)
{
    /* Object table at 0x020e0004 in the main_data region (base 0x02000000),
     * 16-byte entries {w0, w1, w2, w3}. */
    const uint32_t table_file_offset = 0x000e0004u; /* 0x020e0004 - 0x02000000 */
    uint32_t id = 0u;

    if (main_data == NULL || byte_offset == NULL || bank == NULL ||
        object_id == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    for (id = 0u; id <= max_id; ++id) {
        const size_t off = (size_t)table_file_offset + (size_t)id * 16u;
        uint32_t entry_w0 = 0u;
        uint32_t entry_w2 = 0u;
        if (off + 16u > main_data_size) {
            break;
        }
        entry_w0 = (uint32_t)main_data[off] | ((uint32_t)main_data[off + 1u] << 8) |
                   ((uint32_t)main_data[off + 2u] << 16) |
                   ((uint32_t)main_data[off + 3u] << 24);
        if (entry_w0 != w0) {
            continue;
        }
        entry_w2 = (uint32_t)main_data[off + 8u] |
                   ((uint32_t)main_data[off + 9u] << 8) |
                   ((uint32_t)main_data[off + 10u] << 16) |
                   ((uint32_t)main_data[off + 11u] << 24);
        *byte_offset = (entry_w2 & 0x7fffffu) * 4u;
        *bank = (entry_w2 >> 23u) & 1u;
        *object_id = id;
        return VF2_OK;
    }
    return VF2_ERROR_UNSUPPORTED;
}
