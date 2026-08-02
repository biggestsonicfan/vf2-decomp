#ifndef VF2_ANALYSIS_IMAGE_MAP_H
#define VF2_ANALYSIS_IMAGE_MAP_H

#include <stddef.h>
#include <stdint.h>

typedef enum vf2_image_class {
    VF2_IMAGE_UNKNOWN = 0,
    VF2_IMAGE_CODE,
    VF2_IMAGE_DATA,
    VF2_IMAGE_STRING,
    VF2_IMAGE_POINTER_TABLE,
    VF2_IMAGE_PADDING
} vf2_image_class;

const char *vf2_image_class_name(vf2_image_class classification);

#endif
