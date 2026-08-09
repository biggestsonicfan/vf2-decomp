#ifndef VF2_TGP_H
#define VF2_TGP_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/model2a.h"
#include "vf2/platform.h"
#include "vf2/status.h"

enum {
    VF2_TGP_PROGRAM_WORD_COUNT = 0x1000,
    VF2_TGP_FIFO_WORD_COUNT = 8,
    VF2_TGP_TABLE_WORD_COUNT = 0x10000,
    VF2_TGP_POLYGON_RAM_WORD_COUNT = 0x8000,
    VF2_TGP_BUFFER_WORD_MASK = 0x7fff
};

/*
 * Portable reference geometry.  Matrices use column-major storage, matching
 * the affine form used by the Model 2 camera data.  This is a software
 * renderer boundary, not a claim about the unrecovered TGP packet format.
 */
typedef struct vf2_tgp_matrix {
    float values[16];
} vf2_tgp_matrix;

typedef struct vf2_tgp_vertex {
    float x;
    float y;
    float z;
    float w;
} vf2_tgp_vertex;

typedef struct vf2_tgp_screen_vertex {
    int32_t x;
    int32_t y;
    float depth;
} vf2_tgp_screen_vertex;

typedef struct vf2_tgp {
    const uint8_t *tables;
    size_t tables_size;
    const uint8_t *copro_data;
    size_t copro_data_size;
    const uint8_t *polygon_rom;
    size_t polygon_rom_size;
    uint32_t program[VF2_TGP_PROGRAM_WORD_COUNT];
    uint32_t bank_register;
    uint32_t sincos_base;
    uint32_t atan_base[4];
    uint32_t inverse_base;
    uint32_t inverse_sqrt_base;
    uint32_t input_fifo[VF2_TGP_FIFO_WORD_COUNT];
    uint32_t output_fifo[VF2_TGP_FIFO_WORD_COUNT];
    uint8_t input_read;
    uint8_t input_write;
    uint8_t input_count;
    uint8_t output_read;
    uint8_t output_write;
    uint8_t output_count;
    uint32_t polygon_ram0[VF2_TGP_POLYGON_RAM_WORD_COUNT];
    uint32_t polygon_ram1[VF2_TGP_POLYGON_RAM_WORD_COUNT];
    vf2_tgp_matrix geometry_matrix;
    float geometry_focus_x;
    float geometry_focus_y;
    uint32_t geometry_mode;
} vf2_tgp;

typedef struct vf2_tgp_geometry_stream_report {
    size_t words_consumed;
    size_t commands;
    size_t object_commands;
    size_t direct_commands;
    size_t polygon_data_commands;
    size_t polygon_data_words;
    size_t max_command_words;
    size_t object_links;
    size_t rendered_triangles;
    int ended;
} vf2_tgp_geometry_stream_report;

vf2_status vf2_tgp_initialize(
    vf2_tgp *tgp,
    const uint8_t *tables,
    size_t tables_size,
    const uint8_t *copro_data,
    size_t copro_data_size
);
void vf2_tgp_reset(vf2_tgp *tgp);

vf2_status vf2_tgp_attach_polygon_rom(
    vf2_tgp *tgp,
    const uint8_t *polygon_rom,
    size_t polygon_rom_size
);
vf2_status vf2_tgp_read_polygon_word(
    const vf2_tgp *tgp,
    uint32_t word_address,
    uint32_t *value
);

vf2_status vf2_tgp_upload_program_word(
    vf2_tgp *tgp,
    uint32_t word_index,
    uint32_t value
);
vf2_status vf2_tgp_set_bank(vf2_tgp *tgp, uint32_t value);
vf2_status vf2_tgp_write_function_port(
    vf2_tgp *tgp,
    uint32_t byte_offset,
    uint32_t value
);
vf2_status vf2_tgp_read_input(vf2_tgp *tgp, uint32_t *value);
vf2_status vf2_tgp_write_output(vf2_tgp *tgp, uint32_t value);
vf2_status vf2_tgp_read_output(vf2_tgp *tgp, uint32_t *value);
int vf2_tgp_input_empty(const vf2_tgp *tgp);
int vf2_tgp_output_empty(const vf2_tgp *tgp);

void vf2_tgp_matrix_identity(vf2_tgp_matrix *matrix);
vf2_status vf2_tgp_project_vertex(
    const vf2_tgp_matrix *matrix,
    const vf2_tgp_vertex *vertex,
    uint32_t width,
    uint32_t height,
    vf2_tgp_screen_vertex *screen_vertex
);
vf2_status vf2_tgp_render_triangle(
    vf2_platform *platform,
    const vf2_tgp_matrix *matrix,
    const vf2_tgp_vertex *vertices,
    uint32_t color
);
vf2_status vf2_tgp_geometry_command_words(
    const uint32_t *words,
    size_t word_count,
    size_t *command_words
);
vf2_status vf2_tgp_scan_geometry_stream(
    const uint32_t *words,
    size_t word_count,
    vf2_tgp_geometry_stream_report *report
);
vf2_status vf2_tgp_execute_geometry_stream(
    vf2_tgp *tgp,
    const uint32_t *words,
    size_t word_count,
    vf2_platform *platform,
    uint32_t color,
    vf2_tgp_geometry_stream_report *report
);

vf2_status vf2_tgp_write_sincos_base(vf2_tgp *tgp, uint32_t value);
vf2_status vf2_tgp_read_sincos(
    const vf2_tgp *tgp,
    uint32_t word_offset,
    uint32_t *value
);
vf2_status vf2_tgp_write_atan_word(
    vf2_tgp *tgp,
    uint32_t word_index,
    uint32_t value
);
vf2_status vf2_tgp_read_atan(const vf2_tgp *tgp, uint32_t *value);
vf2_status vf2_tgp_write_inverse_base(vf2_tgp *tgp, uint32_t value);
vf2_status vf2_tgp_read_inverse(
    const vf2_tgp *tgp,
    uint32_t word_offset,
    uint32_t *value
);
vf2_status vf2_tgp_write_inverse_sqrt_base(vf2_tgp *tgp, uint32_t value);
vf2_status vf2_tgp_read_inverse_sqrt(
    const vf2_tgp *tgp,
    uint32_t word_offset,
    uint32_t *value
);

vf2_status vf2_tgp_read_banked_memory(
    const vf2_tgp *tgp,
    const vf2_model2a *machine,
    uint32_t word_offset,
    uint32_t *value
);
vf2_status vf2_tgp_write_banked_memory(
    const vf2_tgp *tgp,
    vf2_model2a *machine,
    uint32_t word_offset,
    uint32_t value
);

#endif
