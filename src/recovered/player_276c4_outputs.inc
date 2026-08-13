static vf2_status player_276c4_decode_outputs(
    const int16_t outputs[3],
    uint8_t shift,
    uint16_t xyz[3]
)
{
    int32_t x = 0;
    int32_t y = 0;
    int32_t z = 0;

    if (outputs == NULL || xyz == NULL || shift >= UINT8_C(32)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    x = (int32_t)outputs[1];
    y = -(int32_t)outputs[0];
    z = -(int32_t)outputs[2];
    x >>= shift;
    y >>= shift;
    z >>= shift;
    xyz[0] = (uint16_t)x;
    xyz[1] = (uint16_t)y;
    xyz[2] = (uint16_t)z;
    return VF2_OK;
}
