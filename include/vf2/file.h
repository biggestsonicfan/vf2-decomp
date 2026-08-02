#ifndef VF2_FILE_H
#define VF2_FILE_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/status.h"

vf2_status vf2_read_file(
    const char *path,
    uint8_t **data_out,
    size_t *size_out
);

vf2_status vf2_write_file(
    const char *path,
    const void *data,
    size_t size
);

vf2_status vf2_make_directory(const char *path);
vf2_status vf2_make_directories(const char *path);

vf2_status vf2_join_path(
    char *output,
    size_t output_size,
    const char *directory,
    const char *filename
);

#endif
