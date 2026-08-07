#ifndef VF2_I960_SNAPSHOT_H
#define VF2_I960_SNAPSHOT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define VF2_I960_SNAPSHOT_VERSION 5u

typedef struct vf2_i960_snapshot {
    vf2_i960_cpu cpu;
    uint8_t *geometry;
    size_t geometry_size;
    uint8_t *copro_port;
    size_t copro_port_size;
    uint8_t *work_ram;
    size_t work_ram_size;
    uint8_t *buffer_ram;
    size_t buffer_ram_size;
    uint8_t *video_control;
    size_t video_control_size;
    uint8_t *cpu_control;
    size_t cpu_control_size;
    uint8_t *interrupt_control;
    size_t interrupt_control_size;
    uint8_t *timers;
    size_t timers_size;
    uint8_t *tile_ram;
    size_t tile_ram_size;
    uint8_t *palette_ram;
    size_t palette_ram_size;
    uint8_t *io_control;
    size_t io_control_size;
    uint8_t *backup_sram;
    size_t backup_sram_size;
    uint8_t *copro_control;
    size_t copro_control_size;
    uint8_t *color_translation;
    size_t color_translation_size;
    uint8_t *texture_ram0;
    size_t texture_ram0_size;
    uint8_t *texture_ram1;
    size_t texture_ram1_size;
    uint8_t *luma_ram;
    size_t luma_ram_size;
    uint8_t *system_control;
    size_t system_control_size;
} vf2_i960_snapshot;

typedef struct vf2_i960_snapshot_diff {
    bool equal;
    char component[32];
    size_t differing_bytes;
    size_t first_offset;
    uint32_t expected_value;
    uint32_t actual_value;
} vf2_i960_snapshot_diff;

void vf2_i960_snapshot_init(vf2_i960_snapshot *snapshot);
void vf2_i960_snapshot_destroy(vf2_i960_snapshot *snapshot);

vf2_status vf2_i960_snapshot_capture(
    vf2_i960_snapshot *snapshot,
    const vf2_i960_cpu *cpu,
    const vf2_model2a *machine
);

vf2_status vf2_i960_snapshot_restore(
    const vf2_i960_snapshot *snapshot,
    vf2_i960_cpu *cpu,
    vf2_model2a *machine
);

vf2_status vf2_i960_snapshot_write_file(
    const vf2_i960_snapshot *snapshot,
    const char *path
);

vf2_status vf2_i960_snapshot_read_file(
    vf2_i960_snapshot *snapshot,
    const char *path
);

vf2_status vf2_i960_snapshot_compare(
    const vf2_i960_snapshot *expected,
    const vf2_i960_snapshot *actual,
    vf2_i960_snapshot_diff *diff
);

/* Compare two live CPU/machine states without allocating snapshot buffers. */
vf2_status vf2_i960_compare_live_state(
    const vf2_i960_cpu *expected_cpu,
    const vf2_model2a *expected_machine,
    const vf2_i960_cpu *actual_cpu,
    const vf2_model2a *actual_machine,
    vf2_i960_snapshot_diff *diff
);

/* Compare captured memory regions while intentionally ignoring CPU state. */
vf2_status vf2_i960_snapshot_compare_memory(
    const vf2_i960_snapshot *expected,
    const vf2_i960_snapshot *actual,
    vf2_i960_snapshot_diff *diff
);

#endif
