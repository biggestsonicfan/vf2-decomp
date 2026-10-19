#include "../../src/hardware/sound_l2_ad.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int failures;
#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL %u: %s\n", (unsigned)__LINE__, #condition); \
    ++failures; }} while (0)

int main(void)
{
    static uint8_t rom[0xc000u];
    uint8_t packets[6] = {0u};
    int family;
    int index;
    for (family = 0; family != 2; ++family) {
        for (index = 0; index < 128; ++index) {
            uint8_t *p = rom + (family ? 0xb7e6u : 0xb1e6u);
            p[0] = 0x8fu;
            p[1] = (uint8_t)(family ? 0xbau : 0xbbu);
            p[2] = 0x21u;
            p[3] = (uint8_t)(family ? 7u : 6u);
            p[4] = 0u;
            p[5] = (uint8_t)(family ? 0x9au : 0x9bu);
            p[6] = (uint8_t)index;
            p[7] = (uint8_t)(family || index >= 0x44 ? 0x7f : 0x78);
            p[8] = 0u; p[9] = 0xffu; p[10] = 0x2fu; p[11] = 0u;
            CHECK(vf2_sound_l2_ad_expand(rom, sizeof(rom),
                  (uint32_t)(family ? 0x60b7e7u : 0x60b1e7u), packets));
            CHECK(packets[0] == p[1] && packets[1] == p[2] &&
                  packets[2] == p[3] && packets[3] == p[5] &&
                  packets[4] == p[6] && packets[5] == p[7]);
        }
    }
    {
        uint8_t *p = rom + 0xb7e6u;
        p[6] = 0x70u;  /* Original 0xAD247C alias is not normalized. */
        CHECK(vf2_sound_l2_ad_expand(rom, sizeof(rom), 0x60b7e7u, packets));
        CHECK(packets[4] == 0x70u);
        p[9] = 0xfeu;
        CHECK(!vf2_sound_l2_ad_expand(rom, sizeof(rom), 0x60b7e7u, packets));
        p[9] = 0xffu;
        CHECK(!vf2_sound_l2_ad_expand(rom, 11u, 0x60b7e7u, packets));
        CHECK(!vf2_sound_l2_ad_expand(rom, sizeof(rom), 0x600000u, packets));
        CHECK(!vf2_sound_l2_ad_expand(NULL, sizeof(rom), 0x60b7e7u, packets));
        CHECK(!vf2_sound_l2_ad_expand(rom, sizeof(rom), 0x60b7e7u, NULL));
        CHECK(!vf2_sound_l2_ad_expand(rom, sizeof(rom), 0x60b7e8u, packets));
    }
    if (failures == 0) {
        puts("L2 AD: 256 records, index alias, malformed/unaligned refusal PASS");
    }
    return failures != 0;
}
