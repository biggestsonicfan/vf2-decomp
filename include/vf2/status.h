#ifndef VF2_STATUS_H
#define VF2_STATUS_H

typedef enum vf2_status {
    VF2_OK = 0,
    VF2_ERROR_INVALID_ARGUMENT,
    VF2_ERROR_IO,
    VF2_ERROR_OUT_OF_MEMORY,
    VF2_ERROR_BAD_SIZE,
    VF2_ERROR_BAD_CRC32,
    VF2_ERROR_BAD_SHA1,
    VF2_ERROR_OUT_OF_BOUNDS,
    VF2_ERROR_UNSUPPORTED
} vf2_status;

const char *vf2_status_string(vf2_status status);

#endif
