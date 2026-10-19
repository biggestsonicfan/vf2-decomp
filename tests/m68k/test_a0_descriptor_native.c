#include "../../src/hardware/sound_a0_descriptor.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); ++failures; } } while (0)

static void exercise(size_t free_slot, uint32_t after_byte)
{
    uint8_t slots[6u*0x20u], before[6u*0x20u];
    size_t i, j;
    memset(slots, 0xa5, sizeof(slots));
    for (i = 0u; i < 6u; ++i) {
        slots[i*0x20u] = 0x80u;
    }
    for (j = 0u; j < 0x20u; ++j) {
        slots[free_slot*0x20u+j] = 0x5au;
    }
    slots[free_slot*0x20u] = 0u;
    memcpy(before,slots,sizeof(slots));
    CHECK(vf2_sound_a0_descriptor_init(slots,sizeof(slots),after_byte) == (int)free_slot);
    for (i = 0u; i < 6u; ++i) {
        for (j = 0u; j < 0x20u; ++j) {
            uint8_t want = before[i*0x20u+j];
            if (i == free_slot) {
                want = 0u;
                if (j == 0u) want = 0x80u;
                if (j == 3u) want = 1u;
                if (j == 4u) want = (uint8_t)(after_byte >> 24u);
                if (j == 5u) want = (uint8_t)(after_byte >> 16u);
                if (j == 6u) want = (uint8_t)(after_byte >> 8u);
                if (j == 7u) want = (uint8_t)after_byte;
            }
            CHECK(slots[i*0x20u+j] == want);
        }
    }
}

int main(void)
{
    uint8_t slots[6u*0x20u], before[6u*0x20u];
    size_t i;
    for (i = 0u; i < 6u; ++i) {
        exercise(i, UINT32_C(0x0060b4ff));
        exercise(i, UINT32_C(0x0060b7f3));
    }
    memset(slots, 0x80, sizeof(slots));
    memcpy(before, slots, sizeof(slots));
    CHECK(vf2_sound_a0_descriptor_init(slots,sizeof(slots),UINT32_C(0x60b4ff)) == -1);
    CHECK(memcmp(slots,before,sizeof(slots)) == 0);
    slots[0] = 0u;
    memcpy(before,slots,sizeof(slots));
    CHECK(vf2_sound_a0_descriptor_init(slots,sizeof(slots)-1u,UINT32_C(0x60b4ff)) == -1);
    CHECK(memcmp(slots,before,sizeof(slots)) == 0);
    CHECK(vf2_sound_a0_descriptor_init(NULL,sizeof(slots),UINT32_C(0x60b4ff)) == -1);
    if (failures == 0) {
        puts("A0 descriptor native: 12 ROM-pointer cases + refusals PASS");
    }
    return failures == 0 ? 0 : 1;
}
