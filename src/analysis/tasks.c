#include "vf2/analysis/tasks.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/file.h"
#include "vf2/analysis/cfg.h"
#include "vf2/model2a.h"

#define VF2_TASK_RECORD_SIZE 0x40u
#define VF2_TASK_NAME_OFFSET 0x18u
#define VF2_TASK_NAME_FIELD_SIZE 0x28u

static uint32_t read_u32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8u) |
           ((uint32_t)data[2] << 16u) | ((uint32_t)data[3] << 24u);
}

static int valid_name_character(uint8_t value)
{
    return value == (uint8_t)' ' || value == (uint8_t)'_' ||
           isalnum((int)value) != 0;
}

static int parse_task_record(
    const uint8_t *image,
    size_t image_size,
    uint32_t address,
    vf2_task_descriptor *task
)
{
    const uint8_t *record = NULL;
    size_t name_length = 0u;
    size_t index = 0u;
    uint32_t entry = 0u;
    uint32_t state = 0u;

    if ((size_t)address + VF2_TASK_RECORD_SIZE > image_size || task == NULL) {
        return 0;
    }
    record = image + address;
    entry = read_u32(record + 0x0cu);
    state = read_u32(record + 0x10u);
    if ((entry & 3u) != 0u || entry >= image_size ||
        state < VF2_WORK_RAM_BASE || state >= VF2_WORK_RAM_BASE + VF2_WORK_RAM_SIZE) {
        return 0;
    }
    if (record[VF2_TASK_NAME_OFFSET] != (uint8_t)'f' ||
        record[VF2_TASK_NAME_OFFSET + 1u] != (uint8_t)'a' ||
        record[VF2_TASK_NAME_OFFSET + 2u] != (uint8_t)'_') {
        return 0;
    }
    while (name_length < VF2_TASK_NAME_FIELD_SIZE &&
           record[VF2_TASK_NAME_OFFSET + name_length] != 0u) {
        if (!valid_name_character(record[VF2_TASK_NAME_OFFSET + name_length])) {
            return 0;
        }
        ++name_length;
    }
    while (name_length > 0u && record[VF2_TASK_NAME_OFFSET + name_length - 1u] == (uint8_t)' ') {
        --name_length;
    }
    if (name_length < 4u || name_length >= VF2_TASK_NAME_CAPACITY) {
        return 0;
    }

    memset(task, 0, sizeof(*task));
    task->descriptor_address = address;
    task->flags = read_u32(record + 0x00u);
    task->instance = read_u32(record + 0x04u);
    task->stack_size = read_u32(record + 0x08u);
    task->entry_point = entry;
    task->state_address = state;
    task->scheduler_slot = read_u32(record + 0x14u);
    for (index = 0u; index < name_length; ++index) {
        task->name[index] = (char)record[VF2_TASK_NAME_OFFSET + index];
    }
    task->name[name_length] = '\0';
    return 1;
}

static vf2_status reserve_tasks(vf2_task_catalog *catalog, size_t required)
{
    size_t capacity = 0u;
    vf2_task_descriptor *tasks = NULL;
    if (required <= catalog->capacity) {
        return VF2_OK;
    }
    capacity = catalog->capacity == 0u ? 32u : catalog->capacity;
    while (capacity < required) {
        if (capacity > SIZE_MAX / 2u) {
            return VF2_ERROR_OUT_OF_MEMORY;
        }
        capacity *= 2u;
    }
    tasks = (vf2_task_descriptor *)realloc(catalog->tasks, capacity * sizeof(*tasks));
    if (tasks == NULL) {
        return VF2_ERROR_OUT_OF_MEMORY;
    }
    catalog->tasks = tasks;
    catalog->capacity = capacity;
    return VF2_OK;
}

void vf2_task_catalog_init(vf2_task_catalog *catalog)
{
    if (catalog != NULL) {
        memset(catalog, 0, sizeof(*catalog));
    }
}

