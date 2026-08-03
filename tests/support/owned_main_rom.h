#ifndef VF2_TEST_OWNED_MAIN_ROM_H
#define VF2_TEST_OWNED_MAIN_ROM_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "vf2/model2a.h"

vf2_status vf2_test_attach_owned_main_rom(
    vf2_model2a *machine,
    const uint8_t *main_rom,
    size_t main_rom_size
);

void vf2_test_free_owned_main_rom(void *pointer);

#define vf2_model2a_attach_main_rom(machine, main_rom, main_rom_size) \
    vf2_test_attach_owned_main_rom((machine), (main_rom), (main_rom_size))

#define free(pointer) vf2_test_free_owned_main_rom((pointer))

#endif
