#include "vf2/status.h"

const char *vf2_status_string(vf2_status status)
{
    switch (status) {
    case VF2_OK:
        return "ok";
    case VF2_ERROR_INVALID_ARGUMENT:
        return "invalid argument";
    case VF2_ERROR_IO:
        return "I/O error";
    case VF2_ERROR_OUT_OF_MEMORY:
        return "out of memory";
    case VF2_ERROR_BAD_SIZE:
        return "unexpected file size";
    case VF2_ERROR_BAD_CRC32:
        return "CRC-32 mismatch";
    case VF2_ERROR_BAD_SHA1:
        return "SHA-1 mismatch";
    case VF2_ERROR_OUT_OF_BOUNDS:
        return "out of bounds";
    case VF2_ERROR_UNSUPPORTED:
        return "unsupported operation";
    default:
        return "unknown error";
    }
}