void vf2_task_catalog_destroy(vf2_task_catalog *catalog)
{
    if (catalog != NULL) {
        free(catalog->tasks);
        memset(catalog, 0, sizeof(*catalog));
    }
}

vf2_status vf2_task_catalog_scan(
    vf2_task_catalog *catalog,
    const uint8_t *image,
    size_t image_size
)
{
    uint32_t address = 0u;
    uint32_t best_start = 0u;
    size_t best_count = 0u;
    vf2_status status = VF2_OK;

    if (catalog == NULL || image == NULL || image_size < VF2_TASK_RECORD_SIZE) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    vf2_task_catalog_destroy(catalog);

    for (address = 0u; (size_t)address + VF2_TASK_RECORD_SIZE <= image_size; address += 4u) {
        vf2_task_descriptor task;
        size_t count = 0u;
        uint32_t cursor = address;
        while (parse_task_record(image, image_size, cursor, &task)) {
            ++count;
            cursor += VF2_TASK_RECORD_SIZE;
        }
        if (count > best_count) {
            best_count = count;
            best_start = address;
        }
    }
    if (best_count == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = reserve_tasks(catalog, best_count);
    if (status != VF2_OK) {
        return status;
    }
    for (address = best_start; catalog->count < best_count; address += VF2_TASK_RECORD_SIZE) {
        if (!parse_task_record(image, image_size, address, &catalog->tasks[catalog->count])) {
            vf2_task_catalog_destroy(catalog);
            return VF2_ERROR_BAD_SIZE;
        }
        ++catalog->count;
    }
    catalog->table_start = best_start;
    catalog->table_end = best_start + (uint32_t)(best_count * VF2_TASK_RECORD_SIZE);
    return VF2_OK;
}

const vf2_task_descriptor *vf2_task_catalog_find(
    const vf2_task_catalog *catalog,
    const char *name
)
{
    size_t index = 0u;
    if (catalog == NULL || name == NULL) {
        return NULL;
    }
    for (index = 0u; index < catalog->count; ++index) {
        if (strcmp(catalog->tasks[index].name, name) == 0) {
            return &catalog->tasks[index];
        }
    }
    return NULL;
}

vf2_status vf2_task_catalog_apply_symbols(
    const vf2_task_catalog *catalog,
    struct vf2_i960_analysis *analysis
)
{
    size_t task_index = 0u;
    if (catalog == NULL || analysis == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    for (task_index = 0u; task_index < catalog->count; ++task_index) {
        size_t function_index = 0u;
        size_t shared_count = 0u;
        size_t peer_index = 0u;
        const vf2_task_descriptor *task = &catalog->tasks[task_index];
        const char *suffix = strncmp(task->name, "fa_", 3u) == 0
            ? task->name + 3u
            : task->name;
        char canonical[VF2_TASK_NAME_CAPACITY];
        size_t length = strlen(suffix);

        for (peer_index = 0u; peer_index < catalog->count; ++peer_index) {
            if (catalog->tasks[peer_index].entry_point == task->entry_point) {
                ++shared_count;
            }
        }
        (void)snprintf(canonical, sizeof(canonical), "%s", suffix);
        if (shared_count > 1u) {
            while (length > 0u && isdigit((unsigned char)canonical[length - 1u]) != 0) {
                canonical[--length] = '\0';
            }
        }

        for (function_index = 0u; function_index < analysis->function_count; ++function_index) {
            vf2_function *function = &analysis->functions[function_index];
            if (function->address == task->entry_point) {
                (void)snprintf(function->name, sizeof(function->name), "task_%s", canonical);
                function->user_named = true;
                function->confirmed = true;
                break;
            }
        }
    }
    return VF2_OK;
}

static vf2_status open_output(const char *directory, const char *name, FILE **file)
{
    char path[1024];
    int length = 0;
    if (directory == NULL || name == NULL || file == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (vf2_make_directories(directory) != VF2_OK) {
        return VF2_ERROR_IO;
    }
    length = snprintf(path, sizeof(path), "%s/%s", directory, name);
    if (length < 0 || (size_t)length >= sizeof(path)) {
        return VF2_ERROR_BAD_SIZE;
    }
    *file = fopen(path, "wb");
    return *file != NULL ? VF2_OK : VF2_ERROR_IO;
}

static void write_csv_name(FILE *file, const char *name)
{
    const char *cursor = name;
    fputc('"', file);
    while (*cursor != '\0') {
        if (*cursor == '"') {
            fputc('"', file);
        }
        fputc(*cursor, file);
        ++cursor;
    }
    fputc('"', file);
}

vf2_status vf2_task_catalog_write(
    const vf2_task_catalog *catalog,
    const char *output_directory
)
{
    FILE *csv = NULL;
    FILE *json = NULL;
    FILE *dot = NULL;
    size_t index = 0u;
    vf2_status status = VF2_OK;
    if (catalog == NULL || output_directory == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    status = open_output(output_directory, "tasks.csv", &csv);
    if (status == VF2_OK) {
        fputs("descriptor,name,flags,instance,stack_size,entry_point,state_address,scheduler_slot\n", csv);
        for (index = 0u; index < catalog->count; ++index) {
            const vf2_task_descriptor *task = &catalog->tasks[index];
            fprintf(csv, "0x%08x,", (unsigned)task->descriptor_address);
            write_csv_name(csv, task->name);
            fprintf(csv, ",0x%08x,%u,0x%x,0x%08x,0x%08x,%u\n",
                    (unsigned)task->flags, (unsigned)task->instance,
                    (unsigned)task->stack_size, (unsigned)task->entry_point,
                    (unsigned)task->state_address, (unsigned)task->scheduler_slot);
        }
        if (fclose(csv) != 0) {
            status = VF2_ERROR_IO;
        }
    }

    if (status == VF2_OK) {
        status = open_output(output_directory, "tasks.json", &json);
    }
    if (status == VF2_OK) {
        fprintf(json, "{\n  \"table_start\": \"0x%08x\",\n  \"table_end\": \"0x%08x\",\n  \"task_count\": %zu,\n  \"tasks\": [\n",
                (unsigned)catalog->table_start, (unsigned)catalog->table_end,
                catalog->count);
        for (index = 0u; index < catalog->count; ++index) {
            const vf2_task_descriptor *task = &catalog->tasks[index];
            fprintf(json,
                    "    {\"name\": \"%s\", \"descriptor\": \"0x%08x\", \"flags\": \"0x%08x\", \"instance\": %u, \"stack_size\": %u, \"entry_point\": \"0x%08x\", \"state_address\": \"0x%08x\", \"scheduler_slot\": %u}%s\n",
                    task->name, (unsigned)task->descriptor_address,
                    (unsigned)task->flags, (unsigned)task->instance,
                    (unsigned)task->stack_size, (unsigned)task->entry_point,
                    (unsigned)task->state_address, (unsigned)task->scheduler_slot,
                    index + 1u == catalog->count ? "" : ",");
        }
        fputs("  ]\n}\n", json);
        if (fclose(json) != 0) {
            status = VF2_ERROR_IO;
        }
    }

    if (status == VF2_OK) {
        status = open_output(output_directory, "tasks.dot", &dot);
    }
    if (status == VF2_OK) {
        fputs("digraph vf2_tasks {\n  rankdir=LR;\n  node [shape=box,fontname=monospace];\n", dot);
        for (index = 0u; index < catalog->count; ++index) {
            const vf2_task_descriptor *task = &catalog->tasks[index];
            fprintf(dot,
                    "  task_%08x [label=\"%s\\nentry 0x%08x\\nstate 0x%08x\\nstack 0x%x\"];\n",
                    (unsigned)task->descriptor_address, task->name,
                    (unsigned)task->entry_point, (unsigned)task->state_address,
                    (unsigned)task->stack_size);
        }
        fputs("}\n", dot);
        if (fclose(dot) != 0) {
            status = VF2_ERROR_IO;
        }
    }
    return status;
}
