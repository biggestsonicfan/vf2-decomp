#ifndef VF2_ANALYSIS_TASKS_H
#define VF2_ANALYSIS_TASKS_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/status.h"

struct vf2_i960_analysis;

#define VF2_TASK_NAME_CAPACITY 41u

typedef struct vf2_task_descriptor {
    uint32_t descriptor_address;
    uint32_t flags;
    uint32_t instance;
    uint32_t stack_size;
    uint32_t entry_point;
    uint32_t state_address;
    uint32_t scheduler_slot;
    char name[VF2_TASK_NAME_CAPACITY];
} vf2_task_descriptor;

typedef struct vf2_task_catalog {
    vf2_task_descriptor *tasks;
    size_t count;
    size_t capacity;
    uint32_t table_start;
    uint32_t table_end;
} vf2_task_catalog;

void vf2_task_catalog_init(vf2_task_catalog *catalog);
void vf2_task_catalog_destroy(vf2_task_catalog *catalog);

vf2_status vf2_task_catalog_scan(
    vf2_task_catalog *catalog,
    const uint8_t *image,
    size_t image_size
);

const vf2_task_descriptor *vf2_task_catalog_find(
    const vf2_task_catalog *catalog,
    const char *name
);

vf2_status vf2_task_catalog_apply_symbols(
    const vf2_task_catalog *catalog,
    struct vf2_i960_analysis *analysis
);

vf2_status vf2_task_catalog_write(
    const vf2_task_catalog *catalog,
    const char *output_directory
);

#endif
