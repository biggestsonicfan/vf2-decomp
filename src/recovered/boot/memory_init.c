#include "vf2/recovered.h"

#include <string.h>

vf2_status vf2_recovered_memory_clear_u32(
    uint8_t *memory,
    size_t memory_size,
    size_t byte_offset,
    size_t word_count
)
{
    size_t byte_count = 0u;

    if (memory == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (word_count > SIZE_MAX / 4u) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }

    byte_count = word_count * 4u;
    if (byte_offset > memory_size || byte_count > memory_size - byte_offset) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }

    memset(memory + byte_offset, 0, byte_count);
    return VF2_OK;
}
