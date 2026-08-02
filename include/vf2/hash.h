#ifndef VF2_HASH_H
#define VF2_HASH_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/status.h"

#define VF2_SHA1_SIZE 20
#define VF2_SHA1_HEX_SIZE 41

uint32_t vf2_crc32(const void *data, size_t size);
void vf2_sha1(const void *data, size_t size, uint8_t digest[VF2_SHA1_SIZE]);
void vf2_sha1_to_hex(
    const uint8_t digest[VF2_SHA1_SIZE],
    char output[VF2_SHA1_HEX_SIZE]
);

vf2_status vf2_hash_file(
    const char *path,
    size_t *size_out,
    uint32_t *crc32_out,
    uint8_t sha1_out[VF2_SHA1_SIZE]
);

#endif
