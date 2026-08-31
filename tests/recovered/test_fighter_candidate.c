#include "vf2/fighter_candidate.h"

#include <assert.h>
#include <string.h>

/*
 * ROM-independent regression for the provisional fighter layout.
 * The header already contains _Static_assert for each offset, but this
 * translation unit ensures the header is included in the normal build
 * and that the window constant and accessor helper behave as documented
 * in decomp/i960/notes/fighter_candidate_layout_v0201.md.
 */

static void test_offsets(void)
{
    assert(VF2_FIGHTER_CANDIDATE_WINDOW == 0x2000u);
    assert(VF2_FIGHTER_OFF_01A4 == 0x01a4u);
    assert(VF2_FIGHTER_OFF_05B4 == 0x05b4u);
    assert(VF2_FIGHTER_OFF_05B8 == 0x05b8u);
    assert(VF2_FIGHTER_OFF_1200 == 0x1200u);

    assert(sizeof(struct vf2_fighter_candidate) == VF2_FIGHTER_CANDIDATE_WINDOW);
    assert(offsetof(struct vf2_fighter_candidate, field_01a4) == 0x01a4u);
    assert(offsetof(struct vf2_fighter_candidate, field_05b4) == 0x05b4u);
    assert(offsetof(struct vf2_fighter_candidate, field_05b8) == 0x05b8u);
    assert(offsetof(struct vf2_fighter_candidate, field_1200) == 0x1200u);

    assert(vf2_fighter_candidate_offset_valid(0x01a4u, 4u));
    assert(vf2_fighter_candidate_offset_valid(0x05b4u, 2u));
    assert(!vf2_fighter_candidate_offset_valid(0x2000u, 1u));
    assert(!vf2_fighter_candidate_offset_valid(0x1fffu, 2u));
}

static void test_padding_zero(void)
{
    struct vf2_fighter_candidate f;
    memset(&f, 0xAA, sizeof(f));
    /* The padding bytes are part of the window; writing via field
       accessors must not corrupt adjacent stable fields. */
    f.field_01a4 = 0x11223344u;
    f.field_05b4 = 0x5566u;
    f.field_05b8 = 0x778899AAu;
    assert(f.field_01a4 == 0x11223344u);
    assert(f.field_05b4 == 0x5566u);
    assert(f.field_05b8 == 0x778899AAu);
}

int main(void)
{
    test_offsets();
    test_padding_zero();
    return 0;
}
