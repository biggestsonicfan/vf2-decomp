typedef struct vf2_player_2767c_inputs {
    int32_t first[3];
    int32_t second[3];
    int32_t third[3];
} vf2_player_2767c_inputs;

static int32_t player_2767c_signed_low16(uint32_t word)
{
    return (int32_t)(int16_t)(uint16_t)word;
}

static vf2_status player_2767c_build_inputs(
    const uint32_t active[3],
    const uint32_t backup[3],
    const uint16_t coefficients[3],
    vf2_player_2767c_inputs *inputs
)
{
    if (active == NULL || backup == NULL || coefficients == NULL ||
        inputs == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    inputs->first[0] = -player_2767c_signed_low16(active[0]);
    inputs->first[1] = -player_2767c_signed_low16(active[1]);
    inputs->first[2] = -player_2767c_signed_low16(active[2]);

    inputs->second[0] = player_2767c_signed_low16(backup[2]);
    inputs->second[1] = player_2767c_signed_low16(backup[1]);
    inputs->second[2] = player_2767c_signed_low16(backup[0]);

    inputs->third[0] = (int32_t)(int16_t)coefficients[1];
    inputs->third[1] = (int32_t)(int16_t)coefficients[0];
    inputs->third[2] = (int32_t)(int16_t)coefficients[2];
    return VF2_OK;
}
