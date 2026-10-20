#ifndef VF2_SOUND_L2_AD_H
#define VF2_SOUND_L2_AD_H
#include <stddef.h>
#include <stdint.h>

/* Verified 68000 A0 record -> B-class and 90-class output sequence.
 * 0xff/0x2f ends the stream. Preserve embedded index including AD24
 * 0x7c's historical alias (0x70). No ROM bytes are copied here.
 */
static int vf2_sound_l2_ad_expand(const uint8_t *rom, size_t rom_size,
                                 uint32_t after_header, uint8_t packets[6])
{
    size_t offset;
    const uint8_t *p;
    int ad23;
    uint32_t record;
    if (rom == NULL || packets == NULL || after_header <= 0x600000u) return 0;
    record = after_header - 1u;
    if (!((record >= 0x60b1e6u && record < 0x60b7e6u &&
           (record - 0x60b1e6u) % 12u == 0u) ||
          (record >= 0x60b7e6u && record < 0x60bde6u &&
           (record - 0x60b7e6u) % 12u == 0u))) return 0;
    offset = (size_t)(after_header - 0x600000u - 1u);
    if (offset > rom_size || rom_size - offset < 12u) return 0;
    p = rom + offset;
    ad23 = p[1] == 0xbbu;
    if (p[0] != 0x8fu || (!ad23 && p[1] != 0xbau) ||
        (ad23 && record >= 0x60b7e6u) ||
        (!ad23 && record < 0x60b7e6u) ||
        p[2] != 0x21u || p[3] != (uint8_t)(ad23 ? 0x06u : 0x07u) ||
        p[4] != 0x00u || p[5] != (uint8_t)(ad23 ? 0x9bu : 0x9au) ||
        p[6] > 0x7fu ||
        p[7] != (uint8_t)(ad23 && p[6] < 0x44u ? 0x78u : 0x7fu) ||
        p[8] != 0x00u || p[9] != 0xffu || p[10] != 0x2fu || p[11] != 0x00u)
        return 0;
    packets[0] = p[1]; packets[1] = p[2]; packets[2] = p[3];
    packets[3] = p[5]; packets[4] = p[6]; packets[5] = p[7];
    return 1;
}

/* Owner-ROM-verified original frame-342 AD 21 00 -> stream #1.
 * Its record at 0x60A6B2 is NOT the 0x8F B/90 two-packet AD23/24
 * grammar above. The authentic 68000 L2 interpreter emits twelve
 * B9..BE sound-ring packets from a 52-byte B/CF record in one tick.
 *
 * Original record layout is twelve four-byte instructions:
 *   [opcode=B9..BE, metadata, third-byte, payload-byte]
 * followed by FF 00 00 2F. For this EXACT ROM record, the M68000
 * filed [opcode, payload-byte, third-byte] and left each fourth
 * ring byte untouched. The original 68000/SCSP differential
 * independently pins the resulting 12 files, descriptor retirement
 * and timer/control pokes. No other record address or grammar is
 * admitted until tested.
 */
static inline int vf2_sound_l2_ad21_expand(const uint8_t *rom,
                                   size_t rom_size,
                                   uint32_t after_header,
                                   uint8_t packets[36])
{
    size_t offset, i;
    const uint8_t *p;
    if (rom == NULL || packets == NULL ||
        after_header != UINT32_C(0x60a6b3)) {
        return 0;
    }
    offset = (size_t)(after_header - UINT32_C(0x600001));
    if (offset > rom_size || rom_size - offset < 52u) return 0;
    p = rom + offset;
    for (i = 0u; i < 12u; ++i) {
        size_t at = i * 4u;
        uint8_t code = (uint8_t)(UINT8_C(0xb9) + (i % 6u));
        uint8_t want_meta = i == 0u ? UINT8_C(0xcf) : 0u;
        uint8_t want_third = i < 6u ? 0u : UINT8_C(0x7f);
        uint8_t want_payload = i < 6u ? UINT8_C(0x2a) : 7u;
        if (p[at] != code || p[at + 1u] != want_meta ||
            p[at + 2u] != want_third ||
            p[at + 3u] != want_payload) {
            return 0;
        }
    }
    if (p[48] != UINT8_C(0xff) || p[49] != 0u ||
        p[50] != 0u || p[51] != UINT8_C(0x2f)) return 0;
    for (i = 0u; i < 12u; ++i) {
        size_t at = i * 4u;
        packets[i * 3u] = p[at];
        packets[i * 3u + 1u] = p[at + 3u];
        packets[i * 3u + 2u] = p[at + 2u];
    }
    return 1;
}
#endif
