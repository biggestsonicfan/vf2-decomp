#include "vf2/analysis/image_map.h"

const char *vf2_image_class_name(vf2_image_class classification)
{
    switch (classification) {
    case VF2_IMAGE_CODE:
        return "code";
    case VF2_IMAGE_DATA:
        return "data";
    case VF2_IMAGE_STRING:
        return "string";
    case VF2_IMAGE_POINTER_TABLE:
        return "pointer-table";
    case VF2_IMAGE_PADDING:
        return "padding";
    case VF2_IMAGE_UNKNOWN:
    default:
        return "unknown";
    }
}
