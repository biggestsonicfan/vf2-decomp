#ifndef VF2_SOUND_A0_DESCRIPTOR_H
#define VF2_SOUND_A0_DESCRIPTOR_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Original 68000 at 0x1ed4..0x1f02: six 0x20-byte descriptor
 * candidates, first status-byte bit7 clear. 0x1f04 zeros the
 * selected 32-byte descriptor, then caller writes the postinc
 * record pointer to +4, 0x0001 to +2, and bsets bit7 at +0.
 * This covers FREE entries only. The all-busy DBF fallthrough
 * path is excluded pending a stepper differential.
 * Return selected zero-based index or -1 without mutation.
 */
static int vf2_sound_a0_descriptor_init(
    uint8_t *bank, size_t bank_size, uint32_t record_after_first_byte
)
{
    size_t index;
    if (bank == NULL || bank_size < 6u * 0x20u) {
        return -1;
    }
    for (index = 0u; index < 6u; ++index) {
        uint8_t *descriptor = bank + index * 0x20u;
        if ((descriptor[0] & 0x80u) == 0u) {
            memset(descriptor, 0, 0x20u);
            descriptor[0] = 0x80u;
            descriptor[2] = 0x00u;
            descriptor[3] = 0x01u;
            descriptor[4] = (uint8_t)(record_after_first_byte >> 24u);
            descriptor[5] = (uint8_t)(record_after_first_byte >> 16u);
            descriptor[6] = (uint8_t)(record_after_first_byte >> 8u);
            descriptor[7] = (uint8_t)record_after_first_byte;
            return (int)index;
        }
    }
    return -1;
}
#endif
