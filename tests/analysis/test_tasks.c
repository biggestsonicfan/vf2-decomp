#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/analysis/tasks.h"
#include "vf2/model2a.h"

static void write_le32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
    data[2] = (uint8_t)(value >> 16u);
    data[3] = (uint8_t)(value >> 24u);
}

static void write_task(
    uint8_t *image,
    uint32_t address,
    uint32_t flags,
    uint32_t instance,
    uint32_t stack_size,
    uint32_t entry,
    uint32_t state,
    uint32_t scheduler_slot,
    const char *name
)
{
    write_le32(image + address + 0x00u, flags);
    write_le32(image + address + 0x04u, instance);
    write_le32(image + address + 0x08u, stack_size);
    write_le32(image + address + 0x0cu, entry);
    write_le32(image + address + 0x10u, state);
    write_le32(image + address + 0x14u, scheduler_slot);
    (void)strncpy((char *)(image + address + 0x18u), name, 0x27u);
}

int vf2_test_task_catalog(void)
{
    uint8_t image[0x400];
    vf2_task_catalog catalog;
    const vf2_task_descriptor *task = NULL;
    vf2_status status = VF2_OK;

    memset(image, 0, sizeof(image));
    write_task(image, 0x100u, 0x80000000u, 0u, 0x480u,
               0x40u, VF2_WORK_RAM_BASE + 0x814u, 3u, "fa_camera");
    write_task(image, 0x140u, 0u, 1u, 0x2000u,
               0x80u, VF2_WORK_RAM_BASE + 0x808u, 0u, "fa_rob1");

    vf2_task_catalog_init(&catalog);
    status = vf2_task_catalog_scan(&catalog, image, sizeof(image));
    if (status != VF2_OK || catalog.count != 2u ||
        catalog.table_start != 0x100u || catalog.table_end != 0x180u) {
        vf2_task_catalog_destroy(&catalog);
        return 1;
    }
    task = vf2_task_catalog_find(&catalog, "fa_camera");
    if (task == NULL || task->entry_point != 0x40u ||
        task->state_address != VF2_WORK_RAM_BASE + 0x814u ||
        task->stack_size != 0x480u || task->flags != 0x80000000u ||
        task->scheduler_slot != 3u) {
        vf2_task_catalog_destroy(&catalog);
        return 2;
    }
    task = vf2_task_catalog_find(&catalog, "fa_rob1");
    if (task == NULL || task->instance != 1u || task->entry_point != 0x80u) {
        vf2_task_catalog_destroy(&catalog);
        return 3;
    }
    vf2_task_catalog_destroy(&catalog);
    return 0;
}
