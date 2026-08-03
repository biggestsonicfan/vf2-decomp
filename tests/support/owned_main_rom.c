#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "vf2/model2a.h"

#define VF2_TEST_OWNED_MAIN_ROM_CAPACITY 64u

static const uint8_t *owned_main_roms[VF2_TEST_OWNED_MAIN_ROM_CAPACITY];
static int cleanup_registered = 0;

static void release_remaining_main_roms(void)
{
    size_t index = 0u;
    for (index = 0u; index < VF2_TEST_OWNED_MAIN_ROM_CAPACITY; ++index) {
        free((void *)owned_main_roms[index]);
        owned_main_roms[index] = NULL;
    }
}

static vf2_status track_main_rom(const uint8_t *main_rom)
{
    size_t index = 0u;
    size_t empty = VF2_TEST_OWNED_MAIN_ROM_CAPACITY;

    for (index = 0u; index < VF2_TEST_OWNED_MAIN_ROM_CAPACITY; ++index) {
        if (owned_main_roms[index] == main_rom) {
            return VF2_OK;
        }
        if (owned_main_roms[index] == NULL &&
            empty == VF2_TEST_OWNED_MAIN_ROM_CAPACITY) {
            empty = index;
        }
    }

    if (empty == VF2_TEST_OWNED_MAIN_ROM_CAPACITY) {
        return VF2_ERROR_OUT_OF_MEMORY;
    }
    if (!cleanup_registered) {
        if (atexit(release_remaining_main_roms) != 0) {
            return VF2_ERROR_OUT_OF_MEMORY;
        }
        cleanup_registered = 1;
    }

    owned_main_roms[empty] = main_rom;
    return VF2_OK;
}

vf2_status vf2_test_attach_owned_main_rom(
    vf2_model2a *machine,
    const uint8_t *main_rom,
    size_t main_rom_size
)
{
    vf2_status status = vf2_model2a_attach_main_rom(
        machine,
        main_rom,
        main_rom_size
    );
    if (status == VF2_OK) {
        status = track_main_rom(main_rom);
    }
    return status;
}

void vf2_test_free_owned_main_rom(void *pointer)
{
    size_t index = 0u;

    if (pointer == NULL) {
        return;
    }
    for (index = 0u; index < VF2_TEST_OWNED_MAIN_ROM_CAPACITY; ++index) {
        if (owned_main_roms[index] == pointer) {
            owned_main_roms[index] = NULL;
            break;
        }
    }
    free(pointer);
}
