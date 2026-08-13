typedef struct vf2_player_2779c_selection {
    uint8_t accepted;
    uint16_t scratch_offset;
} vf2_player_2779c_selection;

static vf2_player_2779c_selection player_2779c_select_block(uint8_t selector_byte)
{
    vf2_player_2779c_selection result = {UINT8_C(1), UINT16_C(0)};

    switch (selector_byte) {
    case UINT8_C(0x00):
        result.scratch_offset = UINT16_C(0x01e0);
        break;
    case UINT8_C(0x80):
        result.scratch_offset = UINT16_C(0x02d0);
        break;
    case UINT8_C(0x03):
        result.scratch_offset = UINT16_C(0x03c0);
        break;
    case UINT8_C(0x83):
        result.scratch_offset = UINT16_C(0x04b0);
        break;
    case UINT8_C(0x04):
        result.scratch_offset = UINT16_C(0x05a0);
        break;
    default:
        result.accepted = UINT8_C(0);
        break;
    }
    return result;
}
