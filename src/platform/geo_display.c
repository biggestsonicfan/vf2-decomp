#include "vf2/geo_display.h"

#include <string.h>

/* RAM9 display-list buffer is 0x20000 bytes (0x8000 u32 words); jump targets and
 * read/write pointers are masked to it, exactly as the hardware addresses it. */
#define VF2_GEO_BUF_BYTE_MASK 0x1ffffu
#define VF2_GEO_OPCODE_LIMIT  0x40000u /* loop guard against a malformed list */

static float geo_u2f(uint32_t u)
{
    float f;
    memcpy(&f, &u, sizeof(f));
    return f;
}

vf2_status vf2_geo_walk(const uint32_t *buffer, size_t words,
                        uint32_t read_start,
                        vf2_geo_object *out, size_t max_objects,
                        size_t *out_count, vf2_geo_stop *stop,
                        size_t *opcodes)
{
    size_t ptr;
    size_t count = 0u;
    size_t opc = 0u;
    float mat[12];
    vf2_geo_stop why = VF2_GEO_STOP_END;

    if (buffer == NULL || out == NULL || words == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    memset(mat, 0, sizeof(mat));
    ptr = (size_t)((read_start & VF2_GEO_BUF_BYTE_MASK) >> 2);

    for (;;) {
        uint32_t op;
        uint32_t cmd;
        size_t need = 0u; /* operand words consumed after the opcode word */

        if (opc >= VF2_GEO_OPCODE_LIMIT) {
            why = VF2_GEO_STOP_OPCODE_LIMIT;
            break;
        }
        if (ptr >= words) {
            why = VF2_GEO_STOP_BOUNDS;
            break;
        }
        op = buffer[ptr++];
        ++opc;

        if (op & 0x80000000u) { /* jump: continue at the target word */
            ptr = (size_t)((op & VF2_GEO_BUF_BYTE_MASK) >> 2);
            continue;
        }

        cmd = (op >> 23) & 0x1fu;

        switch (cmd) {
        case 0x00u:                     /* nop */
            need = 0u;
            break;
        case 0x07u: case 0x17u:         /* mode */
        case 0x08u: case 0x18u:         /* zsort mode */
        case 0x10u:                     /* dummy */
        case 0x16u:                     /* lod */
        case 0x1eu:                     /* code jump */
            need = 1u;
            break;
        case 0x09u: case 0x19u:         /* focal distance */
        case 0x0du:                     /* data memory push (consumes addr,count) */
            need = 2u;
            break;
        case 0x0au: case 0x1au:         /* light source vector */
        case 0x0cu: case 0x1cu:         /* parallel translate (3 floats) */
            need = 3u;
            break;
        case 0x03u: case 0x13u:         /* window data */
            need = 6u;
            break;
        case 0x0bu: case 0x1bu:         /* matrix write (12 floats) */
            need = 12u;
            break;
        case 0x01u: case 0x11u:         /* object data */
            need = 4u;
            break;
        case 0x04u: case 0x14u:         /* texture data / log data: 2 + count */
        case 0x05u: case 0x15u:         /* polygon data: 2 + count */
            if (ptr + 2u > words) {
                why = VF2_GEO_STOP_BOUNDS;
                goto done;
            }
            need = 2u + (size_t)buffer[ptr + 1u];
            break;
        case 0x06u:                     /* texture parameters: 2 + count*2 */
            if (ptr + 2u > words) {
                why = VF2_GEO_STOP_BOUNDS;
                goto done;
            }
            need = 2u + (size_t)buffer[ptr + 1u] * 2u;
            break;
        case 0x1du:                     /* code upload: 2 + count*3 */
            if (ptr + 2u > words) {
                why = VF2_GEO_STOP_BOUNDS;
                goto done;
            }
            need = 2u + (size_t)buffer[ptr + 1u] * 3u;
            break;
        case 0x02u: case 0x12u: {       /* direct data: 8 + variable attr loop */
            size_t p = ptr + 8u;        /* tpa,tha + two xyz points */
            for (;;) {
                uint32_t attr;
                if (p >= words) {
                    why = VF2_GEO_STOP_BOUNDS;
                    goto done;
                }
                attr = buffer[p++];
                if ((attr & 3u) == 0u) {
                    break;              /* terminating attr, consumed */
                }
                p += 5u;                /* luma, distance, one xyz point */
                if (attr & 1u) {
                    p += 3u;            /* quad: a second xyz point */
                }
            }
            need = p - ptr;
            break;
        }
        case 0x0eu: {                   /* geo test: 32 + 1 + blocks*3 */
            if (ptr + 33u > words) {
                why = VF2_GEO_STOP_BOUNDS;
                goto done;
            }
            need = 32u + 1u + (size_t)buffer[ptr + 32u] * 3u;
            break;
        }
        case 0x0fu: case 0x1fu:         /* end */
            why = VF2_GEO_STOP_END;
            goto done;
        default:
            /* All 32 five-bit opcodes are enumerated above; unreachable. */
            why = VF2_GEO_STOP_END;
            goto done;
        }

        if (ptr + need > words) {
            why = VF2_GEO_STOP_BOUNDS;
            break;
        }

        if (cmd == 0x0bu || cmd == 0x1bu) {
            size_t i;
            for (i = 0u; i < 12u; ++i) {
                mat[i] = geo_u2f(buffer[ptr + i]);
            }
        } else if (cmd == 0x0cu || cmd == 0x1cu) {
            mat[9] = geo_u2f(buffer[ptr + 0u]);
            mat[10] = geo_u2f(buffer[ptr + 1u]);
            mat[11] = geo_u2f(buffer[ptr + 2u]);
        } else if (cmd == 0x01u || cmd == 0x11u) {
            if (count >= max_objects) {
                why = VF2_GEO_STOP_FULL;
                break;
            }
            out[count].tpa = buffer[ptr + 0u];
            out[count].tha = buffer[ptr + 1u];
            out[count].oba = buffer[ptr + 2u];
            out[count].obc = buffer[ptr + 3u];
            memcpy(out[count].matrix, mat, sizeof(mat));
            ++count;
        }

        ptr += need;
    }

done:
    if (out_count != NULL) {
        *out_count = count;
    }
    if (stop != NULL) {
        *stop = why;
    }
    if (opcodes != NULL) {
        *opcodes = opc;
    }
    return VF2_OK;
}

/* --- buffer builder ---------------------------------------------------------*/

void vf2_geo_builder_init(vf2_geo_builder *b, uint32_t *buffer, size_t words)
{
    if (b == NULL) {
        return;
    }
    b->buffer = buffer;
    b->words = words;
    b->write_ptr = 0u;
    b->read_ptr = 0u;
    b->geoctl = 0u;
}

static void geo_push(vf2_geo_builder *b, uint32_t word)
{
    size_t index = (size_t)((b->write_ptr & VF2_GEO_BUF_BYTE_MASK) >> 2);
    if (b->buffer != NULL && index < b->words) {
        b->buffer[index] = word;
    }
    b->write_ptr += 4u;
}

void vf2_geo_builder_geo_write(vf2_geo_builder *b, uint32_t port_offset,
                               uint32_t value)
{
    uint32_t offset;
    if (b == NULL) {
        return;
    }
    offset = port_offset & 0x3fffu;
    if (offset < 0x1000u) {
        uint32_t func = (offset >> 4) & 0x3fu;
        uint32_t word;
        if (value & 0x80000000u) {
            word = (value & 0x800fffffu) | (func << 23);
        } else if ((offset & 0xfu) == 0u) {
            word = (value & 0x000fffffu) | (func << 23);
            if (((offset >> 4) & 0xc0u) != 0u && func == 1u) {
                word |= ((offset >> 10) & 3u) << 29; /* eye mode */
            }
        } else {
            return; /* non-word-aligned function write: ignored by hardware */
        }
        geo_push(b, word);
        return;
    }
    if (offset == 0x1008u) {
        b->write_ptr = value & 0xfffffu;
    } else if (offset == 0x3008u) {
        b->read_ptr = value & 0xfffffu;
    }
}

void vf2_geo_builder_prg_write(vf2_geo_builder *b, uint32_t value)
{
    if (b == NULL) {
        return;
    }
    /* The prg gate: in microcode-upload mode the words are coprocessor code,
     * not display-list data, and must not be pushed to the buffer. */
    if (b->geoctl & 0x80000000u) {
        return;
    }
    geo_push(b, value);
}

void vf2_geo_builder_set_geoctl(vf2_geo_builder *b, uint32_t value)
{
    if (b != NULL) {
        b->geoctl = value;
    }
}
