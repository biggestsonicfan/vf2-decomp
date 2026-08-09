#ifndef VF2_GEOMETRY_H
#define VF2_GEOMETRY_H

#include <stdint.h>

#include "vf2/model2a.h"
#include "vf2/status.h"

enum {
    VF2_GEOMETRY_COMMAND_RING_COUNT = 4u,
    VF2_GEOMETRY_COMMAND_TABLE = 0x00007a00u,
    VF2_GEOMETRY_COMMAND_STATE = 0x00501004u,
    VF2_GEOMETRY_MAX_DISTANCE = 0x00501008u,
    VF2_GEOMETRY_RING_INDEX = 0x0050100cu,
    VF2_GEOMETRY_COMMAND_SOURCE_CLASS = 0x005010dcu,
    VF2_GEOMETRY_COMMAND_SOURCE_VALUE = 0x005010deu,
    VF2_GEOMETRY_COMMAND_RESULT = 0x005010e0u,
    VF2_GEOMETRY_STATUS_OFFSET = 0x00000080u,
    VF2_GEOMETRY_FRAME_STATUS_OFFSET = 0x000000f0u,
    VF2_GEOMETRY_WRITE_OFFSET = 0x00001008u,
    VF2_GEOMETRY_READ_OFFSET = 0x00002008u,
    VF2_GEOMETRY_PREVIOUS_OFFSET = 0x00003008u,
    VF2_GEOMETRY_RING_MASK = 0x0007fffcu
};

typedef struct vf2_geometry_commit_report {
    int32_t distance;
    uint32_t maximum_distance;
    uint32_t next_command;
    uint8_t ring_index;
    int maximum_updated;
} vf2_geometry_commit_report;

uint32_t vf2_geometry_pack_command(int16_t source_value, uint8_t command_class);

vf2_status vf2_geometry_setup_command(
    vf2_model2a *machine,
    uint32_t geometry_base,
    uint32_t destination_offset,
    int16_t source_value,
    uint8_t command_class,
    uint32_t *command
);

vf2_status vf2_geometry_commit_frame(
    vf2_model2a *machine,
    uint32_t geometry_base,
    vf2_geometry_commit_report *report
);

#endif
