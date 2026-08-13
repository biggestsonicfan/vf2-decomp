static uint8_t player_2760c_shift_from_bucket(uint16_t bucket)
{
    if (bucket == UINT16_C(8)) {
        return UINT8_C(3);
    }
    if (bucket == UINT16_C(4)) {
        return UINT8_C(2);
    }
    return UINT8_C(1);
}
