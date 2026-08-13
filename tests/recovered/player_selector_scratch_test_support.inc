#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vf2/model2a.h"
#include "vf2/status.h"

#include "../../src/recovered/player_selector_scratch.inc"

#define SCRATCH_TEST_PLAYER UINT32_C(0x00512000)
#define SCRATCH_TEST_BASE UINT32_C(0x00518000)
#define SCRATCH_TEST_TABLE UINT32_C(0x02050000)
#define SCRATCH_TEST_SELECTOR UINT16_C(0x0123)
#define SCRATCH_TEST_MAIN_DATA_SIZE UINT32_C(0x00200000)

static uint8_t scratch_test_main_rom[VF2_MAIN_ROM_SIZE];
static uint8_t scratch_test_main_data[SCRATCH_TEST_MAIN_DATA_SIZE];
static int failures = 0;

#define CHECK(expression)                                           \
    do {                                                            \
        if (!(expression)) {                                        \
            fprintf(stderr, "FAILED %s:%d: %s\n",                 \
                    __FILE__, __LINE__, #expression);              \
            ++failures;                                             \
        }                                                           \
    } while (0)

static uint32_t scratch_test_float_bits(float value)
{
    uint32_t raw = 0u;
    memcpy(&raw, &value, sizeof(raw));
    return raw;
}

static void scratch_test_store_u32(
    uint8_t *data,
    size_t offset,
    uint32_t value
)
{
    data[offset] = (uint8_t)value;
    data[offset + 1u] = (uint8_t)(value >> 8u);
    data[offset + 2u] = (uint8_t)(value >> 16u);
    data[offset + 3u] = (uint8_t)(value >> 24u);
}

static uint8_t *scratch_test_main_data_pointer(
    uint32_t address,
    size_t size
)
{
    const uint32_t offset = address - VF2_MAIN_DATA_BASE;

    if (address < VF2_MAIN_DATA_BASE ||
        (uint64_t)offset + (uint64_t)size >
            (uint64_t)SCRATCH_TEST_MAIN_DATA_SIZE) {
        return NULL;
    }
    return scratch_test_main_data + offset;
}

static void scratch_test_main_data_u8(uint32_t address, uint8_t value)
{
    uint8_t *destination = scratch_test_main_data_pointer(address, 1u);
    CHECK(destination != NULL);
    if (destination != NULL) {
        *destination = value;
    }
}

static void scratch_test_main_data_u32(uint32_t address, uint32_t value)
{
    uint8_t *destination = scratch_test_main_data_pointer(address, 4u);
    CHECK(destination != NULL);
    if (destination != NULL) {
        scratch_test_store_u32(destination, 0u, value);
    }
}

static void scratch_test_main_rom_u32(uint32_t address, uint32_t value)
{
    CHECK((uint64_t)address + UINT64_C(4) <= sizeof(scratch_test_main_rom));
    if ((uint64_t)address + UINT64_C(4) <= sizeof(scratch_test_main_rom)) {
        scratch_test_store_u32(scratch_test_main_rom, (size_t)address, value);
    }
}

static uint8_t scratch_test_read_u8(
    const vf2_model2a *machine,
    uint32_t address
)
{
    uint8_t result = 0u;
    CHECK(vf2_model2a_read(machine, address, &result, sizeof(result)) == VF2_OK);
    return result;
}

static uint16_t scratch_test_read_u16(
    const vf2_model2a *machine,
    uint32_t address
)
{
    uint8_t raw[2] = {0u, 0u};
    CHECK(vf2_model2a_read(machine, address, raw, sizeof(raw)) == VF2_OK);
    return (uint16_t)((uint16_t)raw[0] | ((uint16_t)raw[1] << 8u));
}

static uint32_t scratch_test_read_u32(
    const vf2_model2a *machine,
    uint32_t address
)
{
    uint32_t result = 0u;
    CHECK(vf2_model2a_read_u32(machine, address, &result) == VF2_OK);
    return result;
}
