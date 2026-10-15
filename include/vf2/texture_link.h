#ifndef VF2_TEXTURE_LINK_H
#define VF2_TEXTURE_LINK_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/status.h"

/* Polygon -> texture link: the video-board geometrizer decode (recovered
 * clean-room; see decomp/i960/notes/texture_polygon_link_v0831.md).
 *
 * The TGP does not decode textures (tgp_microcode_no_texture_decode_v0830.md);
 * the geometrizer does, while rasterizing a model. An object is submitted with
 * four words -- the object-table entry {w0,w1,w2,w3} at 0x020e0004:
 *
 *   w0 = tpa : texture-point (UV) table address   (u16 index into the texture
 *              region, or texture RAM if bit 0x800000)
 *   w1 = tha : texture-header table address        (likewise)
 *   w2 = oba : polygon-record address              (byte = (w2 & 0x7fffff) * 4)
 *   w3 = obc : primitive count
 *
 * For each primitive of the record (the [attr][normal][vA][vB] quad-strip, see
 * model_decode.h): the UV table yields NumVerts (pv,pu) pairs (13.3 fixed-point
 * texels) and advances by NumVerts*2 u16; the texture-header table yields a
 * 4-u16 header and advances by a SIGNED 5-bit delta ((attr>>12)&0x1f) times 4
 * u16. NumVerts is 4 when (attr & 1) else 3. */

/* A decoded 4-u16 texture header. tex_x/tex_y are the tile's top-left on the
 * logical 2048x1024 sheet; tex_width/tex_height are the tile size in texels. */
typedef struct vf2_texture_header {
    uint32_t sheet;       /* 0 or 1 (which texture bank)                     */
    uint32_t tex_x;       /* 0..2016, multiple of 32                         */
    uint32_t tex_y;       /* 0..992,  multiple of 32                         */
    uint32_t tex_width;   /* 32..4096                                        */
    uint32_t tex_height;  /* 32..4096                                        */
    uint8_t  wrap_x;      /* smooth-wrap enable (disabled when mirrored)     */
    uint8_t  wrap_y;
    uint8_t  mirror_x;
    uint8_t  mirror_y;
    uint8_t  renderer;    /* (th0>>13)&3: bit0 transparent, bit1 textured    */
    uint16_t color_base;  /* (th3>>6)&0x3ff: solid/untextured colour index   */
    uint32_t luma_base;   /* (th1&0xff)<<7                                    */
} vf2_texture_header;

/* Decode the four little-endian u16 header words into the struct above. th must
 * point to four u16; out must be non-NULL. */
void vf2_texture_header_decode(const uint16_t th[4], vf2_texture_header *out);

/* Per-primitive texture-header pointer advance, in u16 words: the signed 5-bit
 * field (attr>>12)&0x1f times 4. May be negative. */
int32_t vf2_texture_header_advance_words(uint32_t attr);

/* Convert a raw 13.3 fixed-point UV component (as stored in the texture-point
 * table) to texels. */
float vf2_texture_uv_texels(uint16_t raw);

/* Fetch one 4-bit luma-index texel at a logical sheet coordinate.
 *
 * bank/bank_size: a raw texture bank (the same bytes vf2_texture_sheet_decode
 * takes; one 2 MiB bank). x in 0..2047, y in 0..1023 (the logical 2048x1024
 * sheet; the hardware stores it folded as 1024x2048 with a 2x2 texel swizzle,
 * applied here). Out-of-range or past the bank returns 0. Correct when the tile
 * origin is a multiple of 2 (VF2 tile origins are multiples of 32). */
uint8_t vf2_texture_sheet_texel(
    const uint8_t *bank,
    size_t bank_size,
    uint32_t x,
    uint32_t y
);

#endif
