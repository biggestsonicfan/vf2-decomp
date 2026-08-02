#ifndef VF2_BOOT_H
#define VF2_BOOT_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/status.h"

typedef struct vf2_i960_boot_vectors {
    uint32_t system_address_table;
    uint32_t initial_prcb;
    uint32_t reserved;
    uint32_t start_ip;
    uint32_t raw_words[8];
} vf2_i960_boot_vectors;

vf2_status vf2_parse_i960_boot_vectors(
    const uint8_t *maincpu,
    size_t maincpu_size,
    vf2_i960_boot_vectors *vectors
);

#endif
