#include "vf2/boot.h"

static uint32_t read_le32(const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8u) |
           ((uint32_t)data[2] << 16u) |
           ((uint32_t)data[3] << 24u);
}

vf2_status vf2_parse_i960_boot_vectors(
    const uint8_t *maincpu,
    size_t maincpu_size,
    vf2_i960_boot_vectors *vectors
)
{
    size_t index = 0u;

    if (maincpu == NULL || vectors == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    if (maincpu_size < 32u) {
        return VF2_ERROR_BAD_SIZE;
    }

    for (index = 0u; index < 8u; ++index) {
        vectors->raw_words[index] = read_le32(maincpu + (index * 4u));
    }

    vectors->system_address_table = vectors->raw_words[0];
    vectors->initial_prcb = vectors->raw_words[1];
    vectors->reserved = vectors->raw_words[2];
    vectors->start_ip = vectors->raw_words[3];

    return VF2_OK;
}
